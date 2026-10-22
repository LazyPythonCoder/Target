#include <Wire.h>
#include <Servo.h>
#include <MPU6050_light.h>

// Определение пинов
const byte CH1_PIN = 2;   // Канал 1 (Прерывание) -> Управляет ESC (Газ / Базовое движение)
const byte CH2_PIN = 3;   // Канал 2 (Прерывание) -> Разрешение работы гироскопа
const byte CH3_PIN = 4;   // Канал 4 аппаратуры -> Поворот на 90 градусов (PinChangeInterrupt)
const byte ESC_PIN = 9;   // Пин управления ESC (ШИМ)

const int MOTOR_STOP = 1500; 
const int MOTOR_MIN = 1000;
const int MOTOR_MAX = 2000;

// НАСТРОЙКА: Минимальный импульс, при котором мотор РЕАЛЬНО начинает крутиться.
constexpr int ESC_DEADBAND_COMP = 30; 

// Коэффициенты PID-регулятора
double Kp = 3.5;
double Ki = 0.5; 
double Kd = 0.1;

// Переменные для PID
double targetAngle = 0.0; 
double error = 0.0, lastError = 0.0;
double integral = 0.0, derivative = 0.0;
unsigned long lastTime = 0;
unsigned long lastSerialTime = 0; 

// Переменные для хранения времени импульса (микросекунды)
volatile unsigned long ch1_start = 0;
volatile int ch1_val = 1500; 

volatile unsigned long ch2_start = 0;
volatile int ch2_val = 1500; 

volatile unsigned long ch3_start = 0;
volatile int ch3_val = 1000; 

bool isGyro = false;
bool isRotated = false;    
double preTurnAngle = 0.0; 
bool systemReady = false;   

MPU6050 mpu(Wire);
Servo esc;

void rx_ch1();
void rx_ch2();
void tracking(int current_gas);

void setup() {
  Serial.begin(115200); 
  
  // Инициализация шины I2C
  Wire.begin();
  
  // ЗАЩИТА ОТ ЗАВИСАНИЯ: Если MPU6050 не ответит в течение 3000 мкс (из-за помех от мотора),
  // библиотека Wire автоматически сбросит флаг блокировки и продолжит выполнение кода.
  Wire.setWireTimeout(3000, true); 
  
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  pinMode(CH3_PIN, INPUT);
  
  randomSeed(analogRead(A0));
  
  esc.attach(ESC_PIN, 1000, 2000);
  esc.writeMicroseconds(MOTOR_STOP);
  delay(2000); 

  Serial.println(F("Инициализация MPU6050..."));
  byte status = mpu.begin();
  if (status != 0) {
    Serial.print(F("Ошибка датчика! Статус: "));
    Serial.println(status);
    while (1); 
  }
  
  Serial.println(F("Калибровка гироскопа. НЕ ДВИГАЙТЕ ТЕЛЕЖКУ..."));
  delay(1000);
  mpu.calcOffsets(); 
  Serial.println(F("Калибровка завершена!"));
  delay(500);

  // Настройка аппаратных прерываний
  attachInterrupt(digitalPinToInterrupt(CH1_PIN), rx_ch1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), rx_ch2, CHANGE);
  
  // Настройка Pin Change Interrupt для D4 (Порт D, PCINT20)
  PCICR |= (1 << PCIE2);     
  PCMSK2 |= (1 << PCINT20);  

  mpu.update();
  targetAngle = mpu.getAngleZ(); 
  
  lastTime = micros();
  systemReady = true; 
}

void loop() {
  if (!systemReady) return;

  int channel1, channel2, channel3;
  
  // Атомарное безопасное копирование данных из прерываний
  noInterrupts();
  channel1 = ch1_val;
  channel2 = ch2_val;
  channel3 = ch3_val;
  interrupts();
  
  int esc_signal = MOTOR_STOP; 
  
  // РЕЖИМ 1: Ручное управление (Тумблер выключен)
  if (channel2 < 1500) {
    if (isGyro) {
      isGyro = false;
      isRotated = false;
      Serial.println(F("Режим: Ручное управление"));
      integral = 0;
      lastError = 0;
    }
    
    // Мертвая зона для стика пульта в ручном режиме
    if (channel1 > 1470 && channel1 < 1530) {
      esc_signal = MOTOR_STOP;
    } else {
      esc_signal = constrain(channel1, MOTOR_MIN, MOTOR_MAX);
    }
    esc.writeMicroseconds(esc_signal);

  // РЕЖИМ 2: Стабилизация курса (Тумблер включен)
  } else {
    if (!isGyro) {
      Serial.println(F("Режим: Стабилизация включена"));
      
      // Сброс таймаутов перед стартом
      if (Wire.getWireTimeoutFlag()) Wire.clearWireTimeoutFlag();
      
      mpu.update();
      targetAngle = mpu.getAngleZ(); 
      lastTime = micros(); 
      isGyro = true;
      isRotated = false; 
      integral = 0;
      lastError = 0;
    }
    
    // Логика триггера случайного поворота на 93 градуса
    if (channel3 > 1550) {
      if (!isRotated) {
        preTurnAngle = targetAngle; 
        int direction = (random(0, 2) == 0) ? -93 : 93; 
        targetAngle = preTurnAngle + direction; 
        isRotated = true;
        
        Serial.print(F("Случайный поворот на: "));
        Serial.print(direction);
        Serial.print(F(" градусов. Новая цель: "));
        Serial.println(targetAngle);
      }
    } else if (channel3 < 1500) { 
      if (isRotated) {
        targetAngle = preTurnAngle; 
        isRotated = false;
        Serial.print(F("Возврат к исходному углу: "));
        Serial.println(targetAngle);
      }
    }

    // Передаем текущий газ для смешивания с коррекцией гироскопа
    tracking(MOTOR_STOP); 
  }
}

// --- ОБРАБОТЧИКИ ПРЕРЫВАНИЙ (С защитой от ложных срабатываний и дребезга) ---

void rx_ch1() {
  unsigned long current_time = micros();
  if (digitalRead(CH1_PIN) == HIGH) {
    ch1_start = current_time;
  } else {
    if (ch1_start > 0) {
      int pulse_width = current_time - ch1_start;
      if (pulse_width >= 800 && pulse_width <= 2200) {
        ch1_val = pulse_width;
      }
      ch1_start = 0; 
    }
  }
}

void rx_ch2() {
  unsigned long current_time = micros();
  if (digitalRead(CH2_PIN) == HIGH) {
    ch2_start = current_time;
  } else {
    if (ch2_start > 0) {
      int pulse_width = current_time - ch2_start;
      if (pulse_width >= 800 && pulse_width <= 2200) {
        ch2_val = pulse_width;
      }
      ch2_start = 0;
    }
  }
}

ISR(PCINT2_vect) {
  unsigned long current_time = micros();
  if (digitalRead(CH3_PIN) == HIGH) {
    ch3_start = current_time;
  } else {
    if (ch3_start > 0) {
      int pulse_width = current_time - ch3_start;
      if (pulse_width >= 800 && pulse_width <= 2200) {
        ch3_val = pulse_width;
      }
      ch3_start = 0;
    }
  }
}

// --- ФУНКЦИЯ АВТОМАТИЧЕСКОГО УДЕРЖАНИЯ КУРСА (PID) ---

void tracking(int current_gas) {
  // Проверка аппаратного сбоя шины I2C
  if (Wire.getWireTimeoutFlag()) {
    Serial.println(F("⚠️ Сбой I2C из-за наводок мотора! Мягкий перезапуск..."));
    Wire.clearWireTimeoutFlag();
    Wire.begin(); // Реанимируем шину
    // На один цикл оставляем газ с пульта без PID-коррекции для безопасности
    esc.writeMicroseconds(current_gas); 
    return;
  }

  mpu.update();
  unsigned long currentTime = micros();
  
  // Переводим dt в секунды
  double dt = (currentTime - lastTime) / 1000000.0;
  
  // Ограничиваем частоту работы PID до ~200 Гц (раз в 5 миллисекунд)
  if (dt < 0.005) return; 
  lastTime = currentTime;

  double currentAngle = mpu.getAngleZ(); 
  
  // Защита от получения поврежденных данных (NaN/Inf) во время сбоя
  if (isnan(currentAngle) || isinf(currentAngle)) return;

  error = targetAngle - currentAngle; 
  
  // Улучшенный Anti-windup (интеграл работает только в зоне точного доворота)
  if (abs(error) < 15.0) {
    integral += error * dt;
    double maxIntegralContribution = 150.0 / Ki; 
    integral = constrain(integral, -maxIntegralContribution, maxIntegralContribution); 
  } else {
    integral = 0; // Отключаем накопление при резком развороте на 93 градуса
  }

  derivative = (error - lastError) / dt;
  lastError = error;

  double controlOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);

  // --- КОМПЕНСАЦИЯ МЕРТВОЙ ЗОНЫ ESC ---
  if (abs(error) > 0.5) {
    if (controlOutput > 0) controlOutput += ESC_DEADBAND_COMP;
    else if (controlOutput < 0) controlOutput -= ESC_DEADBAND_COMP;
  }

  // Расчет отклонения газа от нейтрали (1500)
  int gas_offset = current_gas - MOTOR_STOP;
  
  // Микширование: Центр + Сигнал Хода + Выход регулятора угла
  int pwmSignal = MOTOR_STOP + gas_offset + (int)controlOutput;
  pwmSignal = constrain(pwmSignal, MOTOR_MIN, MOTOR_MAX); 

  esc.writeMicroseconds(pwmSignal);

  // Вывод телеметрии в Serial Monitor / Plotter (частота 20 Гц)
  if (millis() - lastSerialTime > 50) {
    lastSerialTime = millis();
    Serial.print(F("Target:")); Serial.print(targetAngle); Serial.print(F(" "));
    Serial.print(F("Current:")); Serial.print(currentAngle); Serial.print(F(" "));
    Serial.print(F("Gas:")); Serial.print(current_gas); Serial.print(F(" "));
    Serial.print(F("PWM:")); Serial.println(pwmSignal);
  }
}
