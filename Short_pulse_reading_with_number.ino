int target_cnt = 0;
unsigned long previousMillis = 0;    // Переменная для хранения времени последнего выполнения
unsigned long startMillisTarget = 0;
const long interval = 200; 
const long targetinterval = 2000; //Значение времени для высокого значения на targetPin

const byte targetPin = 10;// Пин, к которому подключен источник напряжения
const byte interruptPin1 = A0; // Пин, к которому подключен источник напряжения
const byte interruptPin2 = A1; // Пин, к которому подключен источник напряжения
const byte interruptPin3 = A2; // Пин, к которому подключен источник напряжения
volatile int pulseCounter1 = 0; // Переменная, которая изменяется в прерывании (должна быть volatile)
volatile int pulseCounter2 = 0;
volatile int pulseCounter3 = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Мьютекс для безопасного доступа к переменной из loop()

void IRAM_ATTR handleInterrupt1() {
  // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
  portENTER_CRITICAL_ISR(&mux);
  pulseCounter1++;
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR handleInterrupt2() {
  // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
  portENTER_CRITICAL_ISR(&mux);
  pulseCounter2++;
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR handleInterrupt3() {
  // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
  portENTER_CRITICAL_ISR(&mux);
  pulseCounter3++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(9600);
  Serial.println("Ожидание кратковременного напряжения (импульса) на пине GPIO 9, 8, 7..");

  // Настройка пинов как входа с внутренним подтягивающим резистором
  pinMode(interruptPin1, INPUT_PULLUP);
  pinMode(interruptPin2, INPUT_PULLUP);
  pinMode(interruptPin3, INPUT_PULLUP);
  pinMode(targetPin, OUTPUT);
  digitalWrite(targetPin, LOW);

  // Прикрепление функции прерывания к пину
  // FALLING - срабатывает при переходе из HIGH в LOW (при нажатии кнопки в данной схеме)
  attachInterrupt(digitalPinToInterrupt(interruptPin1), handleInterrupt1, FALLING);
  attachInterrupt(digitalPinToInterrupt(interruptPin2), handleInterrupt2, FALLING);
  attachInterrupt(digitalPinToInterrupt(interruptPin3), handleInterrupt3, FALLING);
}

void loop() {
  int target_sub_number = 0;
  unsigned long currentMillis = millis(); 
    
  if (pulseCounter1 > 0) {
    // Безопасное чтение переменной из прерывания
    portENTER_CRITICAL(&mux);
    int currentCount1 = pulseCounter1;
    pulseCounter1 = 0;
    portEXIT_CRITICAL(&mux);
    // Serial.print("Обнаружен импульс1! Всего импульсов: ");
    // Serial.println(currentCount1);
    target_sub_number = 1;    
    }

    if (pulseCounter2 > 0) {
    // Безопасное чтение переменной из прерывания
    portENTER_CRITICAL(&mux);
    int currentCount2 = pulseCounter2;
    pulseCounter2 = 0;
    portEXIT_CRITICAL(&mux);
    // Serial.print("Обнаружен импульс2! Всего импульсов: ");
    // Serial.println(currentCount2);
    target_sub_number = 2;    
    }

    if (pulseCounter3 > 0) {
    // Безопасное чтение переменной из прерывания
    portENTER_CRITICAL(&mux);
    int currentCount3 = pulseCounter3;
    pulseCounter3 = 0;
    portEXIT_CRITICAL(&mux);
    // Serial.print("Обнаружен импульс3! Всего импульсов: ");
    // Serial.println(currentCount3);
    target_sub_number = 3;    
    }

    if (target_sub_number) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        target_cnt++;
        previousMillis = currentMillis;
        Serial.print("Попаданий=");
        Serial.print(target_cnt);
        switch (target_sub_number) {
          case 1:
            Serial.println(" Альфа");
            break;
          case 2:
            Serial.println(" Чарли"); 
            break; 
          case 3:
            Serial.println(" Дельта"); 
            break; 
        }
        digitalWrite(targetPin, HIGH);// Включаем пин
        startMillisTarget = millis();
        }
    }

    if (digitalRead(targetPin)) {
      unsigned long currentMillisTarget = millis();
      if (currentMillisTarget - startMillisTarget  > targetinterval) {
        digitalWrite(targetPin, LOW);// Выключаем пин target
         }
    }
  // Serial.print("TargetPin=");  
  // Serial.print(digitalRead(targetPin));
  // Serial.print(" ");
  // Serial.print("Попаданий=");
  // Serial.println(target_cnt);
}