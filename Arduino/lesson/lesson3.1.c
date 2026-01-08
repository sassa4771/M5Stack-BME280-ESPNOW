/*
 * gateway_espnow_serial.ino
 * ESP-NOW受信 → 1行JSONをSerialへ出力
 */

 // 必要なライブラリを読み込む
 #include <M5Stack.h>   // Core2なら <M5Core2.h> に変更
 #include <WiFi.h>
 #include <esp_now.h>
 #include <esp_wifi.h>
 
 static const int SERIAL_BAUD = 115200;
 const int CHANNEL = 1;   // 送信側（Sender）と同じチャンネルに設定
 
 // 受信するデータの構造体（送信側と同じ形式）
 typedef struct __attribute__((packed)) {
   char id[4];      // デバイスID
   float t;         // 温度
   float h;         // 湿度
   float p;         // 気圧
   uint32_t seq;    // シーケンス番号
 } Payload;
 
 // 受信したデータを保存する変数
 static uint32_t recvCount = 0; // 受信回数
 static char lastId[4] = "---";  // 最後に受信したデバイスID
 static float lastT=0, lastH=0, lastP=0; // 最後に受信した値
 
 // データを受信したときに呼ばれる関数
 void onRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
   // データサイズが正しいか確認
   if (len != (int)sizeof(Payload)) return;
   Payload pl;
   memcpy(&pl, incomingData, sizeof(pl)); // 受信データをコピー
 
   // 送信元のMACアドレスを文字列に変換
   char macStr[18];
   snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

   // 受信したデータをJSON形式でシリアル出力（PCに送信）
   Serial.printf(
     "{\"type\":\"sample\",\"id\":\"%s\",\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"seq\":%u,\"from\":\"%s\"}\n",
     pl.id, pl.t, pl.h, pl.p, pl.seq, macStr
   );

   // 受信回数を増やして、最後の値を保存
   recvCount++;
   strncpy(lastId, pl.id, sizeof(lastId));
   lastT = pl.t; lastH = pl.h; lastP = pl.p;
 }
 
 void setup() {
   M5.begin();
   Serial.begin(SERIAL_BAUD);
   delay(200);

   // 画面の初期設定
   M5.Lcd.fillScreen(BLACK);
   M5.Lcd.setTextSize(2);
   M5.Lcd.setCursor(0, 0);
   M5.Lcd.println("ESP-NOW Gateway");
   M5.Lcd.printf("CH:%d\n", CHANNEL);

   // WiFiとESP-NOWの初期化
   WiFi.mode(WIFI_STA); // ステーションモード
   ESP_ERROR_CHECK( esp_wifi_start() );
   delay(50);
   ESP_ERROR_CHECK( esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) ); // チャンネル設定
 #ifdef USE_WIFI_LR
   ESP_ERROR_CHECK( esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR) );
 #endif
   WiFi.setSleep(false); // 省電力モードを無効化

   // 自分のMACアドレスを取得して表示
   uint8_t mac[6];
   WiFi.macAddress(mac);
   M5.Lcd.printf("MAC:\n%02x:%02x:%02x:%02x:%02x:%02x\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

   // 起動情報をシリアル出力
   Serial.printf(
     "{\"type\":\"gateway_boot\",\"channel\":%d,\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\n",
     CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
   );

   // ESP-NOWを開始して受信コールバックを登録
   ESP_ERROR_CHECK( esp_now_init() );
   esp_now_register_recv_cb(onRecv);

   M5.Lcd.println("Ready.");
 }
 
 void loop() {
   M5.update(); // ボタンなどの状態を更新

   // 受信したデータを画面に表示
   M5.Lcd.setCursor(0, 140);
   M5.Lcd.printf("Recv:%lu   \n", (unsigned long)recvCount); // 受信回数
   M5.Lcd.printf("Last:%s    \n", lastId); // 最後のデバイスID
   M5.Lcd.printf("T:%.1fC    \n", lastT);  // 最後の温度
   M5.Lcd.printf("H:%.1f%%    \n", lastH);  // 最後の湿度
   M5.Lcd.printf("P:%.1fhPa  \n", lastP);  // 最後の気圧

   delay(200); // 0.2秒待つ
 }
 