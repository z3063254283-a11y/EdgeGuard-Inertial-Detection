# ============================================================
# EdgeGuard 特征提取脚本 v1.0
# 功能：读取 data/normal 和 data/abnormal 下的所有CSV
#       滑动窗口切片 -> 提取统计特征 -> 标准化 -> 保存数据集
# 输出：dataset/X.npy、dataset/y.npy、scaler参数
# ============================================================

import os
import glob
import numpy as np
from sklearn.preprocessing import StandardScaler

# ---------------- 配置区 ----------------
DATA_DIR = "data"
OUTPUT_DIR = "dataset"
WINDOW_SIZE = 20         # 每个窗口含20帧
STRIDE = 5               # 相邻窗口隔5帧（重叠15帧）
# ---------------------------------------

def load_all_csv(label):
    """加载某类别下所有CSV，拼成一个大数组 [总行数, 6]"""
    csv_files = glob.glob(os.path.join(DATA_DIR, label, "*.csv"))
    print(f"[加载] {label} 类别找到 {len(csv_files)} 个CSV文件")

    all_data = []
    for fpath in csv_files:
        data = np.loadtxt(fpath, delimiter=",", skiprows=1, dtype=np.int32)
        if len(data) > 0:
            all_data.append(data)

    if len(all_data) == 0:
        return np.array([]).reshape(0, 6)   # 空数组占位，防止后面崩
    return np.vstack(all_data)

def extract_features_window(window):
    """对单个窗口[20,6]提取特征，输出18维"""
    mean = np.mean(window, axis=0)      # 6个均值：看姿态
    var  = np.var(window, axis=0)       # 6个方差：看抖动
    p2p  = np.ptp(window, axis=0)       # 6个峰峰值：看冲击
    return np.concatenate([mean, var, p2p])   # 拼成18维

def generate_dataset(raw_data, label_id):
    """把原始数据切窗口提特征，返回 X[窗口数,18] 和 y[窗口数]"""
    X_list = []
    y_list = []
    if len(raw_data) == 0:
        return np.array([]).reshape(0, 18), np.array([], dtype=int)

    for start in range(0, len(raw_data) - WINDOW_SIZE + 1, STRIDE):
        window = raw_data[start : start + WINDOW_SIZE]
        X_list.append(extract_features_window(window))
        y_list.append(label_id)

    return np.array(X_list), np.array(y_list)

def main():
    print("=" * 50)
    print("EdgeGuard 特征提取工具")
    print("=" * 50)

    normal_raw   = load_all_csv("normal")
    abnormal_raw = load_all_csv("abnormal")

    print(f"\n[原始数据] normal: {normal_raw.shape}")
    print(f"[原始数据] abnormal: {abnormal_raw.shape}")

    print("\n[特征提取中...]")
    X_norm, y_norm = generate_dataset(normal_raw, label_id=0)
    X_abn,  y_abn  = generate_dataset(abnormal_raw, label_id=1)

    if len(y_norm) == 0 or len(y_abn) == 0:
        print("\n[错误] normal 或 abnormal 没有数据！")
        print("  → 检查 data\\normal\\ 和 data\\abnormal\\ 里是否有CSV文件")
        print("  → abnormal 还没采的话，先去跑 collect_data.py 选 2 采集")
        return

    X = np.vstack([X_norm, X_abn])
    y = np.concatenate([y_norm, y_abn])

    print(f"[数据集] 总窗口数: {len(y)}")
    print(f"  -> normal窗口: {sum(y==0)}, abnormal窗口: {sum(y==1)}")
    print(f"  -> 每个窗口特征数: {X.shape[1]}")

    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    np.save(os.path.join(OUTPUT_DIR, "X.npy"), X_scaled)
    np.save(os.path.join(OUTPUT_DIR, "y.npy"), y)
    np.save(os.path.join(OUTPUT_DIR, "scaler_mean.npy"), scaler.mean_)
    np.save(os.path.join(OUTPUT_DIR, "scaler_std.npy"), scaler.scale_)

    print(f"\n[已保存] 数据集存到 {OUTPUT_DIR}/ 文件夹")
    print("  -> X.npy: 特征矩阵 [窗口数 x 18]")
    print("  -> y.npy: 标签数组（0=normal, 1=abnormal）")
    print("  -> scaler_mean.npy / scaler_std.npy: 标准化参数")

if __name__ == "__main__":
    main()