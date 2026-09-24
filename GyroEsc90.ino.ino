#include <Wire.h>
#include <Servo.h>
#include <MPU6050_light.h>

// Определение пинов
const byte CH1_PIN = 2;   // Канал 1 (Прерывание) -> Управляет ESC
const byte CH2_PIN = 3;   // Канал 2 (Прерывание) -> Разрешение работы гироскопа
const byte CH3_PIN = 4;   // Канал 4 аппаратуры -> Поворот на 90 градусов (PinChangeInterrupt)
const byte ESC_PIN = 9;   // Пин управления ESC (ШИМ)

const int MOTOR_STOP = 1500; 
const int MOTOR_MIN = 1000;
const int MOTOR_MAX = 2000;

// НАСТРОЙКА: Минимальный импульс, при котором мотор РЕАЛЬНО начинает крутиться.
// Для большинства ESC мертвая зона составляет около 20-40 мкс от нейтрали (1500).
constexpr int ESC_DEADBAND_COMP = 30; 

// Коэффициенты PID-регулятора (Увеличили Ki для ускорения доворота)
double Kp = 3.5;
double Ki = 0.5; // Было 0.2
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
void tracking();

void setup() {
  Serial.begin(115200); 
  Wire.begin();
  
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
  
  if (channel2 < 1500) {
    if (isGyro) {
      isGyro = false;
      isRotated = false;
      Serial.println(F("Режим: Ручное управление"));
      integral = 0;
      lastError = 0;
    }
    
    if (channel1 > 1470 && channel1 < 1530) {
      esc_signal = MOTOR_STOP;
    } else {
      esc_signal = constrain(channel1, MOTOR_MIN, MOTOR_MAX);
    }
    esc.writeMicroseconds(esc_signal);

  } else {
    if (!isGyro) {
      Serial.println(F("Режим: Стабилизация включена"));
      mpu.update();
      targetAngle = mpu.getAngleZ(); 
      lastTime = micros(); 
      isGyro = true;
      isRotated = false; 
      integral = 0;
      lastError = 0;
    }
    
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

    tracking(); 
  }
}

void rx_ch1() {
  unsigned long current_time = micros();
  if (digitalRead(CH1_PIN) == HIGH) {
    ch1_start = current_time;
  } else {
    int pulse_width = current_time - ch1_start;
    if (pulse_width >= 800 && pulse_width <= 2200) {
      ch1_val = pulse_width;
    }
  }
}

void rx_ch2() {
  unsigned long current_time = micros();
  if (digitalRead(CH2_PIN) == HIGH) {
    ch2_start = current_time;
  } else {
    int pulse_width = current_time - ch2_start;
    if (pulse_width >= 800 && pulse_width <= 2200) {
      ch2_val = pulse_width;
    }
  }
}

ISR(PCINT2_vect) {
  unsigned long current_time = micros();
  if (digitalRead(CH3_PIN) == HIGH) {
    ch3_start = current_time;
  } else {
    int pulse_width = current_time - ch3_start;
    if (pulse_width >= 800 && pulse_width <= 2200) {
      ch3_val = pulse_width;
    }
  }
}

void tracking() {
  mpu.update();
  unsigned long currentTime = micros();
  
  double dt = (currentTime - lastTime) / 1000000.0;
  if (dt < 0.005) return; 
  lastTime = currentTime;

  double currentAngle = mpu.getAngleZ(); 
  error = targetAngle - currentAngle; 
  
  integral += error * dt;
  double maxIntegralContribution = 400.0 / Ki;
  integral = constrain(integral, -maxIntegralContribution, maxIntegralContribution); 
  
  if ((error > 0 && lastError < 0) || (error < 0 && lastError > 0)) {
    integral = 0;
  }

  derivative = (error - lastError) / dt;
  lastError = error;

  double controlOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);

  // --- КОМПЕНСАЦИЯ МЕРТВОЙ ЗОНЫ ESC ---
  // Если ошибка существенна (> 0.5 градуса), "проталкиваем" ШИМ через слепую зону регулятора
  if (error > 0.5) {
    controlOutput += ESC_DEADBAND_COMP;
  } else if (error < -0.5) {
    controlOutput -= ESC_DEADBAND_COMP;
  }

  int pwmSignal = MOTOR_STOP + (int)controlOutput;
  pwmSignal = constrain(pwmSignal, MOTOR_MIN, MOTOR_MAX); 

  esc.writeMicroseconds(pwmSignal);

  if (millis() - lastSerialTime > 40) {
    lastSerialTime = millis();
    Serial.print(F("Target:")); Serial.print(targetAngle); Serial.print(F(" "));
    Serial.print(F("Current:")); Serial.print(currentAngle); Serial.print(F(" "));
    Serial.print(F("PWM:")); Serial.println(pwmSignal);
  }
}
