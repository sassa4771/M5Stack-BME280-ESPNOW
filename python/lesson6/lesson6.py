import argparse
import json
import time
from dataclasses import dataclass
from typing import List

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.ticker as ticker  # 単位表示用に追加
import serial

@dataclass
class DataPoint:
    t: float
    h: float
    p: float
    timestamp: float

data_history: List[DataPoint] = []
MAX_HISTORY_SECONDS = 600

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except Exception as e:
        print(f"Serial port error: {e}")
        return

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, sharex=True, figsize=(10, 9))
    fig.subplots_adjust(hspace=0.4)

    def update(frame):
        now = time.time()
        
        while ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line: continue
            try:
                data = json.loads(line)
                if data.get("type") == "sample":
                    data_history.append(DataPoint(
                        t=float(data["t"]),
                        h=float(data["h"]),
                        p=float(data["p"]),
                        timestamp=now
                    ))
            except: continue

        while data_history and (now - data_history[0].timestamp > MAX_HISTORY_SECONDS):
            data_history.pop(0)

        if not data_history: return

        times = [(p.timestamp - now) / 60.0 for p in data_history]
        latest = data_history[-1] # 最新のデータ点

        # 各グラフの共通設定
        for ax in [ax1, ax2, ax3]:
            ax.clear()
            ax.grid(True, linestyle='--', alpha=0.5)
            ax.set_xlim(-10, 0)

        # --- 温度 ---
        ax1.plot(times, [p.t for p in data_history], color="#e74c3c")
        ax1.set_ylabel("Temperature")
        ax1.yaxis.set_major_formatter(ticker.FormatStrFormatter('%.1f°C'))
        ax1.text(0.02, 0.9, f"Now: {latest.t:.1f}°C", transform=ax1.transAxes, fontweight='bold')

        # --- 湿度 ---
        ax2.plot(times, [p.h for p in data_history], color="#3498db")
        ax2.set_ylabel("Humidity")
        ax2.yaxis.set_major_formatter(ticker.FormatStrFormatter('%.1f%%'))
        ax2.text(0.02, 0.9, f"Now: {latest.h:.1f}%", transform=ax2.transAxes, fontweight='bold')

        # --- 気圧 (hPa表示を強化) ---
        ax3.plot(times, [p.p for p in data_history], color="#27ae60")
        ax3.set_ylabel("Pressure")
        # Y軸の目盛りに "hPa" を付加
        ax3.yaxis.set_major_formatter(ticker.FormatStrFormatter('%d hPa'))
        # 最新の値をグラフ内に表示
        ax3.text(0.02, 0.9, f"Now: {latest.p:.1f} hPa", transform=ax3.transAxes, 
                 fontsize=12, fontweight='bold', color="#27ae60",
                 bbox=dict(facecolor='white', alpha=0.7, edgecolor='none'))
        
        ax3.set_xlabel("Minutes before now")

    ani = animation.FuncAnimation(fig, update, interval=1000, cache_frame_data=False)

    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

if __name__ == "__main__":
    main()