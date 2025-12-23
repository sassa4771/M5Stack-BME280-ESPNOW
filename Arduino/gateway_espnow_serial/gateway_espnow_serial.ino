/*
 * gateway_espnow_serial.ino
 * ESP-NOW受信 → 1行JSONをSerialへ出力
 */

#include <M5Stack.h>   // Core2なら <M5Core2.h> に変更
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static const int SERIAL_BAUD = 115200;
const int CHANNEL = 1;   // Senderと一致（まず1推奨）

typedef struct __attribute__((packed)) {
  char id[4];
  float t;
  float h;
  float p;
  uint32_t seq;
} Payload;

static uint32_t recvCount = 0;
static char lastId[4] = "---";
static float lastT=0, lastH=0, lastP=0;

void onRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != (int)sizeof(Payload)) return;
  Payload pl;
  memcpy(&pl, incomingData, sizeof(pl));

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf(
    "{\"type\":\"sample\",\"id\":\"%s\",\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"seq\":%u,\"from\":\"%s\"}\n",
    pl.id, pl.t, pl.h, pl.p, pl.seq, macStr
  );

  recvCount++;
  strncpy(lastId, pl.id, sizeof(lastId));
  lastT = pl.t; lastH = pl.h; lastP = pl.p;
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
  ESP_ERROR_CHECK( esp_wifi_start() );
  delay(50);
  ESP_ERROR_CHECK( esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) );
#ifdef USE_WIFI_LR
  ESP_ERROR_CHECK( esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR) );
#endif
  WiFi.setSleep(false);
  // =================================

  uint8_t mac[6];
  WiFi.macAddress(mac);
  M5.Lcd.printf("MAC:\n%02x:%02x:%02x:%02x:%02x:%02x\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf(
    "{\"type\":\"gateway_boot\",\"channel\":%d,\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\n",
    CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
  );

  ESP_ERROR_CHECK( esp_now_init() );
  esp_now_register_recv_cb(onRecv);

  M5.Lcd.println("Ready.");
}

void loop() {
  M5.update();

  M5.Lcd.setCursor(0, 140);
  M5.Lcd.printf("Recv:%lu   \n", (unsigned long)recvCount);
  M5.Lcd.printf("Last:%s    \n", lastId);
  M5.Lcd.printf("T:%.1fC    \n", lastT);
  M5.Lcd.printf("H:%.1f%%    \n", lastH);
  M5.Lcd.printf("P:%.1fhPa  \n", lastP);

  delay(200);
}
