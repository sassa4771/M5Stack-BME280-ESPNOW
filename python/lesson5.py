import argparse
import json
import time
import serial

def main():
    parser = argparse.ArgumentParser(description="M5Stack Serial JSON Receiver")
    parser.add_argument("--port", required=True, help="例: COM5 または /dev/tty.usbserial-xxxx")
    parser.add_argument("--baud", type=int, default=115200, help="ボーレート（デフォルト: 115200）")
    args = parser.parse_args()

    # シリアルポートの設定
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        print(f"--- ポート {args.port} で受信開始（Ctrl+C で終了） ---")
    except Exception as e:
        print(f"エラー: ポートを開けませんでした: {e}")
        return

    try:
        while True:
            # 1行読み込み
            line = ser.readline()
            if not line:
                continue

            try:
                # デコードしてJSONとして解析
                decoded = line.decode("utf-8", errors="ignore").strip()
                if not decoded:
                    continue
                
                data = json.loads(decoded)

                # データ型に応じた表示
                msg_type = data.get("type")

                if msg_type == "sample":
                    # センサーデータの表示
                    dev_id = data.get("id", "Unknown")
                    t = data.get("t", 0.0)
                    h = data.get("h", 0.0)
                    p = data.get("p", 0.0)
                    print(f"[{time.strftime('%H:%M:%S')}] ID:{dev_id} | 温度:{t:>5.2f}°C | 湿度:{h:>5.2f}% | 気圧:{p:>7.2f}hPa")

                elif msg_type == "gateway_boot":
                    # 起動メッセージの表示
                    print(f"--- ゲートウェイ起動: MAC={data.get('mac')} ---")

                else:
                    # その他のJSONデータ
                    print(f"受信データ: {data}")

            except json.JSONDecodeError:
                # JSONじゃない文字列（デバッグログなど）が流れてきた場合
                print(f"Rawログ: {decoded}")
            except Exception as e:
                print(f"解析エラー: {e}")

    except KeyboardInterrupt:
        print("\n終了します。")
    finally:
        ser.close()

if __name__ == "__main__":
    main()