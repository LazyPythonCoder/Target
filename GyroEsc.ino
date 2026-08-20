#include <Wire.h>
#include <Servo.h>
#include <MPU6050_light.h>


// Определение пинов
const byte CH1_PIN = 2;   // Канал 1 (Прерывание 0) -> Управляет ESC
const byte CH2_PIN = 3;   // Канал 2 (Прерывание 1) -> Разрешение работы (Тумблер/Стик)
const byte ESC_PIN = 9;   // Пин управления ESC (ШИМ)
const int MOTOR_STOP = 1500; // Уровень остановки мотора
const int MOTOR_MIN = 1000;
const int MOTOR_MAX = 2000;

// Коэффициенты PID-регулятора (настройте под свою механику)
double Kp = 3.5;
double Ki = 0.2;
double Kd = 0.1;

// Переменные для PID
double targetAngle = 0.0; // Платформа стремится удерживать 0 градусов
double error = 0.0, lastError = 0.0;
double integral = 0.0, derivative = 0.0;
unsigned long lastTime = 0;

// Переменные для хранения времени импульса (микросекунды)
volatile unsigned long ch1_start = 0;
volatile int ch1_val = 1500; // Нейтраль по умолчанию

volatile unsigned long ch2_start = 0;
volatile int ch2_val = 1500; // Нейтраль по умолчанию

bool isGyro = false;

// Объект для управления ESC

MPU6050 mpu(Wire);
Servo esc;

void setup() {
  Serial.begin(115200); // Высокая скорость для вывода данных
  Wire.begin();
  
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  
  // Настройка внешних прерываний на любое изменение сигнала (CHANGE)
  attachInterrupt(digitalPinToInterrupt(CH1_PIN), rx_ch1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), rx_ch2, CHANGE);
  
  // Инициализация ESC с ограничением импульса от 1000 до 2000 мкс
  esc.attach(ESC_PIN, 1000, 2000);
  
  // Безопасность: подаем нейтральный сигнал (1500 мкс) при старте,
  // чтобы ESC успешно прошел инициализацию и не включил мотор сразу
  esc.writeMicroseconds(1500);
  delay(2000); // Пауза 2 секунды для калибровки/включения ESC

  Serial.println(F("Инициализация MPU6050..."));
  byte status = mpu.begin();
  if (status != 0) {
    Serial.print(F("Ошибка датчика! Статус: "));
    Serial.println(status);
    while (1); // Зависаем, если датчик не найден
  }
  Serial.println(F("Калибровка гироскопа. НЕ ДВИГАЙТЕ ТЕЛЕЖКУ..."));
  delay(1000);
  mpu.calcOffsets(); // Автоматический расчет смещения для всех осей
  Serial.println(F("Калибровка завершена!"));
  delay(1000);

  lastTime = micros();

}

void loop() {
  int channel1, channel2;
  
  // Безопасное копирование данных из прерываний (исключаем сбои при чтении)
  noInterrupts();
  channel1 = ch1_val;
  channel2 = ch2_val;
  interrupts();
  
  int esc_signal = 1500; // По умолчанию мотор остановлен (нейтраль)
  
  // --- ПРОВЕРКА РАЗРЕШЕНИЯ С 3-ГО ПИНА (CH2) ---
  // Управление доступно ТОЛЬКО если сигнал МЕНЬШЕ 1500
  if (channel2 < 1500) {
    isGyro = false;
    Serial.println("Гироскоп отключен");
    
    // Ограничиваем рабочий диапазон строго в пределах 1000-2000 мкс
    esc_signal = constrain(channel1, 1000, 2000);
    esc.writeMicroseconds(esc_signal);
    
    // Мертвая зона в центре (если стик около 1500, принудительно ставим 1500)
    if (esc_signal > 1470 && esc_signal < 1530) {
      esc_signal = 1500;
      esc.writeMicroseconds(esc_signal);
      Serial.println("Гироскоп отключен");
    }
  } else {
    // Если сигнал на пине 3 больше или равен 1500, начинаем отслеживать угол
     Serial.println("Гироскоп активирован");
    if (!isGyro) {
      targetAngle =  mpu.getAngleZ();
      isGyro = true;
    }
    tracking(); 
  }
     
  // --- ВЫВОД ДАННЫХ В КОНСОЛЬ ---
  // Serial.print("CH2 (Enable Pin): ");
  // Serial.print(channel2);
  // Serial.print(" us [");
  // Serial.print("]\t||\tCH1 (Input): ");
  // Serial.print(channel1);
  // Serial.print(" us -> ESC Out: ");
  // Serial.print(esc_signal);
  // Serial.println(" us");
  
  delay(20); // Частота обновления 50 Гц (стандарт для RC-аппаратуры)
}

// Обработчик прерывания для 1 канала (Пин 2)
void rx_ch1() {
  unsigned long current_time = micros();
  if (digitalRead(CH1_PIN) == HIGH) {
    ch1_start = current_time;
  } else {
    int pulse_width = current_time - ch1_start;
    // Отсекаем дикие всплески помех
    if (pulse_width >= 800 && pulse_width <= 2200) {
      ch1_val = pulse_width;
    }
  }
}

// Обработчик прерывания для 2 канала (Пин 3)
void rx_ch2() {
  unsigned long current_time = micros();
  if (digitalRead(CH2_PIN) == HIGH) {
    ch2_start = current_time;
  } else {
    int pulse_width = current_time - ch2_start;
    // Отсекаем дикие всплески помех
    if (pulse_width >= 800 && pulse_width <= 2200) {
      ch2_val = pulse_width;
    }
  }
}

//Обработчие треккинга гироскопа
void tracking() {
  mpu.update();
  unsigned long currentTime = micros();
  double dt = (currentTime - lastTime) / 1000000.0;
  lastTime = currentTime;

  // 3. Получение угла по оси Z (горизонтальная плоскость)
  // Библиотека сама считает угол с учетом калибровки
  double currentAngle = mpu.getAngleZ(); 

  // 4. Расчет PID-регулятора
  error = targetAngle - currentAngle; 
  integral += error * dt;
  integral = constrain(integral, -150, 150); // Защита от перенасыщения интеграла
  derivative = (error - lastError) / dt;
  lastError = error;

  double controlOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);

  // 5. Управление мотором через ESC
  int pwmSignal = MOTOR_STOP + (int)controlOutput;
  pwmSignal = constrain(pwmSignal, MOTOR_MIN, MOTOR_MAX); 

  esc.writeMicroseconds(pwmSignal);

  // Вывод данных для Плоттера по последовательному порту
  Serial.print("Target:"); Serial.print(targetAngle); Serial.print(" ");
  Serial.print("Current:"); Serial.print(currentAngle); Serial.print(" ");
  Serial.print("PWM:"); Serial.println(pwmSignal);

  delay(10); // Частота работы цикла около 100 Гц
}

