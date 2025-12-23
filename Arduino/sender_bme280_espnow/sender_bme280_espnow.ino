/*
 * sender_bme280_espnow.ino
 * BME280 → ESP-NOW 送信（Gatewayへ）
 */

#include <M5Stack.h>   // Core2なら <M5Core2.h> に変更
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ===== 受講生が触るのはここだけ =====
const char DEVICE_ID[] = "B1";
uint8_t GATEWAY_MAC[6] = {0x98,0xf4,0xab,0x6e,0x51,0x10};
// 動作確認済みコードに合わせてまずは 1 推奨（6で詰まるなら特に）
const int CHANNEL = 1;
// ===================================

// 送信周期
const int SEND_INTERVAL_MS = 1000;

// BME280
const uint8_t BME_ADDR = 0x76;
const int I2C_SDA = 21;
const int I2C_SCL = 22;
Adafruit_BME280 bme;

typedef struct __attribute__((packed)) {
  char id[4];
  float t;
  float h;
  float p;
  uint32_t seq;
} Payload;

uint32_t seq = 0;

static void ensurePeer() {
  if (esp_now_is_peer_exist(GATEWAY_MAC)) {
    esp_now_del_peer(GATEWAY_MAC); // 再書き込み時のゴミを消す
  }
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, GATEWAY_MAC, 6);
  p.ifidx = WIFI_IF_STA;   // ★親コード準拠
  p.channel = CHANNEL;     // ★親コード準拠
  p.encrypt = false;

  esp_err_t e = esp_now_add_peer(&p);
  Serial.printf("[peer] add ret=%d\n", (int)e);
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  delay(200);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("ESP-NOW Sender");
  M5.Lcd.printf("ID:%s CH:%d\n", DEVICE_ID, CHANNEL);

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

  Serial.printf("My MAC: %s  CH=%d\n", WiFi.macAddress().c_str(), CHANNEL);

  ESP_ERROR_CHECK( esp_now_init() );
  ensurePeer();

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!bme.begin(BME_ADDR)) {
    M5.Lcd.println("BME280 not found");
    while (true) delay(1000);
  }

  M5.Lcd.println("Ready.");
}

void loop() {
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0f;

  Payload pl{};
  strncpy(pl.id, DEVICE_ID, sizeof(pl.id) - 1);
  pl.t = t; pl.h = h; pl.p = p; pl.seq = seq++;

  esp_err_t r = esp_now_send(GATEWAY_MAC, (uint8_t*)&pl, sizeof(pl));

  M5.Lcd.setCursor(0, 60);
  M5.Lcd.printf("T %.1fC   \n", t);
  M5.Lcd.printf("H %.1f%%   \n", h);
  M5.Lcd.printf("P %.1fhPa \n", p);
  M5.Lcd.printf("seq %lu   \n", (unsigned long)pl.seq);
  M5.Lcd.printf("send %d   \n", (int)r);

  delay(SEND_INTERVAL_MS);
}
