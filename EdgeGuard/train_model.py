# ============================================================
# EdgeGuard 模型训练脚本 v1.1
# 功能：读取 dataset/X.npy、y.npy、scaler_mean.npy、scaler_std.npy
#       训练决策树 -> 测试精度 -> 导出 C 数组（树 + scaler）
# 输出：model/tree_data.c（给 STM32 用的 C 源文件）
# ============================================================

import os
import numpy as np

from sklearn.tree import DecisionTreeClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, accuracy_score

# ---------------- 配置区 ----------------
DATASET_DIR = "dataset"
MODEL_DIR   = "model"
MAX_DEPTH   = 5
# ---------------------------------------

def export_tree_to_c(tree, scaler_mean, scaler_std, filename):
    """
    把决策树 + scaler 参数一起导出成 C 数组
    推理时公式：scaled[i] = (raw[i] - scaler_mean[i]) / scaler_std[i]
    然后用 scaled 后的特征和树的 threshold 比大小
    """
    t = tree.tree_
    n_nodes = t.node_count
    n_features = len(scaler_mean)

    lines = []
    lines.append("// ============================================================")
    lines.append("// EdgeGuard 决策树模型（由 train_model.py 自动生成）")
    lines.append("// 推理流程：")
    lines.append("//   1. 对原始特征做标准化：scaled[i] = (raw[i] - scaler_mean[i]) / scaler_std[i]")
    lines.append("//   2. 从节点0开始遍历树：if scaled[feature] > threshold 走右边，否则走左边")
    lines.append("//   3. 到叶子节点(feature==-2)时，leaf_class 就是结果 (0=normal, 1=abnormal)")
    lines.append("// ============================================================")

    # ---------- 宏定义 ----------
    lines.append(f"#define TREE_NODE_COUNT {n_nodes}")
    lines.append(f"#define FEATURE_COUNT   {n_features}")
    lines.append("")

    # ---------- 树结构 5 个数组 ----------
    lines.append(f"// 每个节点用哪个特征做判断 (-2 = 叶子节点)")
    lines.append(f"const int tree_feature[{n_nodes}] = {{")
    lines.append("    " + ", ".join(str(int(x)) for x in t.feature))
    lines.append("};")
    lines.append("")

    lines.append(f"// 每个节点的判断阈值（标准化后的特征值）")
    lines.append(f"const float tree_threshold[{n_nodes}] = {{")
    lines.append("    " + ", ".join(f"{x:.6f}f" for x in t.threshold) + ",")
    lines.append("};")
    lines.append("")

    lines.append(f"// 每个节点的左孩子编号 (-1 = 没有)")
    lines.append(f"const int tree_children_left[{n_nodes}] = {{")
    lines.append("    " + ", ".join(str(int(x)) for x in t.children_left) + ",")
    lines.append("};")
    lines.append("")

    lines.append(f"// 每个节点的右孩子编号 (-1 = 没有)")
    lines.append(f"const int tree_children_right[{n_nodes}] = {{")
    lines.append("    " + ", ".join(str(int(x)) for x in t.children_right) + ",")
    lines.append("};")
    lines.append("")

    # 叶子节点投票结果
    leaf_class = []
    for i in range(n_nodes):
        if t.feature[i] == -2:
            votes = t.value[i][0]
            leaf_class.append(1 if votes[1] >= votes[0] else 0)
        else:
            leaf_class.append(0)
    lines.append(f"// 叶子节点的最终分类 (0=normal, 1=abnormal)；非叶子节点填0占位")
    lines.append(f"const unsigned char tree_leaf_class[{n_nodes}] = {{")
    lines.append("    " + ", ".join(str(c) for c in leaf_class) + ",")
    lines.append("};")
    lines.append("")

    # ---------- 新增：scaler 2 个数组 ----------
    lines.append(f"// 特征标准化参数（训练时拟合）")
    lines.append(f"// 推理公式：scaled[i] = (raw_feature[i] - scaler_mean[i]) / scaler_std[i]")
    lines.append(f"const float scaler_mean[{n_features}] = {{")
    lines.append("    " + ", ".join(f"{x:.6f}f" for x in scaler_mean) + ",")
    lines.append("};")
    lines.append("")

    lines.append(f"const float scaler_std[{n_features}] = {{")
    lines.append("    " + ", ".join(f"{x:.6f}f" for x in scaler_std) + ",")
    lines.append("};")

    with open(filename, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"[已导出] {filename}")

def main():
    print("=" * 50)
    print("EdgeGuard 决策树训练")
    print("=" * 50)

    # ---------- 1. 载入数据集 ----------
    X = np.load(os.path.join(DATASET_DIR, "X.npy"))
    y = np.load(os.path.join(DATASET_DIR, "y.npy"))
    # 同时载入 scaler（是 feature_extract.py 在标准化时存的）
    scaler_mean = np.load(os.path.join(DATASET_DIR, "scaler_mean.npy"))
    scaler_std  = np.load(os.path.join(DATASET_DIR, "scaler_std.npy"))
    print(f"[数据]      X: {X.shape}, y: {y.shape}")
    print(f"[scaler]    mean/std 各 {len(scaler_mean)} 个")

    # ---------- 2. 切训练集/测试集 ----------
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.3, stratify=y, random_state=42
    )
    print(f"[切分]      训练集 {len(y_train)} / 测试集 {len(y_test)}")

    # ---------- 3. 训练 ----------
    clf = DecisionTreeClassifier(max_depth=MAX_DEPTH, random_state=42)
    clf.fit(X_train, y_train)
    print(f"[训练完成]  树节点数: {clf.tree_.node_count}")

    # ---------- 4. 考试 ----------
    y_pred = clf.predict(X_test)
    acc = accuracy_score(y_test, y_pred)
    print(f"\n[测试精度]  {acc*100:.1f}%")
    print(classification_report(y_test, y_pred,
          target_names=["normal", "abnormal"]))

    # ---------- 5. 导出 C 数组（树 + scaler）----------
    os.makedirs(MODEL_DIR, exist_ok=True)
    export_tree_to_c(clf, scaler_mean, scaler_std,
                     os.path.join(MODEL_DIR, "tree_data.c"))

    print("\n✅ 全部完成！下一步：把 model/tree_data.c 拷进 Keil，写 C 推理函数")

if __name__ == "__main__":
    main()