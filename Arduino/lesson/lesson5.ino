/*
 * lesson5.c
 * ESP-NOW受信 → グラフ表示 + シリアル出力（M5Stack側）
 * lesson4.cをベースにシリアル出力機能を追加
 */

 #include <M5Stack.h>
 #include <WiFi.h>
 #include <esp_now.h>
 #include <esp_wifi.h>
 
 static const int SERIAL_BAUD = 115200;
 const int CHANNEL = 1;   // Senderと一致
 
 // ==== lesson4 で追加 ====
 // グラフ描画用の設定
 const int GRAPH_HISTORY_SIZE = 30;  // 履歴データの数
 const int GRAPH_X_START = 0;        // グラフのX開始位置
 const int GRAPH_Y_START = 65;       // グラフのY開始位置
 const int GRAPH_WIDTH = 320;        // グラフの幅
 const int GRAPH_HEIGHT = 40;        // グラフの高さ
 
 // データ履歴を保持する配列（各メトリクスごと）
 float tempHistory[GRAPH_HISTORY_SIZE];
 float humHistory[GRAPH_HISTORY_SIZE];
 float presHistory[GRAPH_HISTORY_SIZE];
 int historyIndex = 0;
 bool historyFilled = false;
 // ========================
 
 typedef struct __attribute__((packed)) {
     char id[4];
     float t;
     float h;
     float p;
     uint32_t seq;
 } Payload;
 
 static uint32_t recvCount = 0;
 static char lastId[4] = "---";
 static float lastT = 0, lastH = 0, lastP = 0;
 static uint8_t gatewayMac[6] = {0, 0, 0, 0, 0, 0};  // ゲートウェイのMACアドレスを保存
 
 // ==== lesson4 で追加 ====
 // グラフを描画する関数
 void drawGraph(float* history, int color, float minVal, float maxVal, int yOffset) {
     // グラフ領域をクリア
     M5.Lcd.fillRect(GRAPH_X_START, GRAPH_Y_START + yOffset, GRAPH_WIDTH, GRAPH_HEIGHT, BLACK);
     
     // グラフの枠線を描画
     M5.Lcd.drawRect(GRAPH_X_START, GRAPH_Y_START + yOffset, GRAPH_WIDTH, GRAPH_HEIGHT, WHITE);
     
     int dataCount = historyFilled ? GRAPH_HISTORY_SIZE : historyIndex;
     if (dataCount == 0) return;
     
     // 値の範囲を少し広げる
     float range = maxVal - minVal;
     if (range < 1.0) range = 1.0;
     minVal -= range * 0.1;
     maxVal += range * 0.1;
     
     // グラフの線を描画
     if (dataCount > 1) {
         for (int i = 0; i < dataCount - 1; i++) {
             int x1 = GRAPH_X_START + 5 + (i * (GRAPH_WIDTH - 10) / (dataCount - 1));
             int y1 = GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 5 - ((history[i] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
             
             int x2 = GRAPH_X_START + 5 + ((i + 1) * (GRAPH_WIDTH - 10) / (dataCount - 1));
             int y2 = GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 5 - ((history[i + 1] - minVal) * (GRAPH_HEIGHT - 10) / (maxVal - minVal));
             
             // 範囲チェック
             if (y1 < GRAPH_Y_START + yOffset + 5) y1 = GRAPH_Y_START + yOffset + 5;
             if (y1 > GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 5) y1 = GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 5;
             if (y2 < GRAPH_Y_START + yOffset + 5) y2 = GRAPH_Y_START + yOffset + 5;
             if (y2 > GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 5) y2 = GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 5;
             
             M5.Lcd.drawLine(x1, y1, x2, y2, color);
         }
     }
     
     // 最小値と最大値を表示
     M5.Lcd.setTextSize(1);
     M5.Lcd.setTextColor(CYAN);
     M5.Lcd.setCursor(GRAPH_X_START + 2, GRAPH_Y_START + yOffset + GRAPH_HEIGHT - 10);
     M5.Lcd.printf("%.1f", minVal);
     M5.Lcd.setCursor(GRAPH_X_START + 2, GRAPH_Y_START + yOffset + 2);
     M5.Lcd.printf("%.1f", maxVal);
     M5.Lcd.setTextSize(2);
 }
 
 // 履歴データの最大値・最小値を計算
 void getMinMax(float* history, float& minVal, float& maxVal) {
     minVal = 999;
     maxVal = -999;
     int dataCount = historyFilled ? GRAPH_HISTORY_SIZE : historyIndex;
     
     for (int i = 0; i < dataCount; i++) {
         float val = history[i];
         if (val < minVal) minVal = val;
         if (val > maxVal) maxVal = val;
     }
     
     if (dataCount == 0) {
         minVal = 0;
         maxVal = 100;
     }
 }
 
 // グラフとラベルを描画する関数
 // history: 履歴データ配列へのポインタ
 // color: グラフの色
 // label: ラベル文字列
 // yOffset: Y方向のオフセット（場所）
 // 参加者ミッション: この関数を使って、気圧や湿度のグラフも表示できるように変更してみましょう
 void drawGraphWithLabel(float* history, int color, const char* label, int yOffset) {
     // ラベルを表示
     M5.Lcd.setTextColor(color);
     M5.Lcd.setCursor(0, GRAPH_Y_START + yOffset - 15);
     M5.Lcd.println(label);
     
     // 最大値・最小値を計算
     float minVal, maxVal;
     getMinMax(history, minVal, maxVal);
     
     // グラフを描画
     drawGraph(history, color, minVal, maxVal, yOffset);
 }
 // ========================
 
 void onRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
     if (len != (int)sizeof(Payload)) return;
     Payload pl;
     memcpy(&pl, incomingData, sizeof(pl));
 
     char macStr[18];
     snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
 
     // ==== lesson5 で追加 ====
     // 受信したデータをJSON形式でシリアルポートに出力（PCに送信）
     // 参加者ミッション: シリアル通信について学ぶ（COMポートの設定が人によって違う）
     Serial.printf(
         "{\"type\":\"sample\",\"id\":\"%s\",\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"seq\":%u,\"from\":\"%s\"}\n",
         pl.id, pl.t, pl.h, pl.p, pl.seq, macStr
     );
     // ========================
 
     // ==== lesson4 で追加 ====
     // 履歴データに追加
     tempHistory[historyIndex] = pl.t;
     humHistory[historyIndex] = pl.h;
     presHistory[historyIndex] = pl.p;
     historyIndex++;
     if (historyIndex >= GRAPH_HISTORY_SIZE) {
         historyIndex = 0;
         historyFilled = true;
     }
     // ========================
 
     recvCount++;
     strncpy(lastId, pl.id, sizeof(lastId));
     lastT = pl.t;
     lastH = pl.h;
     lastP = pl.p;
 }
 
 void setup() {
     M5.begin();
     Serial.begin(SERIAL_BAUD);
     delay(200);
 
     M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setCursor(0, 0);
     M5.Lcd.println("ESP-NOW Gateway");
     M5.Lcd.printf("CH:%d\n", CHANNEL);
 
     // ===== 動作確認済みコード流儀 =====
     WiFi.mode(WIFI_STA);
     ESP_ERROR_CHECK(esp_wifi_start());
     delay(50);
     ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));
 #ifdef USE_WIFI_LR
     ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));
 #endif
     WiFi.setSleep(false);
     // =================================
 
     WiFi.macAddress(gatewayMac);
     M5.Lcd.printf("MAC:\n%02x:%02x:%02x:%02x:%02x:%02x\n",
                   gatewayMac[0], gatewayMac[1], gatewayMac[2], gatewayMac[3], gatewayMac[4], gatewayMac[5]);
 
     // ==== lesson5 で追加 ====
     // 起動時にゲートウェイの情報をシリアルポートに出力
     Serial.printf(
         "{\"type\":\"gateway_boot\",\"channel\":%d,\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\n",
         CHANNEL, gatewayMac[0], gatewayMac[1], gatewayMac[2], gatewayMac[3], gatewayMac[4], gatewayMac[5]
     );
     // ========================
 
     ESP_ERROR_CHECK(esp_now_init());
     esp_now_register_recv_cb(onRecv);
 
     // ==== lesson4 で追加 ====
     // 履歴配列の初期化
     for (int i = 0; i < GRAPH_HISTORY_SIZE; i++) {
         tempHistory[i] = 0;
         humHistory[i] = 0;
         presHistory[i] = 0;
     }
     // ========================
 
     M5.Lcd.println("Ready.");
 }
 
 void loop() {
     M5.update();
 
     // ==== lesson4 で変更 ====
     // 画面表示を更新
     M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(0, 0);
     // MACアドレスを表示
     M5.Lcd.setTextColor(CYAN);
     M5.Lcd.setTextSize(1);
     M5.Lcd.printf("MAC:%02x:%02x:%02x:%02x:%02x:%02x\n",
                   gatewayMac[0], gatewayMac[1], gatewayMac[2], gatewayMac[3], gatewayMac[4], gatewayMac[5]);
     
     M5.Lcd.setTextColor(WHITE);
     M5.Lcd.println("-----------------");
     M5.Lcd.printf("Recv:%lu\n", (unsigned long)recvCount);
     M5.Lcd.printf("Last:%s\n", lastId);
     M5.Lcd.printf("T:%.1fC H:%.1f%%\n", lastT, lastH);
     M5.Lcd.printf("P:%.1fhPa\n", lastP);
     
     // ==== lesson4 で追加 ====
     // グラフを描画
     int graphYOffset = 0;
     
    // 参加者ミッション: drawGraphWithLabelに値（履歴データ）と場所（yOffset）を渡す
     drawGraphWithLabel(tempHistory, RED, "Temp:", graphYOffset);
     graphYOffset += GRAPH_HEIGHT + 20;
 
     // ========================
 
     delay(200);
 }
 