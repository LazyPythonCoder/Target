#include <WiFi.h>

// Определение пина и параметров замера
const int PIEZO_PIN = 35; 
const unsigned long MEASUREMENT_WINDOW = 300; 
const int TRIGGER_THRESHOLD = 50; 

// Настройки Wi-Fi точки доступа
const char *ssid = "Gong_Target_ESP32"; // Имя сети Wi-Fi
const char *password = "12345678";       // Пароль (минимум 8 символов)

// Настройка TCP-сервера на порту 23 (стандартный порт Telnet)
WiFiServer server(23);
WiFiClient client;

void setup() {
  Serial.begin(115200);
  // pinMode(PIEZO_PIN, INPUT);
  analogSetPinAttenuation(PIEZO_PIN, ADC_11db);
  // Настройка ESP32 как точки доступа
  Serial.println("Запуск точки доступа...");
  WiFi.softAP(ssid, password);

  // По умолчанию IP-адрес ESP32 в режиме AP: 192.168.4.1
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP-адрес: ");
  Serial.println(IP);

  // Запуск TCP-сервера
  server.begin();
  Serial.println("Сервер запущен. Ожидание подключения PuTTY...");
}

void loop() {
  // Проверяем, подключился ли новый клиент (PuTTY)
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("PuTTY успешно подключен!");
      client.println("--- Соединение установлено. Ожидание попаданий ---");
    }
  }

  // Считываем текущее значение с датчика
  int currentValue = analogRead(PIEZO_PIN);

  // Если сигнал превысил порог, фиксируем начало удара
  if (currentValue > TRIGGER_THRESHOLD) {
    int maxValue = currentValue; 
    unsigned long startTime = millis(); 

    // Цикл поиска максимума в течение 100 миллисекунд
    while (millis() - startTime < MEASUREMENT_WINDOW) {
      int sensorValue = analogRead(PIEZO_PIN);
      if (sensorValue > maxValue) {
        maxValue = sensorValue;
      }
    }

    // Формируем строку для отправки
    String message = "ПОПАДАНИЕ! Максимум за 300 мс: " + String(maxValue);

    // Вывод в локальную консоль ESP32 (через USB)
    Serial.println(message);

    // Отправка данных на компьютер по Wi-Fi (в PuTTY)
    if (client && client.connected()) {
      client.println(message); 
    }

    // Задержка для затухания вибрации
    delay(200); 
  }
}
