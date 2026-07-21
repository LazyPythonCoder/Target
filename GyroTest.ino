// #include "I2Cdev.h"
// #include "MPU6050.h"
// #include "Wire.h"

// // Создаем объект для работы с датчиком
// MPU6050 mpu;

// // Пины управления драйвером HW-039 (BTS7960)
// const int R_EN = 4;
// const int L_EN = 5;
// const int RPWM = 6;  // Обязательно ШИМ-пин (на Leonardo: 3, 5, 6, 9, 10, 11, 13)
// const int LPWM = 9;  // Обязательно ШИМ-пин

// // Настройки регулятора
// const float Kp = 2.5;       // Коэффициент усиления (подберите экспериментально)
// const int deadZone = 15;    // Мертвая зона для исключения дрожания мотора
// const int maxSpeed = 255;   // Максимальная скорость ШИМ (0-255)

// int16_t gz;                 // Переменная для хранения угловой скорости вокруг оси Z

// void setup() {
//   // Инициализация шины I2C
//   Wire.begin();
  
//   // Настройка пинов драйвера на выход
//   pinMode(R_EN, OUTPUT);
//   pinMode(L_EN, OUTPUT);
//   pinMode(RPWM, OUTPUT);
//   pinMode(LPWM, OUTPUT);

//   // Включаем оба плеча драйвера HW-039
//   digitalWrite(R_EN, HIGH);
//   digitalWrite(L_EN, HIGH);

//   // Инициализация MPU6050
//   mpu.initialize();
  
//   // Дополнительно можно выполнить калибровку нуля в плоскости при старте
//   mpu.setZGyroOffset(0); 
// }

// void loop() {
//   // Считываем только угловую скорость по оси Z (горизонтальное вращение)
//   gz = mpu.getRotationZ();

//   // Вычисляем управляющее воздействие (пропорционально скорости поворота)
//   // Знак управляет направлением вращения мотора для противодействия
//   long motorOut = gz * Kp / 100; 

//   // Ограничиваем рамками ШИМ
//   if (motorOut > maxSpeed)  motorOut = maxSpeed;
//   if (motorOut < -maxSpeed) motorOut = -maxSpeed;

//   // Управление мотором в зависимости от знака и мертвой зоны
//   if (motorOut > deadZone) {
//     // Вращение в одну сторону
//     analogWrite(RPWM, motorOut);
//     analogWrite(LPWM, 0);
//   } 
//   else if (motorOut < -deadZone) {
//     // Вращение в противоположную сторону
//     analogWrite(RPWM, 0);
//     analogWrite(LPWM, abs(motorOut));
//   } 
//   else {
//     // Остановка при отсутствии внешнего вращения
//     analogWrite(RPWM, 0);
//     analogWrite(LPWM, 0);
//   }

//   delay(10); // Небольшая задержка для стабилизации цикла контроля
// }



// #include "I2Cdev.h"
// #include "MPU6050_6Axis_MotionApps20.h"
// #include "Wire.h"

// MPU6050 mpu;

// // Настройки пинов драйвера HW-039 (BTS7960)
// const int R_EN = 4;
// const int L_EN = 5;
// const int RPWM = 6;
// const int LPWM = 9;

// // --- НАСТРОЙКА PID-РЕГУЛЯТОРА ---
// float Kp = 12.0;   // Пропорциональный коэффициент (возвращает в центр)
// float Ki = 0.0;   // Интегральный коэффициент (убирает статическую ошибку)
// float Kd = 0;    // Дифференциальный коэффициент (гасит колебания)

// float targetAngle = 0.0; // Желаемый угол (цель — удерживать 0)
// float integral = 0.0;
// float prevError = 0.0;
// unsigned long prevTime = 0;

// // Переменные для работы с DMP MPU6050
// bool dmpReady = false;
// uint8_t fifoBuffer[64];
// Quaternion q;           // Кватернион для углов
// VectorFloat gravity;    // Вектор гравитации
// float ypr[3];           // Массив углов: [0] - Yaw (Z), [1] - Pitch, [2] - Roll

// void setup() {
//   Wire.begin();
//   Wire.setClock(400000); // Быстрая шина I2C

//   pinMode(R_EN, OUTPUT);
//   pinMode(L_EN, OUTPUT);
//   pinMode(RPWM, OUTPUT);
//   pinMode(LPWM, OUTPUT);

//   // Включаем драйвер мотора
//   digitalWrite(R_EN, HIGH);
//   digitalWrite(L_EN, HIGH);

//   mpu.initialize();
//   if (mpu.dmpInitialize() == 0) {
//     // Автоматическая калибровка датчика (при включении держите мотор неподвижно!)
//     mpu.CalibrateAccel(6);
//     mpu.CalibrateGyro(6);
    
//     mpu.setDMPEnabled(true);
//     dmpReady = true;
//   }
//   prevTime = millis();
// }

// void loop() {
//   if (!dmpReady) return;

//   // Читаем свежие данные из DMP
//   if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
//     // Получаем углы в радианах
//     mpu.dmpGetQuaternion(&q, fifoBuffer);
//     mpu.dmpGetGravity(&gravity, &q);
//     mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

//     // Переводим текущий угол Yaw (ось Z) в градусы
//     float currentAngle = ypr[0] * 180.0 / M_PI;

//     // --- Расчет PID ---
//     unsigned long currentTime = millis();
//     float dt = (currentTime - prevTime) / 1000.0; // Время в секундах
//     if (dt <= 0.0) dt = 0.001; // Защита от деления на 0

//     float error = targetAngle - currentAngle; // Ошибка положения
    
//     // Интегральная составляющая с защитой от насыщения (anti-windup)
//     integral += error * dt;
//     integral = constraint(integral, -500, 500); 

//     // Дифференциальная составляющая
//     float derivative = (error - prevError) / dt;

//     // Итоговый управляющий сигнал
//     float output = (Kp * error) + (Ki * integral) + (Kd * derivative);

//     // Сохраняем значения для следующего шага
//     prevError = error;
//     prevTime = currentTime;

//     // --- Управление мотором ---
//     moveMotor(output);
//   }
// }

// void moveMotor(float controlSignal) {
//   int speed = abs((int)controlSignal);
  
//   // Ограничиваем максимальный ШИМ
//   if (speed > 255) speed = 255;
  
//   // Мертвая зона (минимальный ШИМ, чтобы мотор не пищал на месте)
//   if (speed < 12) {
//     analogWrite(RPWM, 0);
//     analogWrite(LPWM, 0);
//     return;
//   }

//   // Направление вращения в зависимости от знака сигнала
//   if (controlSignal > 0) {
//     analogWrite(RPWM, speed);
//     analogWrite(LPWM, 0);
//   } else {
//     analogWrite(RPWM, 0);
//     analogWrite(LPWM, speed);
//   }
// }

// // Вспомогательная функция ограничения диапазона
// float constraint(float value, float minVal, float maxVal) {
//   if (value < minVal) return minVal;
//   if (value > maxVal) return maxVal;
//   return value;
// }

#include <Wire.h>
#include <MPU6050_light.h>

// Настройка пинов по вашему запросу
const int R_EN = 4;  // Включение правого плеча
const int L_EN = 5;  // Включение левого плеча
const int RPWM = 6;  // ШИМ управление скоростью (вправо)
const int LPWM = 9;  // ШИМ управление скоростью (влево)
const int RC_PIN1 = 7;    //Пин чтения сигнала с приемника 
const int RC_PIN2 = 8;    // Пин чтения сигнала с приемника
// Настройки скорости и мертвой зоны
const int MOTOR_SPEED = 150;     // Постоянная скорость (от 0 до 255)
const int DEAD_ZONE = 40;        // Мертвая зона вокруг 1500 мкс

float startAngle = 0;
bool isGyro = false;

// Коэффициент пропорционального регулятора (P-регулятор)
const float Kp = 7.0; 

// Зона нечувствительности в градусах
const float deadZone = 1.5; 

MPU6050 mpu(Wire);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  // Инициализация MPU6050
  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  while(status != 0){ } // Остановка, если датчик не подключен
  
  Serial.println(F("Калибровка гироскопа. Не двигайте конструкцию..."));
  delay(1000);
  mpu.calcOffsets(true); // Калибровка нуля
  Serial.println(F("Калибровка завершена успешно."));

  // Настройка всех пинов драйвера на выход
  pinMode(R_EN, OUTPUT);
  pinMode(L_EN, OUTPUT);
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);

  pinMode(RC_PIN1, INPUT);
  pinMode(RC_PIN2, INPUT);
  
  // Активируем оба плеча драйвера HW-039
  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);
}

void loop() {
  mpu.update();

  unsigned long duration1 = pulseIn(RC_PIN1, HIGH);
  unsigned long duration2 = pulseIn(RC_PIN2, HIGH);

   // Проверка на наличие сигнала на RC1 (0 означает отсутствие сигнала)
  if (duration1 ==0) {
    stopMotor();
    return;
  }

  // Логика управления направлением
  if (duration1 < (1500 - DEAD_ZONE)) {
    // Вращение в одну сторону
    analogWrite(LPWM, 0);
    analogWrite(RPWM, MOTOR_SPEED);
  } 
  else if (duration1 > (1500 + DEAD_ZONE)) {
    // Вращение в другую сторону
    analogWrite(RPWM, 0);
    analogWrite(LPWM, MOTOR_SPEED);
  } 
  else {
    // Сигнал в районе 1500 мкс — стоп
    stopMotor();
  }

//Проверка на наличие сигнала на RC2 
if (duration2 < 1000) {
  isGyro = false;
  Serial.println("Гироскоп отключен");
}
else {
  Serial.println("Гироскоп активирован");
  if (!isGyro) {
    startAngle =  mpu.getAngleZ();
    isGyro = true;
  }
  tracking();
}
}

// Функция остановки мотора
void stopMotor() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
}

//Фунция отслеживания угла
void tracking() {
  // Получаем текущий угол по оси Z
  float currentAngle = mpu.getAngleZ();
  
  // Ошибка положения (цель — 0 градусов)
  float error = startAngle - currentAngle; 
  
  if (abs(error) < deadZone) {
    stopMotor();
  } 
  else {
    // Расчет управляющего сигнала
    float controlSignal = error * Kp;
    
    // Ограничение ШИМ от 0 до 255
    int pwmValue = constrain(abs(controlSignal), 0, 255);
    
    if (controlSignal < 0) {
      // Вращение в одну сторону
      analogWrite(RPWM, pwmValue);
      analogWrite(LPWM, 0);
    } else {
      // Вращение в другую сторону
      analogWrite(RPWM, 0);
      analogWrite(LPWM, pwmValue);
    }
  }

  // Вывод данных в Плоттер портируемого интерфейса
  Serial.print("AngleZ:"); Serial.print(currentAngle);
  Serial.print(",");
  Serial.print("Error:"); Serial.println(error);

  delay(10); 

}



