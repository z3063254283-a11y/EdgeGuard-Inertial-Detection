// ============================================================
// EdgeGuard ML 推理模块
// 功能：环形缓冲 → 特征提取 → 标准化 → 决策树推理 → 启发式分类
// ============================================================
#ifndef __ML_MODEL_H
#define __ML_MODEL_H

#include "stm32f10x.h"

// ---------------- 常量定义（必须和 Python 训练时一致）----------------
#define ML_WINDOW_SIZE   20    // 窗口大小（20帧）
#define ML_STRIDE        5     // 步长（每5帧推一次，和 feature_extract.py 里的 STRIDE 一致）
#define ML_SENSOR_COUNT  6     // ax, ay, az, gx, gy, gz
#define ML_FEATURE_COUNT 18    // 6列 × 3种统计量

// ---------------- 异常子类型枚举 ----------------
typedef enum {
    SUBTYPE_NORMAL = 0,
    SUBTYPE_SHAKE,       // 震动/晃动
    SUBTYPE_IMPACT,      // 冲击/跌落
    SUBTYPE_TILT         // 倾斜
} MlSubtype_t;

// ---------------- 对外函数 ----------------
// 初始化（环形缓冲清零）
void ml_init(void);

// 塞入一帧 MPU6050 原始数据（Task_MPU6050 每 50ms 调一次）
void ml_push_frame(int32_t ax, int32_t ay, int32_t az,
                   int32_t gx, int32_t gy, int32_t gz);

// 缓冲满了吗？（>=20 帧才算满）
uint8_t ml_is_buffer_full(void);

// 是不是到了该推理的时刻？（每 5 帧推一次，和 stride=5 对应）
uint8_t ml_should_predict(void);

// 完整推理流程：算特征 → 标准化 → 树推理 → 启发式分类
// 返回：0=normal, 1=abnormal
// *subtype 输出：异常子类型（仅 abnormal 时有效）
uint8_t ml_run_full_inference(MlSubtype_t *subtype);

#endif
