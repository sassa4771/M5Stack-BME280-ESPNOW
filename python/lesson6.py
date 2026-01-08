"""
lesson6.py
- Gateway M5Stack が吐く 1行JSON をUSBシリアルで受信
- 複数デバイスの温度・湿度・気圧を時系列グラフで表示
- 参加者ミッション: AIを使って参加者がそれぞれ行う（参考プロンプトを用意）
"""

# pip install pyserial matplotlib numpy
import argparse
import json
import time
from collections import defaultdict
from dataclasses import dataclass
from typing import Dict, List

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial

# ==== lesson6 で追加 ====
# データ保持用の構造
@dataclass
class DataPoint:
    t: float  # 温度
    h: float  # 湿度
    p: float  # 気圧
    timestamp: float  # 受信時刻

# 各デバイスごとのデータ履歴
device_data: Dict[str, List[DataPoint]] = defaultdict(list)
MAX_HISTORY = 100  # 保持する最大データポイント数

# グラフの設定
fig, axes = plt.subplots(3, 1, figsize=(12, 8))  # 3つのサブプロット（温度・湿度・気圧）
axes[0].set_title("Temperature (°C)", fontsize=12)
axes[1].set_title("Humidity (%)", fontsize=12)
axes[2].set_title("Pressure (hPa)", fontsize=12)

for ax in axes:
    ax.set_xlabel("Time")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0, MAX_HISTORY)

# カラーマップ（デバイスごとに異なる色）
colors = ['red', 'blue', 'green', 'orange', 'purple', 'brown', 'pink', 'gray', 'olive', 'cyan']
device_colors: Dict[str, str] = {}
# ========================

def update_graph(frame):
    """グラフを更新する関数（アニメーション用）"""
    global device_data
    
    # 各サブプロットをクリア
    for ax in axes:
        ax.clear()
        ax.grid(True, alpha=0.3)
    
    axes[0].set_title("Temperature (°C)", fontsize=12)
    axes[1].set_title("Humidity (%)", fontsize=12)
    axes[2].set_title("Pressure (hPa)", fontsize=12)
    
    # 各デバイスのデータをプロット
    for device_id, data_list in device_data.items():
        if len(data_list) == 0:
            continue
        
        # デバイスに色を割り当て
        if device_id not in device_colors:
            device_colors[device_id] = colors[len(device_colors) % len(colors)]
        
        color = device_colors[device_id]
        
        # データを準備
        timestamps = [d.timestamp for d in data_list]
        temps = [d.t for d in data_list]
        hums = [d.h for d in data_list]
        pres = [d.p for d in data_list]
        
        # 相対時間に変換（最新の時刻を0とする）
        if len(timestamps) > 0:
            latest_time = timestamps[-1]
            relative_times = [(t - latest_time) / 60.0 for t in timestamps]  # 分単位
        
        # 各メトリクスをプロット
        axes[0].plot(relative_times, temps, label=device_id, color=color, linewidth=2)
        axes[1].plot(relative_times, hums, label=device_id, color=color, linewidth=2)
        axes[2].plot(relative_times, pres, label=device_id, color=color, linewidth=2)
    
    # 凡例を表示
    for ax in axes:
        ax.legend(loc='upper right', fontsize=8)
        ax.set_xlim(-10, 1)  # 過去10分間を表示
    
    axes[0].set_ylabel("°C")
    axes[1].set_ylabel("%")
    axes[2].set_ylabel("hPa")
    axes[2].set_xlabel("Time (minutes ago)")

def main():
    parser = argparse.ArgumentParser(description="ESP-NOW Gateway Serial Viewer with Time Series Graph")
    parser.add_argument("--port", required=True, help="Windows例: COM5 / macOS例: /dev/tty.usbserial-xxxx")
    parser.add_argument("--baud", type=int, default=115200, help="ボーレート（デフォルト: 115200）")
    args = parser.parse_args()

    # ==== lesson6 で追加 ====
    # シリアルポートを開く
    # 参加者ミッション: COMの設定が人によって違うのが学習ポイント
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
        print(f"シリアルポート {args.port} に接続しました（ボーレート: {args.baud}）")
    except serial.SerialException as e:
        print(f"エラー: シリアルポート {args.port} を開けませんでした: {e}")
        print("デバイスマネージャー（Windows）または ls /dev/tty.*（macOS/Linux）でポートを確認してください")
        return
    # ========================

    # グラフをインタラクティブモードで表示
    plt.ion()
    plt.tight_layout()
    
    # アニメーションを開始
    ani = animation.FuncAnimation(fig, update_graph, interval=1000, blit=False)
    
    print("データ受信を開始しました。グラフウィンドウを閉じると終了します。")
    print("受信データ:")
    
    try:
        while True:
            line = ser.readline()
            if line:
                try:
                    # JSONをパース
                    decoded = line.decode("utf-8", errors="ignore").strip()
                    if not decoded:
                        continue
                    
                    d = json.loads(decoded)
                    
                    # サンプルデータを処理
                    if d.get("type") == "sample":
                        device_id = d["id"]
                        timestamp = time.time()
                        
                        # データポイントを作成
                        point = DataPoint(
                            t=float(d["t"]),
                            h=float(d["h"]),
                            p=float(d["p"]),
                            timestamp=timestamp
                        )
                        
                        # デバイスごとの履歴に追加
                        device_data[device_id].append(point)
                        
                        # 最大履歴数を超えたら古いデータを削除
                        if len(device_data[device_id]) > MAX_HISTORY:
                            device_data[device_id].pop(0)
                        
                        # コンソールに表示
                        print(f"[{time.strftime('%H:%M:%S')}] {device_id}: "
                              f"T={point.t:.2f}°C, H={point.h:.2f}%, P={point.p:.2f}hPa")
                    
                    # ゲートウェイ起動メッセージを処理
                    elif d.get("type") == "gateway_boot":
                        print(f"ゲートウェイ起動: MAC={d.get('mac')}, CH={d.get('channel')}")
                
                except json.JSONDecodeError:
                    # JSONパースエラーは無視
                    pass
                except Exception as e:
                    print(f"エラー: {e}")
            
            # グラフを更新
            plt.pause(0.01)
    
    except KeyboardInterrupt:
        print("\n終了します...")
    finally:
        ser.close()
        plt.close()

if __name__ == "__main__":
    main()

