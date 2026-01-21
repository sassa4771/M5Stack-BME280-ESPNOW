#include <Wire.h>
#include <M5StickCPlus.h> 
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ==== lesson3 で追加 ====
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
// ========================

// BME280のインスタンス作成
Adafruit_BME280 bme;

float temp = 0; //温度の値
float hum = 0;  //湿度の値
float pres = 0; //圧力の値


// ==== lesson2 で追加 ====
// グラフを表示するための設定
const int GRAPH_HISTORY_SIZE = 20;  // グラフに表示するデータの数
const int GRAPH_X_START = 0;        // グラフの左端の位置
const int GRAPH_Y_START = 80;       // グラフの上端の位置
const int GRAPH_WIDTH = 220;        // グラフの幅
const int GRAPH_HEIGHT = 50;        // グラフの高さ

// 過去の温度データを保存する配列
float tempHistory[GRAPH_HISTORY_SIZE];
int historyIndex = 0; // 次にデータを保存する位置
bool historyFilled = false; // 配列が満杯になったかどうか

// グラフの表示範囲を保持（急な変化でも範囲が変わらないように）
float graphMinVal = 0;  // グラフの最小値
float graphMaxVal = 0;  // グラフの最大値
bool graphRangeInitialized = false; // 範囲が初期化されたかどうか

// ========================

// ==== lesson3 で追加 ====
// ESP-NOW無線通信の設定
// ゲートウェイ（データを受け取る側）のMACアドレスを設定
// 参加者ミッション: ゲートウェイのMACアドレスを確認して設定してください
uint8_t GATEWAY_MAC[6] = {0x98, 0xf4, 0xab, 0x6e, 0x51, 0x10};  // 実際のMACアドレスに変更
const int CHANNEL = 1;  // ゲートウェイと同じチャンネル番号に設定
const int SEND_INTERVAL_MS = 1000;  // データを送る間隔（1秒ごと）

// このデバイスのID（識別番号）
// 参加者ミッション: 自分の割り当てられた番号に変更してください
const char DEVICE_ID[] = "B1";  // 例: "A1", "B2"など

// ESP-NOWペイロード構造体
typedef struct __attribute__((packed)) {
    char id[4];
    float t;
    float h;
    float p;
    uint32_t seq;
} Payload;

uint32_t seq = 0;
int sendSuccessCount = 0;
int sendFailCount = 0;
// ========================

// ==== lesson3 で追加 ====
// ゲートウェイとの通信を設定する関数
static void ensurePeer() {
    // 既に登録されていれば削除
    if (esp_now_is_peer_exist(GATEWAY_MAC)) {
        esp_now_del_peer(GATEWAY_MAC);
    }
    // ゲートウェイの情報を設定
    esp_now_peer_info_t p{};
    memcpy(p.peer_addr, GATEWAY_MAC, 6);
    p.ifidx = WIFI_IF_STA;
    p.channel = CHANNEL;
    p.encrypt = false;

    // ゲートウェイを登録
    esp_err_t e = esp_now_add_peer(&p);
    Serial.printf("[peer] add ret=%d\n", (int)e);
}

// データ送信が完了したときに呼ばれる関数
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // 送信成功か失敗かをカウント
    if (status == ESP_NOW_SEND_SUCCESS) {
        sendSuccessCount++;
    } else {
        sendFailCount++;
    }
}
// ========================

void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(200);

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

    // ==== lesson2 で追加 ====
    // 履歴配列の初期化
    for (int i = 0; i < GRAPH_HISTORY_SIZE; i++) {
        tempHistory[i] = 0;
    }
    // ========================

    // ==== lesson3 で追加 ====
    // WiFiとESP-NOWの初期化
    WiFi.mode(WIFI_STA); // WiFiをステーションモードに設定
    ESP_ERROR_CHECK(esp_wifi_start());
    delay(50);
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE)); // チャンネルを設定
    WiFi.setSleep(false); // 省電力モードを無効化

    // 自分のMACアドレスを表示
    Serial.printf("My MAC: %s  CH=%d\n", WiFi.macAddress().c_str(), CHANNEL);

    // ESP-NOWを開始
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(OnDataSent); // 送信完了時のコールバックを登録
    ensurePeer(); // ゲートウェイを登録
    // ========================
}

// ==== lesson2 で追加 ====
// グラフを描画する関数
// value: 現在取得した値（温度、湿度、気圧など）
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

    // ==== lesson3 で追加 ====
    // ESP-NOWでデータを送信
    Payload pl{}; // 送信するデータの構造体を作成
    strncpy(pl.id, DEVICE_ID, sizeof(pl.id) - 1); // デバイスIDをコピー
    pl.id[sizeof(pl.id) - 1] = '\0';
    pl.t = temp; // 温度
    pl.h = hum;  // 湿度
    pl.p = pres; // 気圧
    pl.seq = seq++; // シーケンス番号（送信回数）

    // ゲートウェイにデータを送信
    esp_err_t result = esp_now_send(GATEWAY_MAC, (uint8_t*)&pl, sizeof(pl));
    // ========================
    
    // --- ディスプレイ表示の更新 ---
    M5.Lcd.fillScreen(BLACK); 
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.printf("ID: %s\n", DEVICE_ID);
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

    delay(SEND_INTERVAL_MS);
}
