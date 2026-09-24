#include <Arduino.h>

// --- Конфигурация пинов ---
#define HEAD_PIN   25
#define BODY_PIN   27
#define SIGNAL_PIN 21

// --- Зоны попадания ---
constexpr int ZONE_HEAD = 1;
constexpr int ZONE_BODY = 2;

// --- Переменные для прерываний ---
volatile bool head_hit = false;
volatile bool body_hit  = false;

// --- Спинлок ---
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

// --- Блокировка мишени ---
volatile unsigned long last_hit_time = 0;
volatile bool is_locked = false;
constexpr unsigned long LOCK_DURATION_MS = 5000;

// --- Сигнальный пин ---
unsigned long signal_start_time = 0;
bool signal_active = false;
constexpr unsigned long SIGNAL_DURATION_MS = 2000;

// --- Счётчик ---
uint32_t total_hits = 0;

// --- Общий обработчик попадания ---
void IRAM_ATTR registerHit(volatile bool& hit_flag) {
  portENTER_CRITICAL_ISR(&myMutex);
  if (!is_locked) {
    hit_flag = true;
    is_locked = true;
    last_hit_time = millis();
  }
  portEXIT_CRITICAL_ISR(&myMutex);
}

void IRAM_ATTR headISR() { registerHit(head_hit); }
void IRAM_ATTR bodyISR() { registerHit(body_hit); }

void setup() {
  Serial.begin(9600);

  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);

  // Внешняя подтяжка 1 кОм — внутренняя не нужна
  pinMode(HEAD_PIN, INPUT);
  pinMode(BODY_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(HEAD_PIN), headISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BODY_PIN), bodyISR, FALLING);

  Serial.println("Система двухзонной мишени №1 запущена и готова.");
}

void loop() {
  unsigned long current_millis = millis();

  bool local_head_hit = false;
  bool local_body_hit = false;

  portENTER_CRITICAL(&myMutex);

  if (is_locked && (current_millis - last_hit_time >= LOCK_DURATION_MS)) {
    is_locked = false;
  }

  if (head_hit) {
    local_head_hit = true;
    head_hit = false;
    body_hit = false; // Защита: приоритет головы (фактически недостижимо из-за is_locked)
  } else if (body_hit) {
    local_body_hit = true;
    body_hit = false;
  }

  portEXIT_CRITICAL(&myMutex);

  int hit_zone = 0;
  if (local_head_hit) hit_zone = ZONE_HEAD;
  else if (local_body_hit) hit_zone = ZONE_BODY;

  if (hit_zone > 0) {
    total_hits++;
    Serial.print("1-");
    Serial.print(total_hits);
    Serial.print("-");
    Serial.println(hit_zone);

    digitalWrite(SIGNAL_PIN, HIGH);
    signal_start_time = current_millis;
    signal_active = true;
  }

  if (signal_active && (current_millis - signal_start_time >= SIGNAL_DURATION_MS)) {
    digitalWrite(SIGNAL_PIN, LOW);
    signal_active = false;
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}
