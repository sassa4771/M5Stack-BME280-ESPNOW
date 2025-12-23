"""
pc_viewer_2dmap.py
- Gateway M5Stack が吐く 1行JSON をUSBシリアルで受信
- id -> (x,y) に割り当てて 2Dマップ表示（imshow）
"""

# pip install pyserial matplotlib numpy
import argparse
import json
import time
from dataclasses import dataclass
from typing import Dict

import numpy as np
import matplotlib.pyplot as plt
import serial

# ====== ここだけ調整（座席ID→座標）======
ID_TO_XY = {
    "A1": (0, 0), "B1": (1, 0), "C1": (2, 0), "D1": (3, 0),
    "A2": (0, 1), "B2": (1, 1), "C2": (2, 1), "D2": (3, 1),
    "A3": (0, 2), "B3": (1, 2), "C3": (2, 2), "D3": (3, 2),
    "A4": (0, 3), "B4": (1, 3), "C4": (2, 3), "D4": (3, 3),
}
GRID_W, GRID_H = 4, 4
STALE_SEC = 30
# =========================================

@dataclass
class Sample:
    t: float
    h: float
    p: float
    ts: float

latest: Dict[str, Sample] = {}

def build_grid(metric: str) -> np.ndarray:
    grid = np.full((GRID_H, GRID_W), np.nan, dtype=float)
    now = time.time()
    for sid, s in list(latest.items()):
        if now - s.ts > STALE_SEC:
            continue
        if sid not in ID_TO_XY:
            continue
        x, y = ID_TO_XY[sid]
        grid[y, x] = getattr(s, metric)
    return grid

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Windows例: COM5 / macOS例: /dev/tty.usbserial-xxxx")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--metric", choices=["t","h","p"], default="t")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)

    plt.ion()
    fig, ax = plt.subplots()
    
    # 温度の範囲を10度～40度に固定してヒートマップ表示
    if args.metric == "t":
        vmin, vmax = 10.0, 40.0
        cmap = "coolwarm"  # 温度に適したカラーマップ
    else:
        vmin, vmax = None, None
        cmap = "viridis"
    
    img = ax.imshow(np.full((GRID_H, GRID_W), np.nan), interpolation="nearest", 
                    vmin=vmin, vmax=vmax, cmap=cmap)
    
    # カラーバーを追加（温度の場合のみ範囲を表示）
    if args.metric == "t":
        cbar = plt.colorbar(img, ax=ax)
        cbar.set_label("温度 (°C)", rotation=270, labelpad=15)
    
    ax.set_xticks(range(GRID_W))
    ax.set_yticks(range(GRID_H))
    ax.set_xlabel("x")
    ax.set_ylabel("y")

    for sid, (x, y) in ID_TO_XY.items():
        ax.text(x, y, sid, ha="center", va="center", fontsize=9)

    last_draw = 0.0

    while True:
        line = ser.readline()
        if line:
            try:
                d = json.loads(line.decode("utf-8", errors="ignore").strip())
                if d.get("type") == "sample":
                    sid = d["id"]
                    latest[sid] = Sample(
                        t=float(d["t"]),
                        h=float(d["h"]),
                        p=float(d["p"]),
                        ts=time.time()
                    )
            except Exception:
                pass

        now = time.time()
        if now - last_draw >= 1.0:
            grid = build_grid(args.metric)
            img.set_data(grid)
            ax.set_title(f"2D Map metric={args.metric}  updated={time.strftime('%H:%M:%S')}")
            fig.canvas.draw()
            fig.canvas.flush_events()
            last_draw = now

if __name__ == "__main__":
    main()