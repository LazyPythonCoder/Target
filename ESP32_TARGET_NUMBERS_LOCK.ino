#include <Arduino.h>

// --- Конфигурация пинов GPIO ---
#define HEAD_PIN 25   // Вход: зона "Голова"
#define BODY_PIN 27   // Вход: зона "Туловище"
#define SIGNAL_PIN 21 // Выход: индикация попадания (HIGH на 2 секунды)

// --- Переменные для прерываний (volatile) ---
volatile bool head_hit = false;
volatile bool body_hit = false;

// --- Объявление мьютекса для безопасного многоядерного доступа ---
// Это стандартный и самый надежный способ защиты памяти в ESP32
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

// --- Переменные глобальной блокировки и логики (в миллисекундах) ---
unsigned long global_lock_until = 0; 
const unsigned long LOCK_DURATION_MS = 5000; // Время слепоты мишени — 5 секунд

// --- Переменные управления сигнальным пином GPIO21 ---
unsigned long signal_off_time = 0;
bool signal_active = false;

// --- Общий сквозной счетчик попаданий ---
unsigned int total_hits = 0;

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ГОЛОВЫ (в IRAM памяти)
// ========================================================
void IRAM_ATTR headISR() {
  unsigned long current_millis = millis();
  
  if (current_millis >= global_lock_until) { 
    // Внутри ISR для изменения переменной используем ISR-версию критической секции
    portENTER_CRITICAL_ISR(&myMutex);
    head_hit = true;
    portEXIT_CRITICAL_ISR(&myMutex);
  }
}

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ТУЛОВИЩА (в IRAM памяти)
// ========================================================
void IRAM_ATTR bodyISR() {
  unsigned long current_millis = millis();
  
  if (current_millis >= global_lock_until) { 
    // Внутри ISR для изменения переменной используем ISR-версию критической секции
    portENTER_CRITICAL_ISR(&myMutex);
    body_hit = true;
    portEXIT_CRITICAL_ISR(&myMutex);
  }
}

// ========================================================
// ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
// ========================================================
void setup() {
  Serial.begin(115200);
  
  // Настройка сигнального пина
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW); // Изначально находится в режиме LOW
  
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
  
  // Локальные копии флагов для безопасной работы
  bool local_head_hit = false;
  bool local_body_hit = false;

  // --- АТОМАРНАЯ СЕКЦИЯ БЕЗОПАСНОСТИ ESP32 (Вместо noInterrupts) ---
  // Блокируем доступ к переменным для других потоков и ядер на доли микросекунды,
  // быстро копируем флаги и обнуляем оригиналы.
  portENTER_CRITICAL(&myMutex);
  if (head_hit) { local_head_hit = true; head_hit = false; }
  if (body_hit) { local_body_hit = true; body_hit = false; }
  portEXIT_CRITICAL(&myMutex);
  
  // --- ОБРАБОТКА ПОПАДАНИЯ В ГОЛОВУ ---
  if (local_head_hit) {
    if (current_millis >= global_lock_until) {
      global_lock_until = current_millis + LOCK_DURATION_MS; // Запираем мишень на 5 секунд
      total_hits++; 
      
      // Вывод сообщения: 1-А-Б (1 - мишень №1, А - всего попаданий, Б - 1 для головы)
      Serial.print("1-");
      Serial.print(total_hits);
      Serial.println("-1");
      
      // Активируем сигнальный пин GPIO21 на 2 секунды
      digitalWrite(SIGNAL_PIN, HIGH);
      signal_off_time = current_millis + 2000;
      signal_active = true;
    }
  }
  
  // --- ОБРАБОТКА ПОПАДАНИЯ В ТУЛОВИЩЕ ---
  if (local_body_hit) {
    if (current_millis >= global_lock_until) {
      global_lock_until = current_millis + LOCK_DURATION_MS; // Запираем мишень на 5 секунд
      total_hits++; 
      
      // Вывод сообщения: 1-А-Б (1 - мишень №1, А - всего попаданий, Б - 2 для туловища)
      Serial.print("1-");
      Serial.print(total_hits);
      Serial.println("-2");
      
      // Активируем сигнальный пин GPIO21 на 2 секунды
      digitalWrite(SIGNAL_PIN, HIGH);
      signal_off_time = current_millis + 2000;
      signal_active = true;
    }
  }
  
  // --- АСИНХРОННОЕ ВЫКЛЮЧЕНИЕ СИГНАЛЬНОГО ПИНА GPIO21 Через 2 секунды ---
  if (signal_active && (current_millis >= signal_off_time)) {
    digitalWrite(SIGNAL_PIN, LOW);
    signal_active = false;
  }
}
