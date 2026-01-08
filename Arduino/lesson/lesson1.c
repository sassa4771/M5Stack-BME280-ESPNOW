// 必要なライブラリを読み込む
#include <Wire.h>
#include <M5StickCPlus.h> // Plus用ライブラリ
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// BME280センサーを使うための準備
Adafruit_BME280 bme;

// センサーの値を保存する変数
float temp = 0; //温度の値
float hum = 0;  //湿度の値
float pres = 0; //圧力の値

void setup() {
    // M5StickC Plusの初期化
    M5.begin();
    // シリアル通信の開始（PCと通信するため）
    Serial.begin(115200);

    // I2C通信の初期化（センサーと通信するため）
    // SDA=32, SCL=33はM5StickC PlusのGroveポートのピン番号
    Wire.begin(32, 33);

    // BME280センサーが見つかるか確認
    // 見つからない場合はエラーメッセージを表示して停止
    if (!bme.begin(0x76, &Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        M5.Lcd.println("Sensor Error");
        while (1); // ここでプログラムが止まる
    }

    // ディスプレイの初期設定
    M5.Lcd.setRotation(1);    // 横向き
    M5.Lcd.setTextSize(2);    // 文字サイズ
    M5.Lcd.fillScreen(BLACK); // 画面クリア
    M5.Lcd.println("BME280 Init OK");
    delay(1000);
}

void loop() {
    // センサーから値を読み取る
    temp = bme.readTemperature();          // 温度を取得（℃）
    hum  = bme.readHumidity();             // 湿度を取得（%）
    pres = bme.readPressure() / 100.0F;    // 気圧を取得（hPaに変換）
    
    // 画面を黒でクリアしてから表示
    M5.Lcd.fillScreen(BLACK); 
    M5.Lcd.setCursor(0, 0); // 文字の表示位置を左上に設定
    
    // タイトルを黄色で表示
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.println("Env Measure");
    M5.Lcd.println("-----------------");
    
    // センサーの値を白色で表示
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.printf("Temp: %.2f C\n", temp);
    M5.Lcd.printf("Hum:  %.2f %%\n", hum);
    M5.Lcd.printf("Pres: %.2f hPa\n", pres);

    delay(1000); // 1秒待ってから次のループへ
}