# M5Stack-BME280-ESPNOW

M5Stack GrayとBME280センサーを使用したESP-NOW通信による環境センサーネットワークシステムです。複数のセンサーノードから温度・湿度・気圧データを収集し、PC上で2Dマップとして可視化します。

## 概要

このプロジェクトは以下の3つのコンポーネントで構成されています：

1. **Sender（送信機）**: BME280センサーから環境データを読み取り、ESP-NOWでゲートウェイに送信
2. **Gateway（ゲートウェイ）**: ESP-NOWで受信したデータをシリアルポート（USB）経由でPCに送信
3. **PC Viewer（可視化ツール）**: シリアルポートから受信したデータを2Dマップとして表示

## ハードウェア要件

- **マイコン**: M5Stack Gray
- **センサー**: BME280（I2C接続）
- **ボード**: ESP32 by Espressif Systems バージョン 2.0.1
- **ライブラリ**: M5Stack by M5Stack バージョン 0.3.1

## ソフトウェア要件

### Arduino IDE

- **ボードマネージャー**: ESP32 by Espressif Systems バージョン 2.0.1
- **ライブラリマネージャー**: M5Stack by M5Stack バージョン 0.3.1
- **追加ライブラリ**:
  - Adafruit BME280 Library
  - Adafruit Unified Sensor

### Python環境

以下のPythonパッケージが必要です：

```bash
pip install pyserial matplotlib numpy scipy
```

## プロジェクト構成

```
M5Stack-BME280-ESPNOW/
├── Arduino/
│   ├── gateway_espnow_serial/
│   │   └── gateway_espnow_serial.ino  # ゲートウェイ（受信→シリアル出力）
│   └── sender_bme280_espnow/
│       └── sender_bme280_espnow.ino   # 送信機（BME280→ESP-NOW）
└── python/
    ├── pc_viewer_2dmap.py              # 2Dマップ表示（4x4グリッド）
    └── pc_viewer_2dmap_interpolated.py # 2Dマップ表示（4x4→80x80補間）
```

## セットアップ手順

### 1. Arduino IDEの設定

1. Arduino IDEを開く
2. **ファイル > 環境設定 > 追加のボードマネージャーのURL**に以下を追加：
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **ツール > ボード > ボードマネージャー**で「ESP32」を検索し、**バージョン 2.0.1**をインストール
4. **ツール > ボード**で「M5Stack-ATOM」または「ESP32 Dev Module」を選択
5. **ツール > ライブラリを管理**で「M5Stack」を検索し、**バージョン 0.3.1**をインストール
6. **ツール > ライブラリを管理**で「Adafruit BME280 Library」と「Adafruit Unified Sensor」をインストール

### 2. ゲートウェイの設定とアップロード

1. `Arduino/gateway_espnow_serial/gateway_espnow_serial.ino`を開く
2. `CHANNEL`を確認（デフォルト: 1）
3. Arduino IDEでコンパイル・アップロード
4. シリアルモニターを開き（115200 baud）、ゲートウェイのMACアドレスを確認

### 3. 送信機の設定とアップロード

1. `Arduino/sender_bme280_espnow/sender_bme280_espnow.ino`を開く
2. 以下の設定を変更：
   - `DEVICE_ID`: デバイスID（例: "B1", "A2"など）
   - `GATEWAY_MAC`: ゲートウェイのMACアドレス（6バイト配列）
   - `CHANNEL`: ゲートウェイと同じチャンネル（デフォルト: 1）
3. Arduino IDEでコンパイル・アップロード
4. 複数の送信機を使用する場合は、各デバイスで`DEVICE_ID`を変更してアップロード

### 4. Python環境のセットアップ

```bash
cd python
pip install pyserial matplotlib numpy scipy
```

## 使用方法

### 1. ゲートウェイの起動

1. M5Stack Grayにゲートウェイスケッチをアップロード済みであることを確認
2. USBケーブルでPCに接続
3. シリアルポート番号を確認（Windows: COM5など、macOS/Linux: /dev/tty.usbserial-xxxxなど）

### 2. 送信機の起動

1. 各M5Stack Grayに送信機スケッチをアップロード済みであることを確認
2. 電源を投入（USBまたはバッテリー）

### 3. PC Viewerの起動

#### 基本表示（4x4グリッド）

```bash
python pc_viewer_2dmap.py --port COM5 --metric t
```

#### 補間表示（80x80グリッド）

```bash
python pc_viewer_2dmap_interpolated.py --port COM5 --metric t
```

#### オプション

- `--port`: シリアルポート名（必須）
  - Windows例: `COM5`
  - macOS例: `/dev/tty.usbserial-xxxx`
- `--baud`: ボーレート（デフォルト: 115200）
- `--metric`: 表示する指標
  - `t`: 温度（°C）
  - `h`: 湿度（%RH）
  - `p`: 気圧（hPa）

## データ形式

### ESP-NOWペイロード

```c
typedef struct {
  char id[4];      // デバイスID（例: "B1"）
  float t;         // 温度（°C）
  float h;         // 湿度（%RH）
  float p;         // 気圧（hPa）
  uint32_t seq;    // シーケンス番号
} Payload;
```

### シリアル出力（JSON）

```json
{"type":"sample","id":"B1","t":25.30,"h":55.20,"p":1013.25,"seq":123,"from":"98:f4:ab:6e:51:10"}
```

## 座席IDマッピング

デフォルトでは4x4グリッド（16座席）に対応しています。`pc_viewer_2dmap.py`の`ID_TO_XY`辞書で座席IDと座標の対応を変更できます：

```python
ID_TO_XY = {
    "A1": (0, 0), "B1": (1, 0), "C1": (2, 0), "D1": (3, 0),
    "A2": (0, 1), "B2": (1, 1), "C2": (2, 1), "D2": (3, 1),
    "A3": (0, 2), "B3": (1, 2), "C3": (2, 2), "D3": (3, 2),
    "A4": (0, 3), "B4": (1, 3), "C4": (2, 3), "D4": (3, 3),
}
```

## トラブルシューティング

### データが受信されない

1. ゲートウェイと送信機の`CHANNEL`が一致しているか確認
2. 送信機の`GATEWAY_MAC`が正しいか確認
3. シリアルモニターでゲートウェイが起動しているか確認
4. ESP-NOWの通信範囲内にいるか確認（通常10-50m）

### シリアルポートが見つからない

1. デバイスマネージャー（Windows）または`ls /dev/tty.*`（macOS/Linux）でポートを確認
2. 他のアプリケーションがポートを使用していないか確認
3. USBケーブルがデータ転送対応か確認

### BME280が見つからない

1. I2C接続を確認（SDA: GPIO21, SCL: GPIO22）
2. BME280のI2Cアドレスを確認（デフォルト: 0x76）
3. 配線を確認

## ライセンス

このプロジェクトのライセンス情報は各ファイルのヘッダーを参照してください。

## 参考資料

- [M5Stack公式ドキュメント](https://docs.m5stack.com/)
- [ESP-NOW公式ドキュメント](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library)

