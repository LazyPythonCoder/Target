#include <WiFi.h>
#include <esp_now.h>

// НАСТРОЙКА ПЬЕЗОДАТЧИКА
#define PIEZO_PIN 34       // Аналоговый пин (используйте GPIO 32-39, так как они работают при Wi-Fi)
#define THRESHOLD 3000     // Порог срабатывания (от 0 до 4095)
#define DEBOUNCE_MS 500    // Пауза 0.5 секунды для затухания колебаний

// Укажите MAC-адрес вашего ПРИЕМНИКА
uint8_t receiverAddress[] = {0x6C, 0xC8, 0x40, 0x56, 0x6E, 0x68};

// Структура данных для отправки (должна совпадать на приемнике)
typedef struct struct_message {
    char message[32];      // Буфер для хранения строки "Попадание"
} struct_message;

struct_message sendData;
esp_now_peer_info_t peerInfo;

// Callback-функция отправки (сохраняем вашу рабочую сигнатуру)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    Serial.print("Статус отправки пакета: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Успешно" : "Ошибка доставки");
}

void setup() {
    Serial.begin(115200);
    
    // Настраиваем пин пьезоэлемента на вход
    pinMode(PIEZO_PIN, INPUT);

    // Включаем Wi-Fi в режиме Station
    WiFi.mode(WIFI_STA);

    // Инициализация ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Ошибка инициализации ESP-NOW");
        return;
    }

    // Регистрация callback-функции контроля отправки
    esp_now_register_send_cb(OnDataSent);
    
    // Регистрация принимающего устройства (Peer)
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;  // Оставляем ваш рабочий вариант канала    
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Не удалось добавить устройство-приемник");
        return;
    }

    Serial.println("Система запущена. Мониторинг вибрации...");
}

void loop() {
    // Непрерывно считываем текущее значение вибрации
    int currentVal = analogRead(PIEZO_PIN);

    // Постоянный вывод текущего значения в Serial
    Serial.print("Вибрация: ");
    Serial.println(currentVal);

    // Проверка превышения порога
    if (currentVal > THRESHOLD) {
        // Выводим событие в Serial
        Serial.println("!!! ПОПАДАНИЕ !!!");

        // Формируем текстовое сообщение
        strcpy(sendData.message, "Попадание");

        // Мгновенная отправка структуры через ESP-NOW
        esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &sendData, sizeof(sendData));
        
        if (result != ESP_OK) {
            Serial.println("Критическая ошибка при вызове функции отправки");
        }

        // Блокируем опрос на 0.5 секунды, чтобы физические колебания датчика утихли
        delay(DEBOUNCE_MS); 
    }

    // Небольшая задержка, чтобы не перегружать Serial-порт (около 100 чтений в секунду)
    delay(10); 
}
