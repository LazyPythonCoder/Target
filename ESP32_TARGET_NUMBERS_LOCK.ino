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

// --- Переменные глобальной блокировки ---
volatile unsigned long last_hit_time = 0;     // Время последнего засчитанного попадания
volatile bool is_locked = false;              // Флаг активной блокировки мишени
constexpr unsigned long LOCK_DURATION_MS = 5000; // Время слепоты мишени — 5 секунд

// --- Переменные управления сигнальным пином GPIO21 ---
unsigned long signal_start_time = 0;
bool signal_active = false;
constexpr unsigned long SIGNAL_DURATION_MS = 2000; // Длительность сигнала — 2 секунды

// --- Общий сквозной счетчик попаданий ---
unsigned int total_hits = 0;

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ГОЛОВЫ (в IRAM памяти)
// ========================================================
void IRAM_ATTR headISR() {
  portENTER_CRITICAL_ISR(&myMutex);
  if (!is_locked) { 
    head_hit = true;
    is_locked = true; // Мгновенная блокировка на аппаратном уровне прерываний
  }
  portEXIT_CRITICAL_ISR(&myMutex);
}

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ТУЛОВИЩА (в IRAM памяти)
// ========================================================
void IRAM_ATTR bodyISR() {
  portENTER_CRITICAL_ISR(&myMutex);
  if (!is_locked) { 
    body_hit = true;
    is_locked = true; // Мгновенная блокировка на аппаратном уровне прерываний
  }
  portEXIT_CRITICAL_ISR(&myMutex);
}

// ========================================================
// ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
// ========================================================
void setup() {
  Serial.begin(9600);
  
  // Настройка сигнального пина
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);
  
  // Настройка входов мишени (требуются внешние подтяжки 1 кОм и конденсаторы 1 нФ!)
  pinMode(HEAD_PIN, INPUT_PULLUP);
  pinMode(BODY_PIN, INPUT_PULLUP);
  
  // Привязка аппаратных прерываний строго на падение уровня (FALLING)
  attachInterrupt(digitalPinToInterrupt(HEAD_PIN), headISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BODY_PIN), bodyISR, FALLING);
  
  // Serial.println("Система двухзонной мишени №1 запущена и готова.");
}

// ========================================================
// ОСНОВНОЙ ЦИКЛ ОБРАБОТКИ
// ========================================================
void loop() {
  unsigned long current_millis = millis();
  
  bool local_head_hit = false;
  bool local_body_hit = false;
  int hit_zone = 0; // 0 - нет попадания, 1 - голова, 2 - туловище

  // --- АТОМАРНАЯ СЕКЦИЯ СИНХРОНИЗАЦИИ ---
  portENTER_CRITICAL(&myMutex);
  
  // Автоматический сброс флага блокировки по истечении времени
  if (is_locked && (current_millis - last_hit_time >= LOCK_DURATION_MS)) {
    is_locked = false;
  }
  
  // Если блокировки нет (или она только что снялась) и есть прерывание
  if (!is_locked) {
    if (head_hit) { 
      local_head_hit = true; 
      hit_zone = 1;
      is_locked = true;
      last_hit_time = current_millis; // Фиксируем точное время попадания в loop
    } 
    // Если голова не поражена, но есть туловище
    else if (body_hit) { 
      local_body_hit = true; 
      hit_zone = 2;
      is_locked = true;
      last_hit_time = current_millis; // Фиксируем точное время попадания в loop
    }
  }

  // В любом случае очищаем «сырые» флаги прерываний, так как система сейчас в режиме блокировки
  head_hit = false;
  body_hit = false;
  
  portEXIT_CRITICAL(&myMutex);
  
  // --- ОБРАБОТКА ПОПАДАНИЯ ---
  if (hit_zone > 0) {
    total_hits++; 
    
    // Унифицированный вывод сообщения: 1-А-Б (1 - мишень №1, А - всего, Б - зона)
    Serial.print("1-");
    Serial.print(total_hits);
    Serial.print("-");
    Serial.println(hit_zone);
    
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

