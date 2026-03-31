#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SH110X.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "private_config.h"

// I2C пины ESP32
#define I2C_SDA 21
#define I2C_SCL 22

#define BUTTON_PIN 13

// Работа с яркостью дисплея
unsigned long lastActivity = 0;
int displayMode = 2;  // 2-ярко, 1-тускло, 0-выкл

const long DIM_AFTER = 15000;
const long OFF_AFTER = 20000;

// Объявление функции
void updateDisplay();

// Wi-Fi настройки
const char *ssid = SECRET_SSID;           // Название вашей сети
const char *password = SECRET_PASS; // Пароль сети

// Настройки связи с сервером
const char* apiToken = SECRET_TOKEN;
String serverUrl = "http://" + String(SECRET_SERVER_IP) + ":8742/update";

// Создание объекты
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire);

// Подключение к NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 36000, 60000); // GMT+10 для Владивостока (10*3600 = 36000)

// Переменные для обновления времени/сенсоров
float globalTemp = 0;
float globalHum = 0;
float globalPress = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastSensorUpdate = 0;

const long timeInterval = 1000;
const long sensorInterval = 5000;

void sendDataTask(void * pvParameters) {
  for(;;) {
    vTaskDelay(600000 / portTICK_PERIOD_MS);
    
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-type", "application/json");

      JsonDocument doc;
      doc["token"] = apiToken;
      doc["temp"] = globalTemp;
      doc["hum"] = globalHum;
      doc["press"] = globalPress;

      String requestBody;
      serializeJson(doc, requestBody);

      int httpResponseCode = http.POST(requestBody);

      http.end();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Инициализация OLED (адрес 0x3C)
  if(!display.begin(0x3C, true)) {
    Serial.println("OLED не найден!");
    while(1);
  }

  // Инициализация кнопки
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastActivity = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("MeteoStation");
  display.display();
  delay(2000);
  
  // Инициализация AHT20
  if(!aht.begin()) {
    Serial.println("AHT20 не найден!");
    while(1);
  }
  
  // Инициализация BMP280 (адрес 0x77)
  if(!bmp.begin(0x77)) {
    Serial.println("BMP280 не найден!");
    while(1);
  }

  bmp.setSampling(
    Adafruit_BMP280::MODE_FORCED,
    Adafruit_BMP280::SAMPLING_X1,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );
  
  // Подключение к Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi подключен!");
  
  // Запуск клиента времени
  timeClient.begin();
  
  Serial.println("Все датчики готовы!");

  xTaskCreatePinnedToCore(
    sendDataTask,
    "DataSender",
    10000,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  unsigned long currentMillis = millis();

  // Логика работы кнопки
  if (digitalRead(BUTTON_PIN) == LOW) {
    lastActivity = currentMillis;
    if (displayMode != 2) {
      display.oled_command(SH110X_DISPLAYON);
      display.setContrast(255);
      displayMode = 2;
    }
    delay(200);
  }

  // Таймеры экрана
  unsigned long diff = currentMillis - lastActivity;

  if (diff >= OFF_AFTER && displayMode != 0) {
    display.oled_command(SH110X_DISPLAYOFF);
    displayMode = 0;
  }
  else if (diff >= DIM_AFTER && displayMode == 2) {
    display.setContrast(1);
    displayMode = 1;
  }

  // Обновление экрана
  if (currentMillis - lastTimeUpdate >= 1000 && displayMode > 0) {
    lastTimeUpdate = currentMillis;
    updateDisplay(); 
  }

  // Обновление времени
  if (currentMillis - lastTimeUpdate >= timeInterval) {
    lastTimeUpdate = currentMillis;
    
    timeClient.update();

    updateDisplay(); 
  }
  
  // Чтение датчиков
  if (currentMillis - lastSensorUpdate >= sensorInterval) {
    lastSensorUpdate = currentMillis;

    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);

    bmp.takeForcedMeasurement();

    globalTemp = temp.temperature;
    globalHum = humidity.relative_humidity;
    globalPress = bmp.readPressure() / 100.0F;

    Serial.println("Данные датчиков обновлены");
  }
}

// Вывод на OLED
void updateDisplay() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(timeClient.getFormattedTime()); // Время
  
  display.setTextSize(1);
  display.setCursor(0, 12);
  display.print("Meteostation");

  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print("------");

  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Temp: ");
  display.print(globalTemp, 1);
  display.print(" C");
  
  display.setCursor(0, 44);
  display.print("Hum:  ");
  display.print(globalHum, 0);
  display.print(" %");
  
  display.setCursor(0, 56);
  display.print("Press:");
  display.print(globalPress, 0);
  display.print(" hPa");
  
  display.display();
}