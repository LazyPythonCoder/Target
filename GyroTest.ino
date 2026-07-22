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
const float Kp = 5.0; 

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
  
  unsigned long duration1 = pulseIn(RC_PIN1, HIGH);
  unsigned long duration2 = pulseIn(RC_PIN2, HIGH);

  //Проверка на наличие сигнала на RC2 
  if (duration2 < 1000) {
    isGyro = false;
    Serial.println("Гироскоп отключен");

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
  mpu.update();
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
