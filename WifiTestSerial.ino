#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Настройки Wi-Fi точки доступа
const char* ssid = "ESP32_Sensor_AP";
const char* password = "12345678"; // Минимум 8 символов

// Создаем TCP-сервер на порту 8023
WiFiServer server(8023);
WiFiClient client;

Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;

// Порог для резкого металлического удара (в м/с²).
const float IMPACT_THRESHOLD = 40.0; 

// Время блокировки после удара (в мс) для гашения вибрации ("звона") металла
const unsigned long DEBOUNCE_TIME = 300; 
unsigned long lastImpactTime = 0;

void setup() {
  Serial.begin(115200);

  // Настройка ESP32 в режим точки доступа
  WiFi.softAP(ssid, password);

  // Вывод IP-адреса в Serial для контроля
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Запуск TCP-сервера
  server.begin();
  Serial.println("Server started. Waiting for PuTTY connection...");

  // Повышаем скорость шины I2C до 400 кГц для быстрого опроса датчиков
  Wire.begin(21, 22, 400000); 

  if (!mpu1.begin(0x68)) {
    Serial.println("Ошибка: Датчик 1 (0x68) не найден!");
    while (1) delay(10);
  }

  if (!mpu2.begin(0x69)) {
    Serial.println("Ошибка: Датчик 2 (0x69) не найден!");
    while (1) delay(10);
  }

  // Настройки для регистрации резких пиковых ускорений
  mpu1.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu2.setAccelerometerRange(MPU6050_RANGE_16_G);
  
  mpu1.setFilterBandwidth(MPU6050_BAND_260_HZ);
  mpu2.setFilterBandwidth(MPU6050_BAND_260_HZ);

  Serial.println("Система запущена. Ожидание удара...");
}

void loop() {
  // Поддерживаем подключение PuTTY в фоновом режиме
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("PuTTY connected!");
    }
  }

  sensors_event_t a1, g1, temp1;
  sensors_event_t a2, g2, temp2;

  // Быстрое считывание данных
  mpu1.getEvent(&a1, &g1, &temp1);
  mpu2.getEvent(&a2, &g2, &temp2);

  // Получаем модуль ускорения по оси Z
  float accelZ1 = abs(a1.acceleration.z);
  float accelZ2 = abs(a2.acceleration.z);

  // Проверяем факт превышения порога
  bool hit1 = (accelZ1 > IMPACT_THRESHOLD);
  bool hit2 = (accelZ2 > IMPACT_THRESHOLD);

  // Если зафиксировано превышение порога ХОТЯ БЫ на одном датчике
  if (hit1 || hit2) {
    // Проверяем, прошло ли время блокировки дребезга
    if ((millis() - lastImpactTime) > DEBOUNCE_TIME) {
      
      // Формируем базовую строку с показаниями
      String msg = "Событие! Датчик1: " + String(accelZ1, 2) + " м/с², Датчик2: " + String(accelZ2, 2) + " м/с² -> ";

      // Логика обработки результата
      if (hit1 && !hit2) {
        msg += "РЕЗУЛЬТАТ: УДАР\r\n";
      } 
      else if (hit1 && hit2) {
        msg += "ИГНОР (Оба одновременно)\r\n";
      }
      else {
        msg += "ИГНОР (Только датчик 2)\r\n";
      }

      // Выводим в Serial для локальной отладки
      Serial.print(msg);

      // Мгновенная отправка данных в PuTTY (если программа подключена)
      if (client && client.connected()) {
        client.print(msg);
      }

      // Обновляем время последнего события
      lastImpactTime = millis();
    }
  }

  // Минимальная задержка цикла для высокой частоты сканирования
  delay(1); 
}
