#include <Arduino.h>

// --- Конфигурация пинов GPIO ---
#define HEAD_PIN 25   // Вход: зона "Голова"
#define BODY_PIN 27   // Вход: зона "Туловище"
#define SIGNAL_PIN 21 // Выход: индикация попадания (HIGH на 2 секунды)

// --- Переменные для прерываний (volatile) ---
volatile bool head_hit = false;
volatile bool body_hit = false;

// --- Спинлок для безопасного многоядерного доступа ESP32 ---
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

// --- Переменные глобальной блокировки (в миллисекундах) ---
volatile unsigned long last_hit_time = 0; // Время последнего засчитанного попадания
volatile bool is_locked = false;          // Флаг активной блокировки мишени
const unsigned long LOCK_DURATION_MS = 5000; // Время слепоты мишени — 5 секунд

// --- Переменные управления сигнальным пином GPIO21 ---
unsigned long signal_start_time = 0;
bool signal_active = false;
const unsigned long SIGNAL_DURATION_MS = 2000; // Длительность сигнала — 2 секунды

// --- Общий сквозной счетчик попаданий ---
unsigned int total_hits = 0;

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ГОЛОВЫ (в IRAM памяти)
// ========================================================
void IRAM_ATTR headISR() {
  unsigned long current_millis = millis();
  
  portENTER_CRITICAL_ISR(&myMutex);
  // Проверяем блокировку прямо внутри критической секции прерывания
  if (!is_locked || (current_millis - last_hit_time >= LOCK_DURATION_MS)) { 
    head_hit = true;
    is_locked = true; // Сразу же блокируем новые попадания на уровне прерываний
    last_hit_time = current_millis;
  }
  portEXIT_CRITICAL_ISR(&myMutex);
}

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ТУЛОВИЩА (в IRAM памяти)
// ========================================================
void IRAM_ATTR bodyISR() {
  unsigned long current_millis = millis();
  
  portENTER_CRITICAL_ISR(&myMutex);
  // Проверяем блокировку прямо внутри критической секции прерывания
  if (!is_locked || (current_millis - last_hit_time >= LOCK_DURATION_MS)) { 
    body_hit = true;
    is_locked = true; // Сразу же блокируем новые попадания на уровне прерываний
    last_hit_time = current_millis;
  }
  portEXIT_CRITICAL_ISR(&myMutex);
}

// ========================================================
// ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
// ========================================================
void setup() {
  Serial.begin(115200);
  
  // Настройка сигнального пина
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);
  
  // Настройка входов мишени (требуются внешние подтяжки 1 кОм и конденсаторы 1 нФ!)
  pinMode(HEAD_PIN, INPUT_PULLUP);
  pinMode(BODY_PIN, INPUT_PULLUP);
  
  // Привязка аппаратных прерываний строго на падение уровня (FALLING)
  attachInterrupt(digitalPinToInterrupt(HEAD_PIN), headISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BODY_PIN), bodyISR, FALLING);
  
  Serial.println("Система двухзонной мишени №1 запущена и готова.");
}

// ========================================================
// ОСНОВНОЙ ЦИКЛ ОБРАБОТКИ
// ========================================================
void loop() {
  unsigned long current_millis = millis();
  
  bool local_head_hit = false;
  bool local_body_hit = false;

  // --- АТОМАРНАЯ СЕКЦИЯ КОПИРОВАНИЯ ---
  portENTER_CRITICAL(&myMutex);
  
  // Автоматический сброс флага блокировки по истечении времени
  if (is_locked && (current_millis - last_hit_time >= LOCK_DURATION_MS)) {
    is_locked = false;
  }
  
  if (head_hit) { local_head_hit = true; head_hit = false; }
  // Если зафиксировано попадание в голову, игнорируем туловище в этот же миг
  if (body_hit && !local_head_hit) { local_body_hit = true; body_hit = false; }
  else if (body_hit) { body_hit = false; } // Очищаем дублирующий флаг
  
  portEXIT_CRITICAL(&myMutex);
  
  // --- ОБРАБОТКА ПОПАДАНИЯ В ГОЛОВУ ---
  if (local_head_hit) {
    total_hits++; 
    
    // Вывод сообщения: 1-А-Б (1 - мишень №1, А - всего попаданий, Б - 1 для головы)
    Serial.print("1-");
    Serial.print(total_hits);
    Serial.println("-1");
    
    // Активируем сигнальный пин GPIO21
    digitalWrite(SIGNAL_PIN, HIGH);
    signal_start_time = current_millis;
    signal_active = true;
  }
  
  // --- ОБРАБОТКА ПОПАДАНИЯ В ТУЛОВИЩЕ ---
  if (local_body_hit) {
    total_hits++; 
    
    // Вывод сообщения: 1-А-Б (1 - мишень №1, А - всего попаданий, Б - 2 для туловища)
    Serial.print("1-");
    Serial.print(total_hits);
    Serial.println("-2");
    
    // Активируем сигнальный пин GPIO21
    digitalWrite(SIGNAL_PIN, HIGH);
    signal_start_time = current_millis;
    signal_active = true;
  }
  
  // --- АСИНХРОННОЕ ВЫКЛЮЧЕНИЕ СИГНАЛЬНОГО ПИНА GPIO21 ---
  if (signal_active && (current_millis - signal_start_time >= SIGNAL_DURATION_MS)) {
    digitalWrite(SIGNAL_PIN, LOW);
    signal_active = false;
  }
  
  // Даем FreeRTOS перевести дыхание (предотвращает срабатывание Watchdog)
  vTaskDelay(pdMS_TO_TICKS(1)); 
}

