#include <WiFi.h>
#include <esp_now.h>

// Переменная-счетчик попаданий
int hitCount = 0;

// Структура данных (должна полностью совпадать с передатчиком)
typedef struct struct_message {
    char message[32];    // Буфер на 32 байта, как мы настроили ранее
} struct_message;

struct_message incomingReadings;

// Callback-функция приема
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    // Копируем входящие байты в нашу структуру
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    
    // Проверяем, что пришло именно "Попадание" (сравниваем строки)
    if (strcmp(incomingReadings.message, "Попадание") == 0) {
        hitCount++; // Увеличиваем счетчик на 1
        
        Serial.println("=========================");
        Serial.print("Получено событие: ");
        Serial.println(incomingReadings.message); 
        
        // Выводим актуальное количество попаданий
        Serial.print("ВСЕГО ПОПАДАНИЙ: ");
        Serial.println(hitCount);
        Serial.println("=========================");
    }
}

void setup() {
    Serial.begin(115200);
    
    // Включаем Wi-Fi в режиме Station
    WiFi.mode(WIFI_STA);
    delay(500);
    
    // Вывод MAC-адреса в монитор порта
    Serial.print("MAC-адрес этого приемника: ");
    Serial.println(WiFi.macAddress());
    Serial.println("Ожидание приема вибрации...");

    // Инициализация ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Ошибка ESP-NOW");
        return;
    }
    
    // Регистрация callback-функции приема
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
    // В loop() по-прежнему ничего делать не нужно
}
