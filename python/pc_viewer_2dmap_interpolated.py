"""
pc_viewer_2dmap.py
- Gateway M5Stack が吐く 1行JSON をUSBシリアルで受信
- id -> (x,y) に割り当てて 2Dマップ表示（4x4 → 80x80 補間）
- NaN（未着/STALE）は必ず出る前提：
  1) まず 4x4 の NaN を近傍平均で inpaint
  2) scipy.ndimage.zoom(order=3) で 80x80 にアップサンプリング（cubic相当）
"""

# pip install pyserial matplotlib numpy scipy
import argparse
import json
import time
from dataclasses import dataclass
from typing import Dict

import numpy as np
import matplotlib.pyplot as plt
import serial
from scipy.ndimage import zoom

# ====== ここだけ調整（座席ID→座標）======
ID_TO_XY = {
    "A1": (0, 0), "B1": (1, 0), "C1": (2, 0), "D1": (3, 0),
    "A2": (0, 1), "B2": (1, 1), "C2": (2, 1), "D2": (3, 1),
    "A3": (0, 2), "B3": (1, 2), "C3": (2, 2), "D3": (3, 2),
    "A4": (0, 3), "B4": (1, 3), "C4": (2, 3), "D4": (3, 3),
}
GRID_W, GRID_H = 4, 4
STALE_SEC = 30

# 出力解像度（ここを変えるだけでOK）
OUT_W, OUT_H = 80, 80
# =========================================

@dataclass
class Sample:
    t: float
    h: float
    p: float
    ts: float

latest: Dict[str, Sample] = {}

def build_grid(metric: str) -> np.ndarray:
    """4x4 のグリッドを作る（欠損は NaN）"""
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

def inpaint_nans_iterative(a: np.ndarray, max_iter: int = 50, metric: str = "t") -> np.ndarray:
    """
    NaNを近傍(8近傍)平均で埋める簡易inpaint.
    追加ライブラリ不要（numpyのみ）。max_iter回まで繰り返す。
    温度（t）の場合、残ったNaNは25度で埋める。
    """
    b = a.copy().astype(float)
    if np.all(np.isnan(b)):
        # 全部NaNの場合、温度なら25度で埋める
        if metric == "t":
            b[:] = 25.0
        return b

    for _ in range(max_iter):
        nan_mask = np.isnan(b)
        if not nan_mask.any():
            break

        # 端をNaNでパディング
        p = np.pad(b, 1, mode="constant", constant_values=np.nan)

        # 8近傍
        nbs = [
            p[:-2, 1:-1],  # up
            p[2:, 1:-1],   # down
            p[1:-1, :-2],  # left
            p[1:-1, 2:],   # right
            p[:-2, :-2],   # up-left
            p[:-2, 2:],    # up-right
            p[2:, :-2],    # down-left
            p[2:, 2:],     # down-right
        ]

        s = np.zeros_like(b)
        c = np.zeros_like(b)

        for nb in nbs:
            m = ~np.isnan(nb)
            s[m] += nb[m]
            c[m] += 1

        fillable = nan_mask & (c > 0)
        if not fillable.any():
            break

        b[fillable] = s[fillable] / c[fillable]

    # まだNaNが残る場合
    if np.isnan(b).any():
        if metric == "t":
            # 温度の場合は25度で埋める
            b[np.isnan(b)] = 25.0
        elif not np.all(np.isnan(b)):
            # その他の場合は全体平均で埋める
            b[np.isnan(b)] = np.nanmean(b)

    return b

def upscale_to_80x80(coarse_filled: np.ndarray, metric: str = "t") -> np.ndarray:
    """4x4 → 80x80 にcubic相当（order=3）でアップサンプリング"""
    if np.all(np.isnan(coarse_filled)):
        # 全部NaNの場合、温度なら25度で埋める
        if metric == "t":
            return np.full((OUT_H, OUT_W), 25.0, dtype=float)
        return np.full((OUT_H, OUT_W), np.nan, dtype=float)

    zoom_y = OUT_H / GRID_H
    zoom_x = OUT_W / GRID_W
    fine = zoom(coarse_filled, (zoom_y, zoom_x), order=3)  # order=3 = cubic

    # zoomの結果が(OUT_H, OUT_W)からズレることが稀にあるので保険
    fine = fine[:OUT_H, :OUT_W]
    if fine.shape != (OUT_H, OUT_W):
        pad = np.full((OUT_H, OUT_W), np.nan, dtype=float)
        pad[:fine.shape[0], :fine.shape[1]] = fine
        fine = pad

    # まだNaNが残る場合、温度なら25度で埋める
    if np.isnan(fine).any():
        if metric == "t":
            fine[np.isnan(fine)] = 25.0

    return fine

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Windows例: COM5 / macOS例: /dev/tty.usbserial-xxxx")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--metric", choices=["t", "h", "p"], default="t")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)

    plt.ion()
    fig, ax = plt.subplots()

    # 温度の範囲を10度～40度に固定してヒートマップ表示
    if args.metric == "t":
        vmin, vmax = 10.0, 40.0
        cmap = "coolwarm"
        cbar_label = "温度 (°C)"
    elif args.metric == "h":
        vmin, vmax = 0.0, 100.0
        cmap = "viridis"
        cbar_label = "湿度 (%RH)"
    else:
        vmin, vmax = None, None
        cmap = "viridis"
        cbar_label = "気圧 (hPa)"

    # 80x80表示（データ自体を80x80にするのでinterpolationは不要）
    img = ax.imshow(
        np.full((OUT_H, OUT_W), np.nan),
        interpolation="nearest",
        vmin=vmin, vmax=vmax, cmap=cmap
    )

    cbar = plt.colorbar(img, ax=ax)
    cbar.set_label(cbar_label, rotation=270, labelpad=15)

    ax.set_xlabel("x (fine grid)")
    ax.set_ylabel("y (fine grid)")

    # 座席IDを80x80の座標にスケールして描く（初期化）
    sx = (OUT_W - 1) / (GRID_W - 1)
    sy = (OUT_H - 1) / (GRID_H - 1)
    text_objects = {}
    for sid, (x, y) in ID_TO_XY.items():
        text_objects[sid] = ax.text(x * sx, y * sy, sid, ha="center", va="center", fontsize=9, color="black")

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
            # 4x4（NaNあり）→ NaN埋め → 80x80補間データ
            grid_raw = build_grid(args.metric)
            grid_filled = inpaint_nans_iterative(grid_raw, metric=args.metric)
            grid_fine = upscale_to_80x80(grid_filled, metric=args.metric)

            # データが来ている座標と来ていない座標を視覚的に区別
            for sid, (x, y) in ID_TO_XY.items():
                if sid in text_objects:
                    if np.isnan(grid_raw[y, x]):
                        # データが来ていない座標は赤色
                        text_objects[sid].set_color("red")
                    else:
                        # データが来ている座標は黒色
                        text_objects[sid].set_color("black")

            img.set_data(grid_fine)
            ax.set_title(f"2D Map (4x4→{OUT_W}x{OUT_H}) metric={args.metric}  updated={time.strftime('%H:%M:%S')}")
            fig.canvas.draw()
            fig.canvas.flush_events()
            last_draw = now

if __name__ == "__main__":
    main()
