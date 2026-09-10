#include "ml_model.h"
#include <math.h>
#include "MyUART.h"

extern const int	tree_feature[];
extern const float  tree_threshold[];
extern const int    tree_children_left[];
extern const int    tree_children_right[];
extern const unsigned char tree_leaf_class[];
extern const float  scaler_mean[];
extern const float  scaler_std[];

static int32_t ml_buf[ML_WINDOW_SIZE][ML_SENSOR_COUNT];//环形缓冲本体，存 20 帧 × 6 通道原始数据
static uint8_t ml_write_idx = 0;//下一个要写入的位置(写指针)
static uint8_t ml_buf_count = 0;//缓冲里现在有多少帧数据
static uint8_t ml_predict_counter = 0;//内部计数器，数到 5 就触发推理
static uint8_t ml_tree_strike = 0;//树的连续"异常"计数（去抖用，防止单帧误报）

void ml_init(void)
{
	for(int i = 0; i < ML_WINDOW_SIZE; i++)
	{
		for(int j = 0; j < ML_SENSOR_COUNT; j++)
			ml_buf[i][j] = 0;
	}
	ml_write_idx = 0;
	ml_buf_count = 0;
	ml_predict_counter = 0;
	ml_tree_strike = 0;
}

void ml_push_frame(int32_t ax, int32_t ay, int32_t az,
					int32_t gx, int32_t gy, int32_t gz)
{
	ml_buf[ml_write_idx][0] = ax;
	ml_buf[ml_write_idx][1] = ay;
	ml_buf[ml_write_idx][2] = az;
	ml_buf[ml_write_idx][3] = gx;
	ml_buf[ml_write_idx][4] = gy;
	ml_buf[ml_write_idx][5] = gz;
	
	ml_write_idx = (ml_write_idx + 1) % ML_WINDOW_SIZE;
	if(ml_buf_count < ML_WINDOW_SIZE)
		ml_buf_count ++;
}

uint8_t ml_is_buffer_full(void)
{
	return (ml_buf_count >= ML_WINDOW_SIZE) ? 1:0;
}

uint8_t ml_should_predict(void)
{
	if(!ml_is_buffer_full())	return 0;
	ml_predict_counter++;
	if(ml_predict_counter >= ML_STRIDE)
	{
		ml_predict_counter = 0;
		return 1;
	}
	return 0;
}

static void extract_features(int32_t *raw_mean, int32_t *raw_var, int32_t *raw_p2p)//内部调用
{
	for(uint8_t s = 0; s < ML_SENSOR_COUNT; s++)
	{
		int64_t sum = 0;
		for(uint8_t k = 0; k < ML_WINDOW_SIZE; k++)
			sum += ml_buf[k][s];
		raw_mean[s] = (int32_t)(sum / ML_WINDOW_SIZE);//平均数
		
		int64_t var_sum = 0;
		int32_t mean_s = raw_mean[s];
		for(uint8_t k = 0; k<ML_WINDOW_SIZE; k++)
		{
			int64_t diff = (int64_t)ml_buf[k][s] - mean_s;
			var_sum += diff *diff;
		}
		raw_var[s] = (int32_t)(var_sum / ML_WINDOW_SIZE);//方差
		
		int32_t max_val = ml_buf[0][s];
		int32_t min_val = ml_buf[0][s];
		for(uint8_t k =1;k<ML_WINDOW_SIZE;k++)
		{
			if(ml_buf[k][s] > max_val)	max_val = ml_buf[k][s];
			if(ml_buf[k][s] < min_val)	min_val = ml_buf[k][s];
		}
		raw_p2p[s] = max_val - min_val;//峰峰值
	}
}

 static void standardize(int32_t *raw_mean, int32_t *raw_var, int32_t *raw_p2p, float *scaled)
{
	float raw_feature[ML_FEATURE_COUNT];
	 
	 for(int i = 0; i<ML_SENSOR_COUNT;i++)
	 {
		raw_feature[i] =(float)raw_mean[i];
		raw_feature[i+6] = (float)raw_var[i];
		raw_feature[i+12] = (float)raw_p2p[i];
	 }
	 
	 for(int i = 0;i< ML_FEATURE_COUNT;i++)
	 {
		if(fabsf(scaler_std[i]) < 1e-6f)//检查除数是不是接近 0。fabsf 是 <math.h> 里的"单精度浮点数绝对值"。如果 std ≈ 0，说明这个特征在训练数据里几乎没波动，直接给 0 就行。为什么要检查？ 任何数除以 0 都是 undefined behavior（要么程序崩，要么得到乱码数），嵌入式里这种防御性检查必须做。
			scaled[i] = 0.0f;
		else
			scaled[i] = (raw_feature[i]-scaler_mean[i])/scaler_std[i];
	 }
}
 
 static uint8_t tree_predict(float *scaled)
{
	int node = 0;
	while(tree_feature[node] != -2)
	{
		int idx = tree_feature[node];
		float val = scaled[idx];
		if(val > tree_threshold[node])
			node = tree_children_right[node];
		else
			node = tree_children_left[node];
	}
	return tree_leaf_class[node];
}
 
// ---- 启发式分类的可调阈值（依据 2026-09-10 实测数据校准）----
// 实测基准（平放静止）：ax≈+180, ay≈-240, az≈18700, gyvar≈10
// 倾斜 30°（绕 X 轴滚转）：ay≈+6400，gyvar 最大≈26万
// 猛晃 / 自由落体：gyvar 都 > 65万，p2p_az 都 ≈ 3万（两者统计上分不开）
#define TILT_H_LIMIT       4000   // 水平分量 sqrt(ax²+ay²) 超过它＝倾斜（平放≈300）
#define STATIC_VAR_LIMIT   500000 // 角速度方差在 50万~3000万 之间＝中轻度震动
#define SHAKE_VAR_HARD     30000000 // 角速度方差 >3000万＝一定猛晃/跌落（最快倾斜才800万）
#define TREE_CONFIRM_COUNT 3      // 树连续 3 帧判异常才升级（约 0.75 秒，去抖）
#define RECENT_WINDOW      6      // 倾斜"快速通道"：用最近 6 帧(0.3秒)算水平分量
#define GY_MEAN_TILT_LIMIT 500    // 最近6帧陀螺仪均值|超过它|＝单向净转动(倾斜过程)

// 取"最近 RECENT_WINDOW 帧"里某一通道的平均值。
// axis = 通道列号：0=ax,1=ay,2=az,3=gx,4=gy,5=gz。
// 最近几帧 = 环形缓冲从写指针往前数 N 帧。
static int32_t recent_mean(uint8_t axis)
{
    int64_t sum = 0;
    uint8_t n = RECENT_WINDOW;
    if (n > ml_buf_count) n = ml_buf_count;   // 防御：缓冲未满时只取已有帧
    for (uint8_t k = 1; k <= n; k++)
    {
        uint8_t idx = (uint8_t)((ml_write_idx - k + ML_WINDOW_SIZE) % ML_WINDOW_SIZE);
        sum += ml_buf[idx][axis];
    }
    return (int32_t)(sum / n);
}

// 最近几帧 ax/ay 均值的水平分量平方 h2（判断"已经斜到位"）。
static int64_t recent_h2(void)
{
    int32_t ax = recent_mean(0);
    int32_t ay = recent_mean(1);
    return (int64_t)ax * ax + (int64_t)ay * ay;
}

static MlSubtype_t heuristic_classify(int32_t *raw_mean, int32_t *raw_var, int32_t *raw_p2p) {
    // 判定顺序：猛晃 > 倾斜 > 中轻震动 > 正常
    //
    // 为什么要先判"猛晃"？最近实测发现一个关键断层：
    //   最快倾斜：gyvar(角速度方差) 最高 ≈800万
    //   猛晃/跌落：gyvar 最低 ≈1.3亿
    // 中间差 15 倍以上。而"猛晃"是来回高速振荡，最近 6 帧陀螺仪均值也会
    // 偶然偏大、被误判成倾斜。所以先用这个巨大方差把猛晃一票否决，最稳妥。

    (void)raw_p2p;   // 峰峰值本轮不参与分类，显式忽略，避免编译器警告
    (void)raw_mean;  // 全窗口均值不用于倾斜判定，改用下面的 recent_h2()

    int64_t h2_now = recent_h2();   // 最近6帧 ax/ay 的水平分量平方
    int32_t rgx = recent_mean(3);   // 最近6帧 陀螺仪X(滚转) 均值
    int32_t rgy = recent_mean(4);   // 最近6帧 陀螺仪Y(俯仰) 均值
    int64_t h2_limit = (int64_t)TILT_H_LIMIT * TILT_H_LIMIT;

    // ---- 1. 猛晃/跌落：角速度方差巨大，直接判 SHAKE ----
    if (raw_var[4] > SHAKE_VAR_HARD)
        return SUBTYPE_SHAKE;

    // ---- 2. 倾斜 TILT：两个条件满足其一即可 ----
    // 条件A（已经斜到位）：最近几帧水平分量 h2 大 → 快/慢倾斜稳定后都判 TILT。
    // 条件B（正在单向转动）：最近几帧陀螺仪均值有明显方向性（gx 或 gy 持续偏一侧）
    //     → 倾斜"过程中"立刻判 TILT，不用等角度攒够。
    if (h2_now > h2_limit ||
        rgx > GY_MEAN_TILT_LIMIT || rgx < -GY_MEAN_TILT_LIMIT ||
        rgy > GY_MEAN_TILT_LIMIT || rgy < -GY_MEAN_TILT_LIMIT)
        return SUBTYPE_TILT;

    // ---- 3. 中轻度震动：角速度方差较大（50万~3000万，没到猛晃级别）----
    if (raw_var[4] > STATIC_VAR_LIMIT || raw_var[3] > STATIC_VAR_LIMIT)
        return SUBTYPE_SHAKE;

    // ---- 4. 正常 ----
    return SUBTYPE_NORMAL;
}
 
uint8_t ml_run_full_inference(MlSubtype_t *subtype) {
    int32_t raw_mean[ML_SENSOR_COUNT];
    int32_t raw_var[ML_SENSOR_COUNT];
    int32_t raw_p2p[ML_SENSOR_COUNT];
    float   scaled[ML_FEATURE_COUNT];

    extract_features(raw_mean, raw_var, raw_p2p);
    standardize(raw_mean, raw_var, raw_p2p, scaled);

    // ======= 1. 启发式先跑一次 =======
    *subtype = heuristic_classify(raw_mean, raw_var, raw_p2p);

    // ======= 2. 跑决策树 =======
    uint8_t tree_result = tree_predict(scaled);

    // ======= 3. 合并判断 =======
	// 为什么不用"启发式 OR 树"直接合并？
    // 实测：现在决策树只有 3 个节点（=只做一次分裂，只看 gx 方差），训练数据少、
    // 阈值偏敏感，在"倾斜回正"那一帧 gx 方差还没掉下来时，树会单帧误报。
    // 而启发式规则是用真实数据标定的，四个场景全对。
    // 所以：启发式判异常 → 直接信；启发式判正常 → 树要"连续 N 帧"都判异常才升级。
    uint8_t result;
    if (*subtype != SUBTYPE_NORMAL) {
        result = 1;                 // 启发式（强规则）已判异常，直接报
        ml_tree_strike = 0;         // 树的连续计数清零
    } else {
        if (tree_result == 1) {
            ml_tree_strike++;       // 树在正常态里继续报警，计数 +1
        } else {
            ml_tree_strike = 0;     // 树说正常，就清空计数
        }
        // 只有连续 TREE_CONFIRM_COUNT 帧都异常，才相信树（保留 AI 的兜底能力）
        result = (ml_tree_strike >= TREE_CONFIRM_COUNT) ? 1 : 0;
    }

    // ======= 4. 调试打印（别删！跑一次看完真实数值再决定调不调）=======
    // rh2=最近6帧水平分量², rgx/rgy=最近6帧陀螺仪X/Y均值(看单向净转动)
    printf("[ML] mean=%d,%d,%d  gyvar=%d gxvar=%d  rh2=%d  rgx=%d  rgy=%d  tree=%d  sub=%d  result=%d\r\n",
        raw_mean[0], raw_mean[1], raw_mean[2],
        raw_var[4], raw_var[3],
        (int)recent_h2(), recent_mean(3), recent_mean(4),
        tree_result, *subtype, result);

    return result;
}
