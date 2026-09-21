// Конфигурация пинов GPIO
#define HEAD_PIN 25   // GPIO для зоны "Голова"
#define BODY_PIN 27   // GPIO для зоны "Туловище"
#define SIGNAL_PIN 21 // Вывод индикации попадания

// Флаги попаданий для обработки в основном цикле
volatile bool head_hit = false;
volatile bool body_hit = false;

// Таймеры для глобальной блокировки системы после выстрела (в миллисекундах)
unsigned long global_lock_until = 0; 
const unsigned long LOCK_DURATION_MS = 5000; // Время слепоты мишени — 5 секунд

// Общий счетчик попаданий (голова + туловище)
unsigned int total_hits = 0;

// Таймеры для управления сигнальным пином GPIO21 (в миллисекундах)
unsigned long signal_off_time = 0;
bool signal_active = false;

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ГОЛОВЫ
// ========================================================
void IRAM_ATTR headISR() {
  unsigned long current_millis = millis();
  
  // Если 5 секунд с момента последнего засчитанного попадания еще не прошли — игнорируем
  if (current_millis >= global_lock_until) { 
    head_hit = true;
  }
}

// ========================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЛЯ ТУЛОВИЩА
// ========================================================
void IRAM_ATTR bodyISR() {
  unsigned long current_millis = millis();
  
  // Если 5 секунд с момента последнего засчитанного попадания еще не прошли — игнорируем
  if (current_millis >= global_lock_until) { 
    body_hit = true;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Конфигурация сигнального пина
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW); // Изначально LOW
  
  // Конфигурация входов мишени (требуется внешняя обвязка 1 кОм и 1 нФ!)
  pinMode(HEAD_PIN, INPUT_PULLUP);
  pinMode(BODY_PIN, INPUT_PULLUP);
  
  // Настройка прерываний на падение уровня (FALLING)
  attachInterrupt(digitalPinToInterrupt(HEAD_PIN), headISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BODY_PIN), bodyISR, FALLING);
  
  // Serial.println("Система мишени №1 с блокировкой на 5 секунд готова.");
}

void loop() {
  unsigned long current_millis = millis();
  
  // --- ОБРАБОТКА ПОПАДАНИЯ В ГОЛОВУ ---
  if (head_hit) {
    head_hit = false; // Сразу сбрасываем флаг прерывания
    
    // Двойная проверка на случай, если прерывание успело проскочить до обновления таймера
    if (current_millis >= global_lock_until) {
      // Устанавливаем «слепоту» мишени на 5 секунд вперед
      global_lock_until = current_millis + LOCK_DURATION_MS; 
      
      total_hits++; // Увеличиваем общий счетчик мишени
      
      // Вывод сообщения вида 1-А-Б (Б = 1 для головы)
      Serial.print("1-");
      Serial.print(total_hits);
      Serial.println("-1");
      
      // Включение сигнального пина на 2 секунды
      digitalWrite(SIGNAL_PIN, HIGH);
      signal_off_time = current_millis + 2000;
      signal_active = true;
    }
  }
  
  // --- ОБРАБОТКА ПОПАДАНИЯ В ТУЛОВИЩЕ ---
  if (body_hit) {
    body_hit = false; // Сразу сбрасываем флаг прерывания
    
    // Двойная проверка на случай, если прерывание успело проскочить до обновления таймера
    if (current_millis >= global_lock_until) {
      // Устанавливаем «слепоту» мишени на 5 секунд вперед
      global_lock_until = current_millis + LOCK_DURATION_MS; 
      
      total_hits++; // Увеличиваем общий счетчик мишени
      
      // Вывод сообщения вида 1-А-Б (Б = 2 для туловища)
      Serial.print("1-");
      Serial.print(total_hits);
      Serial.println("-2");
      
      // Включение сигнального пина на 2 секунды
      digitalWrite(SIGNAL_PIN, HIGH);
      signal_off_time = current_millis + 2000;
      signal_active = true;
    }
  }
  
  // --- АСИНХРОННОЕ ВЫКЛЮЧЕНИЕ СИГНАЛЬНОГО ПИНА Через 2 секунды ---
  if (signal_active && (current_millis >= signal_off_time)) {
    digitalWrite(SIGNAL_PIN, LOW);
    signal_active = false;
  }
}
