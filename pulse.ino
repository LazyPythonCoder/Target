int target_cnt = 0;
unsigned long previousMillis = 0;    // Переменная для хранения времени последнего выполнения
const long interval = 100; 

const long intervalTarget = 2000; 

unsigned long startTimeTarget = 0;


const byte interruptPin = 27; // Пин, к которому подключен источник напряжения
const byte targetPin = 26; 
volatile int pulseCounter = 0; // Переменная, которая изменяется в прерывании (должна быть volatile)
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Мьютекс для безопасного доступа к переменной из loop()

void IRAM_ATTR handleInterrupt() {
  // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
  portENTER_CRITICAL_ISR(&mux);
  pulseCounter++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Ожидание кратковременного напряжения (импульса) на пине GPIO 27...");

  // Настройка пина как входа с внутренним подтягивающим резистором
  pinMode(interruptPin, INPUT_PULLUP);
  pinMode(targetPin, OUTPUT);
  digitalWrite(targetPin, LOW);

  // Прикрепление функции прерывания к пину
  // FALLING - срабатывает при переходе из HIGH в LOW (при нажатии кнопки в данной схеме)
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
}

void loop() {
  bool target_done = false;
    
  if (pulseCounter > 0) {
    // Безопасное чтение переменной из прерывания
    portENTER_CRITICAL(&mux);
    int currentCount = pulseCounter;
    pulseCounter = 0;
    portEXIT_CRITICAL(&mux);

    target_done = true;
   
    Serial.print("Обнаружен импульс! Всего импульсов: ");
    Serial.println(currentCount);
   
    }

    if (target_done) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        target_cnt++;
        digitalWrite(targetPin, HIGH);
        startTimeTarget = millis();
        previousMillis = currentMillis;
        Serial.print("Попаданий=");
        Serial.println(target_cnt);
      }
    }
  // Здесь может выполняться другой код
    if (digitalRead(targetPin) == HIGH) {
      unsigned long now = millis();
      if (now - startTimeTarget > intervalTarget) {
         digitalWrite(targetPin, LOW);
      }
    }

    Serial.print("targetPin=");
    Serial.println(digitalRead(targetPin));
}
