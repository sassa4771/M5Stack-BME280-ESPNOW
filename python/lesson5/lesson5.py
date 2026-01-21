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
                
                # まず1行全体をJSONとして解析を試みる
                try:
                    data = json.loads(decoded)
                    print(json.dumps(data, ensure_ascii=False))
                    continue
                except json.JSONDecodeError:
                    # 1行全体がJSONでない場合、複数のJSONが含まれている可能性がある
                    pass
                
                # 複数のJSONが1行に含まれている場合、完全なJSONオブジェクトを抽出
                buffer = ""
                brace_count = 0
                in_string = False
                escape_next = False
                
                for char in decoded:
                    if escape_next:
                        buffer += char
                        escape_next = False
                        continue
                    
                    if char == '\\':
                        buffer += char
                        escape_next = True
                        continue
                    
                    if char == '"' and not escape_next:
                        in_string = not in_string
                        buffer += char
                        continue
                    
                    if not in_string:
                        if char == '{':
                            if brace_count == 0:
                                # 新しいJSONオブジェクトの開始
                                buffer = char
                            else:
                                buffer += char
                            brace_count += 1
                        elif char == '}':
                            buffer += char
                            brace_count -= 1
                            if brace_count == 0:
                                # 完全なJSONオブジェクトが見つかった
                                try:
                                    data = json.loads(buffer)
                                    print(json.dumps(data, ensure_ascii=False))
                                except json.JSONDecodeError:
                                    # 壊れたJSONは無視
                                    pass
                                buffer = ""
                        else:
                            if brace_count > 0:
                                buffer += char
                    else:
                        buffer += char

            except Exception:
                # 予期しないエラーは無視（壊れたデータの可能性）
                pass

    except KeyboardInterrupt:
        print("\n終了します。")
    finally:
        ser.close()

if __name__ == "__main__":
    main()