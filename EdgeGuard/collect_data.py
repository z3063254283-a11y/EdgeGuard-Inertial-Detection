# d:\EdgeGuard\collect_data.py
# ============================================================
# EdgeGuard 数据采集脚本 v1.0
# 功能：从串口(COM6, 115200)读取 B 板 MPU6050 数据
#       识别 "=== COLLECT START/STOP ===" 标记
#       将数据自动保存为 CSV 文件
# 用法：python collect_data.py
# 退出：按 Ctrl+C
# ============================================================

import serial                # 串口库（pyserial，注意安装名和导入名不同）
import serial.tools.list_ports  # 串口枚举工具
import csv                   # CSV 文件读写库（Python 自带，不用安装）
import os                    # 文件夹/路径操作库（Python 自带）
from datetime import datetime  # 时间库，用于生成带时间戳的文件名

# ---------------- 配置区（要改参数只改这里） ----------------
SERIAL_PORT = "COM6"         # 你的 CH340 串口号（设备管理器里查到的）
BAUDRATE    = 115200         # 波特率，必须和 B 板 MyUART_Init(115200) 一致
DATA_DIR    = "data"         # 数据总文件夹（脚本自动创建）
# -------------------------------------------------------------

def choose_label():
    """询问用户本次采集的数据类型，返回子文件夹名（normal/abnormal）"""
    print("=" * 50)
    print("EdgeGuard 数据采集工具")
    print("=" * 50)
    print("本次采集哪种数据？")
    print("  1. normal   —— 正常状态（板子平稳放置/轻微移动）")
    print("  2. abnormal —— 异常状态（猛烈晃动/倾斜/跌落模拟）")
    
    choice = input("请输入 1 或 2，回车确认: ").strip()
    
    if choice == "2":
        return "abnormal"
    else:
        return "normal"

def open_csv_file(label):
    """
    创建并打开一个新的 CSV 文件，返回文件对象和 csv 写入器。
    文件名格式: normal_20260908_153012.csv（类型_日期_时间）
    """
    folder = os.path.join(DATA_DIR, label)
    os.makedirs(folder, exist_ok=True)
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = os.path.join(folder, f"{label}_{timestamp}.csv")
    
    f = open(filename, "w", newline="", encoding="utf-8")
    writer = csv.writer(f)
    
    writer.writerow(["ax", "ay", "az", "gx", "gy", "gz"])
    
    print(f"[已创建] {filename}")
    return f, writer

def main():
    label = choose_label()
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.1)
    except serial.SerialException as e:
        print(f"[错误] 无法打开 {SERIAL_PORT}: {e}")
        print("请检查: 1) 板子是否插好  2) 串口号是否正确  3) 串口助手是否已关闭")
        return
    
    print(f"[串口已打开] {SERIAL_PORT} @ {BAUDRATE} bps")
    print("等待 B 板信号... 在板子上按 PB11 键开始/结束采集，Ctrl+C 退出脚本")
    
    collecting = False
    f = None
    writer = None
    row_count = 0
    buffer = ""
    
    try:
        while True:
            chunk = ser.read(4096)
            if not chunk:
                continue
            
            buffer += chunk.decode("ascii", errors="ignore")
            
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                
                if "COLLECT START" in line:
                    f, writer = open_csv_file(label)
                    collecting = True
                    row_count = 0
                    print("[采集开始]")
                    
                elif "COLLECT STOP" in line:
                    if collecting and f:
                        f.close()
                        print(f"[采集结束] 共保存 {row_count} 行数据")
                    collecting = False
                    f = None
                    writer = None
                    
                elif collecting and writer is not None:
                    try:
                        parts = line.split(",")
                        if len(parts) == 6:
                            row = [int(p) for p in parts]
                            writer.writerow(row)
                            row_count += 1
                            if row_count % 100 == 0:
                                print(f"  ... 已保存 {row_count} 行")
                    except ValueError:
                        pass
                    
    except KeyboardInterrupt:
        print("\n[手动退出]")
        if collecting and f:
            f.close()
            print(f"[提示] 最后一次采集保存了 {row_count} 行")
    finally:
        ser.close()
        print("[串口已关闭] 脚本结束")

if __name__ == "__main__":
    main()