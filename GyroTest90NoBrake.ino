#include <Wire.h>
#include <Servo.h>
#include <MPU6050_light.h>

// Определение пинов
const byte CH1_PIN = 2;   // Канал 1 (Прерывание) -> Поворот мишени вокруг оси
const byte CH2_PIN = 3;   // Канал 2 (Прерывание) -> Разрешение работы гироскопа / Режимов
const byte CH3_PIN = 4;   // Канал 4 аппаратуры -> Случайный поворот на 93 градуса
const byte ESC_PIN = 9;   // Пин управления ESC (ШИМ)

const int MOTOR_STOP = 1500; 
const int MOTOR_MIN = 1000;
const int MOTOR_MAX = 2000;

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

// --- ПЕРЕМЕННЫЕ ДЛЯ ТАЙМЕРА ТОРМОЖЕНИЯ ---
bool stickInNeutral = true;
unsigned long brakeStartTime = 0; 
bool isBraking = false;
// Длительность активного торможения (в миллисекундах). 
// 400 мс обычно хватает, чтобы полностью остановить вращение по инерции.
const unsigned long BRAKE_DURATION = 100; 

// --- НАСТРОЙКИ ПЛАВНОГО ПОВОРОТА ---
double finalTargetAngle = 0.0; 
const double TURN_SPEED_DEG_PER_SEC = 75.0; 

MPU6050 mpu(Wire);
Servo esc;

void rx_ch1();
void rx_ch2();
void tracking(int current_gas);

void setup() {
  Serial.begin(115200); 
  
  Wire.begin();
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

  attachInterrupt(digitalPinToInterrupt(CH1_PIN), rx_ch1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), rx_ch2, CHANGE);
  
  PCICR |= (1 << PCIE2);     
  PCMSK2 |= (1 << PCINT20);  

  mpu.update();
  targetAngle = mpu.getAngleZ(); 
  finalTargetAngle = targetAngle; 
  
  lastTime = micros();
  systemReady = true; 
}

void loop() {
  if (!systemReady) return;

  int channel1, channel2, channel3;
  
  noInterrupts();
  channel1 = ch1_val;
  channel2 = ch2_val;
  channel3 = ch3_val;
  interrupts();
  
  int esc_signal = MOTOR_STOP; 
  
  // =========================================================================
  // РЕЖИМ 1: Ручное управление (Канал 2 < 1500)
  // =========================================================================
  if (channel2 < 1500) {
    if (isGyro) {
      isGyro = false;
      isRotated = false;
      isBraking = false;
      Serial.println(F("Режим: Ручное управление"));
    }
    
    // Проверяем, в нейтрали ли стик
    if (channel1 > 1470 && channel1 < 1530) {
      
      // Если стик ТОЛЬКО ЧТО отпустили — запускаем таймер тормоза
      if (!stickInNeutral) {
        stickInNeutral = true;
        mpu.update();
        targetAngle = mpu.getAngleZ();   // Запоминаем точку, где отпустили стик
        finalTargetAngle = targetAngle; 
        integral = 0;                    
        lastError = 0;
        
        brakeStartTime = millis();       // Засекаем время старта торможения
        isBraking = true;
        Serial.println(F("🛑 Стик в нейтрали: Активное торможение гироскопом..."));
      }
      
      // Если торможение еще идет по таймеру
      if (isBraking) {
        if (millis() - brakeStartTime < BRAKE_DURATION) {
          tracking(MOTOR_STOP); // Активно гасим инерцию с помощью PID
        } else {
          isBraking = false;    // Время вышло — выключаем тормоз
          Serial.println(F("⏸️ Торможение завершено: Мотор отключен"));
          esc.writeMicroseconds(MOTOR_STOP); // Полный стоп (мотор расслаблен)
        }
      } else {
        // Тормоз уже отработал, мишень стоит на месте, гироскоп её НЕ удерживает
        esc.writeMicroseconds(MOTOR_STOP);
      }
      
    } else {
      // Стик отклонен — пользователь вращает мишень, тормоз сбрасывается
      stickInNeutral = false;
      isBraking = false;
      esc_signal = constrain(channel1, MOTOR_MIN, MOTOR_MAX);
      esc.writeMicroseconds(esc_signal);
    }

  // =========================================================================
  // РЕЖИМ 2: Автоматическая стабилизация курса (Канал 2 >= 1500)
  // =========================================================================
  } else {
    stickInNeutral = true; 
    isBraking = false;
    
    if (!isGyro) {
      Serial.println(F("Режим: Стабилизация включена"));
      
      if (Wire.getWireTimeoutFlag()) Wire.clearWireTimeoutFlag();
      
      mpu.update();
      targetAngle = mpu.getAngleZ(); 
      finalTargetAngle = targetAngle; 
      lastTime = micros(); 
      isGyro = true;
      isRotated = false; 
      integral = 0;
      lastError = 0;
    }
    
    // Логика триггера случайного поворота на 93 градуса
    if (channel3 > 1550) {
      if (!isRotated) {
        preTurnAngle = finalTargetAngle; 
        int direction = (random(0, 2) == 0) ? -93 : 93; 
        finalTargetAngle = preTurnAngle + direction; 
        isRotated = true;
        
        Serial.print(F("Случайный поворот на: "));
        Serial.print(direction);
        Serial.print(F(" градусов. Финальная цель: "));
        Serial.println(finalTargetAngle);
      }
    } else if (channel3 < 1500) { 
      if (isRotated) {
        finalTargetAngle = preTurnAngle; 
        isRotated = false;
        Serial.print(F("Возврат к исходному углу: "));
        Serial.println(finalTargetAngle);
      }
    }

    tracking(MOTOR_STOP); // В режиме 2 стабилизация работает бесконечно
  }
}

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

void tracking(int current_gas) {
  if (Wire.getWireTimeoutFlag()) {
    Serial.println(F("⚠️ Сбой I2C! Восстановление..."));
    Wire.clearWireTimeoutFlag();
    Wire.begin(); 
    esc.writeMicroseconds(current_gas); 
    return;
  }

  mpu.update();
  unsigned long currentTime = micros();
  double dt = (currentTime - lastTime) / 1000000.0;
  
  if (dt < 0.005) return; 
  lastTime = currentTime;

  if (targetAngle < finalTargetAngle) {
    targetAngle += TURN_SPEED_DEG_PER_SEC * dt;
    if (targetAngle > finalTargetAngle) targetAngle = finalTargetAngle; 
  } else if (targetAngle > finalTargetAngle) {
    targetAngle -= TURN_SPEED_DEG_PER_SEC * dt;
    if (targetAngle < finalTargetAngle) targetAngle = finalTargetAngle; 
  }

  double currentAngle = mpu.getAngleZ(); 
  if (isnan(currentAngle) || isinf(currentAngle)) return;

  error = targetAngle - currentAngle; 
  
  if (abs(error) < 15.0) {
    integral += error * dt;
    double maxIntegralContribution = 150.0 / Ki; 
    integral = constrain(integral, -maxIntegralContribution, maxIntegralContribution); 
  } else {
    integral = 0; 
  }

  derivative = (error - lastError) / dt;
  lastError = error;

  double controlOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);

  if (abs(error) > 0.5) {
    if (controlOutput > 0) controlOutput += ESC_DEADBAND_COMP;
    else if (controlOutput < 0) controlOutput -= ESC_DEADBAND_COMP;
  }

  int gas_offset = current_gas - MOTOR_STOP;
  int pwmSignal = MOTOR_STOP + gas_offset + (int)controlOutput;
  pwmSignal = constrain(pwmSignal, MOTOR_MIN, MOTOR_MAX); 

  esc.writeMicroseconds(pwmSignal);

  if (millis() - lastSerialTime > 50) {
    lastSerialTime = millis();
    Serial.print(F("FinalTarget:")); Serial.print(finalTargetAngle); Serial.print(F(" "));
    Serial.print(F("Target:")); Serial.print(targetAngle); Serial.print(F(" "));
    Serial.print(F("Current:")); Serial.print(currentAngle); Serial.print(F(" "));
    Serial.print(F("PWM:")); Serial.println(pwmSignal);
  }
}
