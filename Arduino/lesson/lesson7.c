/*
 * lesson7.c
 * 共同作品用コード（室内の温度マップ作成）
 * lesson3.cをベースに、全参加者が同じチャンネルを使用するように設定
 * デバイスIDを座席番号に合わせて設定
 * PC側は既存のpc_viewer_2dmap.pyを使用可能
 */

#include <Wire.h>
#include <M5StickCPlus.h> // Plus用ライブラリ
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
// グラフ描画用の設定
const int GRAPH_HISTORY_SIZE = 20;  // 履歴データの数
const int GRAPH_X_START = 0;        // グラフのX開始位置
const int GRAPH_Y_START = 80;       // グラフのY開始位置
const int GRAPH_WIDTH = 240;        // グラフの幅
const int GRAPH_HEIGHT = 50;        // グラフの高さ

// データ履歴を保持する配列
float tempHistory[GRAPH_HISTORY_SIZE];
int historyIndex = 0;
bool historyFilled = false;

// グラフ表示するメトリクスを選択（'t': 温度, 'h': 湿度, 'p': 気圧）
// 参加者ミッション: この変数を変更して気圧や湿度のグラフを表示できるようにする
char displayMetric = 't';  // デフォルトは温度
// ========================

// ==== lesson3 で追加 ====
// ESP-NOW無線通信の設定
// ==== lesson7 で変更 ====
// 共同作品用: 全参加者が同じチャンネルを使用する
// 参加者ミッション: 講師から指定されたチャンネル番号に変更してください
const int CHANNEL = 1;  // ★全員が同じチャンネルに設定すること★

// ゲートウェイ（データを受け取る側）のMACアドレス
// 参加者ミッション: ゲートウェイのMACアドレスを確認して設定してください
uint8_t GATEWAY_MAC[6] = {0x98, 0xf4, 0xab, 0x6c, 0xe7, 0x88};  // 実際のMACアドレスに変更
const int SEND_INTERVAL_MS = 1000;  // データを送る間隔（1秒ごと）

// ==== lesson7 で変更 ====
// デバイスID（座席番号に合わせて設定）
// 参加者ミッション: 自分の座席番号に合わせて変更してください
// 例: "A1", "A2", "A3", "A4", "B1", "B2", "B3", "B4", "C1", "C2", "C3", "C4", "D1", "D2", "D3", "D4"
// pc_viewer_2dmap.pyのID_TO_XYマッピングに合わせて設定すること
const char DEVICE_ID[] = "B1";  // ★自分の座席番号に変更★
// ========================

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
// ESP-NOWピアの設定
static void ensurePeer() {
    if (esp_now_is_peer_exist(GATEWAY_MAC)) {
        esp_now_del_peer(GATEWAY_MAC);
    }
    esp_now_peer_info_t p{};
    memcpy(p.peer_addr, GATEWAY_MAC, 6);
    p.ifidx = WIFI_IF_STA;
    p.channel = CHANNEL;
    p.encrypt = false;

    esp_err_t e = esp_now_add_peer(&p);
    Serial.printf("[peer] add ret=%d\n", (int)e);
}

// ESP-NOW送信コールバック
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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
    
    // ==== lesson3 で追加 ====
    // ==== lesson7 で変更 ====
    // 起動画面を表示
    M5.Lcd.println("Collaborative");
    M5.Lcd.println("Temp Map");
    M5.Lcd.printf("ID:%s CH:%d\n", DEVICE_ID, CHANNEL);
    delay(1000);
    // ========================

    // ==== lesson2 で追加 ====
    // 履歴配列の初期化
    for (int i = 0; i < GRAPH_HISTORY_SIZE; i++) {
        tempHistory[i] = 0;
    }
    // ========================

    // ==== lesson3 で追加 ====
    // WiFiとESP-NOWの初期化
    WiFi.mode(WIFI_STA); // ステーションモード
    ESP_ERROR_CHECK(esp_wifi_start());
    delay(50);
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE)); // チャンネル設定
    WiFi.setSleep(false); // 省電力モードを無効化

    // 自分の情報をシリアル出力
    Serial.printf("My MAC: %s  CH=%d\n", WiFi.macAddress().c_str(), CHANNEL);
    Serial.printf("Device ID: %s\n", DEVICE_ID);
    Serial.println("=== Collaborative Temperature Map ===");

    // ESP-NOWを開始
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(OnDataSent); // 送信完了時のコールバックを登録
    ensurePeer(); // ゲートウェイを登録

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Ready.");
    M5.Lcd.printf("ID: %s\n", DEVICE_ID);
    delay(500);
    // ========================
}

// ==== lesson2 で追加 ====
// グラフを描画する関数
// value: 現在取得した値（温度、湿度、気圧など）
void drawGraph(float value) {
    // 履歴データに追加
    tempHistory[historyIndex] = value;
    historyIndex++;
    if (historyIndex >= GRAPH_HISTORY_SIZE) {
        historyIndex = 0;
        historyFilled = true;
    }
    
    // グラフ領域をクリア
    M5.Lcd.fillRect(GRAPH_X_START, GRAPH_Y_START, GRAPH_WIDTH, GRAPH_HEIGHT, BLACK);
    
    // グラフの枠線を描画
    M5.Lcd.drawRect(GRAPH_X_START, GRAPH_Y_START, GRAPH_WIDTH, GRAPH_HEIGHT, WHITE);
    
    // データの最大値と最小値を計算
    float minVal = 999, maxVal = -999;
    int dataCount = historyFilled ? GRAPH_HISTORY_SIZE : historyIndex;
    
    if (dataCount == 0) return;
    
    for (int i = 0; i < dataCount; i++) {
        float val = tempHistory[i];
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }
    
    // 値の範囲を少し広げる
    float range = maxVal - minVal;
    if (range < 1.0) range = 1.0;  // 最小範囲を確保
    minVal -= range * 0.1;
    maxVal += range * 0.1;
    
    // グラフの線を描画
    if (dataCount > 1) {
        for (int i = 0; i < dataCount - 1; i++) {
            int x1 = GRAPH_X_START + 5 + (i * (GRAPH_WIDTH - 10) / (dataCount - 1));
            int y1 = GRAPH_Y_START + GRAPH_HEIGHT - 5 - ((tempHistory[i] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
            
            int x2 = GRAPH_X_START + 5 + ((i + 1) * (GRAPH_WIDTH - 10) / (dataCount - 1));
            int y2 = GRAPH_Y_START + GRAPH_HEIGHT - 5 - ((tempHistory[i + 1] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
            
            // 範囲チェック
            if (y1 < GRAPH_Y_START + 5) y1 = GRAPH_Y_START + 5;
            if (y1 > GRAPH_Y_START + GRAPH_HEIGHT - 5) y1 = GRAPH_Y_START + GRAPH_HEIGHT - 5;
            if (y2 < GRAPH_Y_START + 5) y2 = GRAPH_Y_START + 5;
            if (y2 > GRAPH_Y_START + GRAPH_HEIGHT - 5) y2 = GRAPH_Y_START + GRAPH_HEIGHT - 5;
            
            M5.Lcd.drawLine(x1, y1, x2, y2, GREEN);
        }
    }
    
    // 現在の値を点で表示
    if (dataCount > 0) {
        int lastIdx = (historyIndex - 1 + GRAPH_HISTORY_SIZE) % GRAPH_HISTORY_SIZE;
        int x = GRAPH_X_START + 5 + ((dataCount - 1) * (GRAPH_WIDTH - 10) / max(1, dataCount - 1));
        int y = GRAPH_Y_START + GRAPH_HEIGHT - 5 - ((tempHistory[lastIdx] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
        if (y < GRAPH_Y_START + 5) y = GRAPH_Y_START + 5;
        if (y > GRAPH_Y_START + GRAPH_HEIGHT - 5) y = GRAPH_Y_START + GRAPH_HEIGHT - 5;
        M5.Lcd.fillCircle(x, y, 3, YELLOW);
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
    Payload pl{};
    strncpy(pl.id, DEVICE_ID, sizeof(pl.id) - 1);
    pl.id[sizeof(pl.id) - 1] = '\0';
    pl.t = temp;
    pl.h = hum;
    pl.p = pres;
    pl.seq = seq++;

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
    
    // ==== lesson3 で追加 ====
    // データ送信の状態を表示
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.printf("seq: %lu\n", (unsigned long)pl.seq); // シーケンス番号
    M5.Lcd.setTextColor(result == ESP_OK ? GREEN : RED); // 成功なら緑、失敗なら赤
    M5.Lcd.printf("send: %d\n", (int)result);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.printf("OK:%d NG:%d\n", sendSuccessCount, sendFailCount); // 成功回数と失敗回数
    // ========================

    // ==== lesson2 で追加 ====
    // グラフを描画（取得した値だけを渡す）
    // 参加者ミッション: displayMetricを変更して気圧や湿度のグラフを表示できるようにする
    // 例: displayMetric = 'h'; で湿度のグラフ
    // 例: displayMetric = 'p'; で気圧のグラフ
    float valueToGraph = 0;
    switch (displayMetric) {
        case 't': valueToGraph = temp; break;
        case 'h': valueToGraph = hum; break;
        case 'p': valueToGraph = pres; break;
        default: valueToGraph = temp; break;
    }
    drawGraph(valueToGraph);
    // ========================

    delay(SEND_INTERVAL_MS);
}

