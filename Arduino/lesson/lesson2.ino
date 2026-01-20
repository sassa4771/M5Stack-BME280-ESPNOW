#include <Wire.h>
#include <M5StickCPlus.h> // Plus用ライブラリ
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// BME280のインスタンス作成
Adafruit_BME280 bme;

float temp = 0; //温度の値
float hum = 0;  //湿度の値
float pres = 0; //圧力の値

// ==== lesson2 で追加 ====
// グラフを表示するための設定
const int GRAPH_HISTORY_SIZE = 20;  // グラフに表示するデータの数
const int GRAPH_X_START = 0;        // グラフの左端の位置
const int GRAPH_Y_START = 50;       // グラフの上端の位置
const int GRAPH_WIDTH = 220;        // グラフの幅
const int GRAPH_HEIGHT = 80;        // グラフの高さ

// 過去の温度データを保存する配列
float tempHistory[GRAPH_HISTORY_SIZE];
int historyIndex = 0; // 次にデータを保存する位置
bool historyFilled = false; // 配列が満杯になったかどうか

// グラフの表示範囲を保持（急な変化でも範囲が変わらないように）
float graphMinVal = 0;  // グラフの最小値
float graphMaxVal = 0;  // グラフの最大値
bool graphRangeInitialized = false; // 範囲が初期化されたかどうか

// ========================

void setup() {
    M5.begin();
    Serial.begin(115200);

    // I2C通信の初期化 (M5StickC/PlusのGroveポート: SDA=32, SCL=33)
    Wire.begin(32, 33);

    // BME280の初期化 (0x76は多くのアドオンのデフォルトアドレス)
    if (!bme.begin(0x76, &Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        M5.Lcd.println("Sensor Error");
        while (1);
    }

    // ディスプレイの初期設定
    M5.Lcd.setRotation(1);    // 横向き
    M5.Lcd.setTextSize(2);    // 文字サイズ
    M5.Lcd.fillScreen(BLACK); // 画面クリア
    M5.Lcd.println("BME280 Init OK");
    delay(1000);

    // ==== lesson2 で追加 ====
    // 履歴配列の初期化
    for (int i = 0; i < GRAPH_HISTORY_SIZE; i++) {
        tempHistory[i] = 0;
    }
    // ========================
}

// ==== lesson2 で追加 ====
// グラフを描画する関数
// value: 現在の値（温度、湿度、気圧など）
void drawGraph(float value) {
    // 新しい値を配列に保存
    tempHistory[historyIndex] = value;
    historyIndex++;
    // 配列の最後まで来たら最初に戻る
    if (historyIndex >= GRAPH_HISTORY_SIZE) {
        historyIndex = 0;
        historyFilled = true;
    }
    
    // グラフを描く場所を黒で塗りつぶす
    M5.Lcd.fillRect(GRAPH_X_START, GRAPH_Y_START, GRAPH_WIDTH, GRAPH_HEIGHT, BLACK);
    
    // グラフの枠を白い線で描く
    M5.Lcd.drawRect(GRAPH_X_START, GRAPH_Y_START, GRAPH_WIDTH, GRAPH_HEIGHT, WHITE);
    
    // データの最大値と最小値を計算
    float minVal = 999, maxVal = -999;
    int dataCount = historyFilled ? GRAPH_HISTORY_SIZE : historyIndex;
    
    if (dataCount == 0) return;
    
    // 最大値と最小値を計算（循環バッファを考慮）
    for (int i = 0; i < dataCount; i++) {
        int idx;
        if (historyFilled) {
            // 配列が満杯の場合、historyIndexから時系列順に取得
            idx = (historyIndex + i) % GRAPH_HISTORY_SIZE;
        } else {
            // 配列が満杯でない場合、0から順番に取得
            idx = i;
        }
        float val = tempHistory[idx];
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }
    
    // グラフの表示範囲を決定（急な変化でも範囲が変わらないように）
    if (!graphRangeInitialized) {
        // 初回は現在のデータ範囲に余裕を持たせて設定
        float range = maxVal - minVal;
        if (range < 1.0) range = 1.0;  // 最小範囲を確保
        graphMinVal = minVal - range * 0.2;  // 上下20%の余裕
        graphMaxVal = maxVal + range * 0.2;
        graphRangeInitialized = true;
    } else {
        // 既に範囲が設定されている場合、新しい値が範囲外に出た場合のみ拡張
        float margin = (graphMaxVal - graphMinVal) * 0.1;  // 現在の範囲の10%をマージンとして使用
        if (maxVal > graphMaxVal - margin) {
            // 上限に近づいたら、余裕を持たせて拡張
            float range = graphMaxVal - graphMinVal;
            if (range < 1.0) range = 1.0;
            graphMaxVal = maxVal + range * 0.2;  // 20%の余裕を追加
        }
        if (minVal < graphMinVal + margin) {
            // 下限に近づいたら、余裕を持たせて拡張
            float range = graphMaxVal - graphMinVal;
            if (range < 1.0) range = 1.0;
            graphMinVal = minVal - range * 0.2;  // 20%の余裕を追加
        }
    }
    
    // 表示用の範囲を設定
    minVal = graphMinVal;
    maxVal = graphMaxVal;
    
    // グラフの線を描画
    if (dataCount > 1) {
        for (int i = 0; i < dataCount - 1; i++) {
            // 時系列順にデータを取得（循環バッファを考慮）
            int idx1, idx2;
            if (historyFilled) {
                // 配列が満杯の場合、historyIndexから時系列順に取得
                idx1 = (historyIndex + i) % GRAPH_HISTORY_SIZE;
                idx2 = (historyIndex + i + 1) % GRAPH_HISTORY_SIZE;
            } else {
                // 配列が満杯でない場合、0から順番に取得
                idx1 = i;
                idx2 = i + 1;
            }
            
            int x1 = GRAPH_X_START + 5 + (i * (GRAPH_WIDTH - 10) / (dataCount - 1));
            int y1 = GRAPH_Y_START + GRAPH_HEIGHT - 5 - ((tempHistory[idx1] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
            
            int x2 = GRAPH_X_START + 5 + ((i + 1) * (GRAPH_WIDTH - 10) / (dataCount - 1));
            int y2 = GRAPH_Y_START + GRAPH_HEIGHT - 5 - ((tempHistory[idx2] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
            
            // 範囲チェック
            if (y1 < GRAPH_Y_START + 5) y1 = GRAPH_Y_START + 5;
            if (y1 > GRAPH_Y_START + GRAPH_HEIGHT - 5) y1 = GRAPH_Y_START + GRAPH_HEIGHT - 5;
            if (y2 < GRAPH_Y_START + 5) y2 = GRAPH_Y_START + 5;
            if (y2 > GRAPH_Y_START + GRAPH_HEIGHT - 5) y2 = GRAPH_Y_START + GRAPH_HEIGHT - 5;
            
            M5.Lcd.drawLine(x1, y1, x2, y2, GREEN);
        }
    }
    
    // 最小値と最大値を表示
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.setCursor(GRAPH_X_START + 2, GRAPH_Y_START + GRAPH_HEIGHT - 10);
    M5.Lcd.printf("%.1f", minVal);
    M5.Lcd.setCursor(GRAPH_X_START + 2, GRAPH_Y_START + 2);
    M5.Lcd.printf("%.1f", maxVal);
    M5.Lcd.setTextSize(2);
}
// ========================

void loop() {
    // --- センサーから実際の値を取得 ---
    temp = bme.readTemperature();          // 温度 (℃)
    hum  = bme.readHumidity();             // 湿度 (%)
    pres = bme.readPressure() / 100.0F;    // 気圧 (hPaに変換)
    M5.Lcd.setCursor(0,0);
    // --- ディスプレイ表示の更新 ---
    M5.Lcd.fillScreen(BLACK); 
    
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.printf("Temp: %.2f C\n", temp);
    M5.Lcd.printf("Hum:  %.2f %%\n", hum);
    M5.Lcd.printf("Pres: %.2f hPa\n", pres);
    
    // ==== lesson2 で追加 ====
    // グラフを描画（取得した値だけを渡す）
    // 参加者ミッション: 気圧や湿度のグラフを表示するように変更してみましょう
    // 例: drawGraph(pres);  // 気圧のグラフ
    // 例: drawGraph(hum);  // 湿度のグラフ
    drawGraph(temp);  // 温度のグラフ
    // ========================

    delay(1000); // 1秒ごとに更新
}

