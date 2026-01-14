import argparse
import json
import time
from collections import defaultdict
from dataclasses import dataclass
from typing import Dict, List

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial

@dataclass
class DataPoint:
    t: float
    h: float
    p: float
    timestamp: float

device_data: Dict[str, List[DataPoint]] = defaultdict(list)
MAX_HISTORY_SECONDS = 600

# グラフの設定：サブプロットを1つだけ作成
fig, ax = plt.subplots(figsize=(10, 6))
fig.canvas.manager.set_window_title('Sensor Multi-Data Monitor')

def update_graph(frame, ser):
    global device_data

    # シリアルデータの読み取り
    while ser.in_waiting > 0:
        line = ser.readline()
        if not line: continue
        try:
            decoded = line.decode("utf-8", errors="ignore").strip()
            d = json.loads(decoded)
            if d.get("type") == "sample":
                device_id = d["id"]
                now = time.time()
                point = DataPoint(t=float(d["t"]), h=float(d["h"]), p=float(d["p"]), timestamp=now)
                device_data[device_id].append(point)
                # 10分以前のデータを削除
                device_data[device_id] = [p for p in device_data[device_id] if now - p.timestamp <= MAX_HISTORY_SECONDS]
        except:
            pass

    # 描画の更新
    ax.clear()
    ax.set_title("Environmental Data Over Time", fontsize=12)
    ax.set_xlabel("Time (minutes ago)")
    ax.set_ylabel("Value (T, H, P)")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(-10, 0)

    now = time.time()
    
    # 全デバイスのデータを1つのグラフにプロット
    for device_id, data_list in device_data.items():
        if not data_list: continue
        
        rel_times = [(p.timestamp - now) / 60.0 for p in data_list]
        
        # 指標ごとに線種やマーカーを変えて区別（色はデバイスごとに変えるなど工夫が可能）
        # ここでは「デバイスID_指標名」で凡例を作成
        ax.plot(rel_times, [p.t for p in data_list], label=f"{device_id} Temp(°C)", linestyle='-')
        ax.plot(rel_times, [p.h for p in data_list], label=f"{device_id} Hum(%)", linestyle='--')
        ax.plot(rel_times, [p.p for p in data_list], label=f"{device_id} Pres(hPa)", linestyle=':')

    # 凡例をグラフの外側（右側）に表示
    if device_data:
        ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.01)
        print(f"Port {args.port} に接続しました。")
    except Exception as e:
        print(f"Error: {e}"); return

    ani = animation.FuncAnimation(fig, update_graph, fargs=(ser,), interval=1000, cache_frame_data=False)
    plt.tight_layout(rect=[0, 0, 0.85, 1]) # 凡例のスペースを確保
    plt.show()
    ser.close()

if __name__ == "__main__":
    main()