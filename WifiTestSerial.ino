#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Настройки создаваемой Wi-Fi сети
const char* ap_ssid     = "ESP32_GY521_Net"; // Имя сети, которую создаст ESP32
const char* ap_password = "12345678"; // Пароль (минимум 8 символов)

// Создаем TCP-сервер на порту 23 (Telnet)
WiFiServer server(23);
WiFiClient client;

// Датчики GY-521
Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;
const uint8_t MPU1_ADDR = 0x68;
const uint8_t MPU2_ADDR = 0x69;

const float THRESHOLD = 10.0;       
const unsigned long DURATION = 100;  

void setup() {
  Serial.begin(115200);
  delay(10);

  // Настройка ESP32 в режим Точки Доступа (Access Point)
  Serial.println("Запуск точки доступа...");
  WiFi.softAP(ap_ssid, ap_password);

  // Выводим информацию о созданной сети
  Serial.print("Сеть создана: ");
  Serial.println(ap_ssid);
  Serial.print("IP-адрес для подключения в PuTTY: ");
  Serial.println(WiFi.softAPIP()); // Обычно это 192.168.4.1

  // Запуск TCP-сервера
  server.begin();

  // Инициализация датчиков
  Wire.begin(21, 22);
  
  if (!mpu1.begin(MPU1_ADDR, &Wire) || !mpu2.begin(MPU2_ADDR, &Wire)) {
    Serial.println("Ошибка инициализации датчиков!");
    while (1) delay(10);
  }
  mpu1.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu2.setAccelerometerRange(MPU6050_RANGE_16_G);
  
  Serial.println("Система готова к работе.");
}

void loop() {
  // Проверяем подключение клиента (PuTTY)
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("PuTTY успешно подключился по Wi-Fi!");
      client.println("Подключение установлено. Ожидание данных...");
    }
  }

  float maxZ1 = 0;
  float maxZ2 = 0;
  sensors_event_t a1, g1, temp1;
  sensors_event_t a2, g2, temp2;

  unsigned long startTime = millis();

  // Сбор данных 50 мс
  while (millis() - startTime < DURATION) {
    mpu1.getEvent(&a1, &g1, &temp1);
    mpu2.getEvent(&a2, &g2, &temp2);

    float absZ1 = abs(a1.acceleration.z);
    float absZ2 = abs(a2.acceleration.z);

    if (absZ1 > maxZ1) maxZ1 = absZ1;
    if (absZ2 > maxZ2) maxZ2 = absZ2;
  }

  bool p1_exceeded = (maxZ1 > THRESHOLD);
  bool p2_exceeded = (maxZ2 > THRESHOLD);

  // Проверка порогов и отправка
  if (p1_exceeded || p2_exceeded) {
    String message = "";

    if (p1_exceeded && p2_exceeded) {
      message += "Два датчика | ";
    } else if (p1_exceeded) {
      message += "Удар | ";
    } else if (p2_exceeded) {
      message += "Превышен порог на втором датчике | ";
    }

    message += "Макс Z1: " + String(maxZ1) + " м/с², Макс Z2: " + String(maxZ2) + " м/с²";

    // Лог в локальный Serial (для отладки по проводу)
    Serial.println(message);

    // Беспроводная отправка в PuTTY
    if (client && client.connected()) {
      client.println(message);
    }
  }

  delay(10);
}
