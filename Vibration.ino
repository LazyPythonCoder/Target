// #include <Adafruit_MPU6050.h>
// #include <Adafruit_Sensor.h>
// #include <Wire.h>

// Adafruit_MPU6050 mpu;

// // Настройки чувствительности триггера удара
// // Исходное ускорение свободного падения ~ 9.8 м/с²
// const float THRESHOLD_IMPACT = 30.0; // Порог удара в м/с² (примерно 2.5G)
// unsigned long lastDebounceTime = 0;
// const unsigned long DEBOUNCE_DELAY = 1000; // Игнорировать повторные удары в течение 1 сек

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) delay(10); 

//   Serial.println("Поиск датчика MPU-6050...");

//   if (!mpu.begin()) {
//     Serial.println("Ошибка: Датчик MPU-6050 не найден!");
//     while (1) { delay(10); }
//   }
//   Serial.println("MPU-6050 успешно подключен.");

//   // Настройка диапазонов для регистрации резких перегрузок
//   mpu.setAccelerometerRange(MPU6050_RANGE_8_G); // Диапазон до +/- 8G
//   mpu.setGyroRange(MPU6050_RANGE_500_DEG);      // Диапазон гироскопа
//   mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // Фильтр шумов

//   delay(100);
// }

// void loop() {
//   sensors_event_t a, g, temp;
//   mpu.getEvent(&a, &g, &temp);

//   // Вычисление общего вектора ускорения (Magnitude)
//   // Формула: sqrt(x² + y² + z²)
//   float totalAcceleration = sqrt(a.acceleration.x * a.acceleration.x +
//                                  a.acceleration.y * a.acceleration.y +
//                                  a.acceleration.z * a.acceleration.z);

//   // Проверка превышения порога с защитой от дребезга
//   if (totalAcceleration > THRESHOLD_IMPACT) {
//     if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      
//       Serial.print("!!! УДАР ЗАФИКСИРОВАН !!! Сила: ");
//       Serial.print(totalAcceleration);
//       Serial.println(" м/с²");
      
//       // Здесь код реакции (включение сирены, отправка SMS, запись в лог)

//       lastDebounceTime = millis();
//     }
//   }
  
//   delay(10); // Высокая частота опроса для фиксации быстрых событий
// }





#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// Порог срабатывания удара по оси Z (в м/с²). 
// 15.0 м/с² ≈ 1.5g. Подберите значение под ваши задачи.
const float IMPACT_THRESHOLD = 15.0; 

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Инициализация датчика I2C
  if (!mpu.begin()) {
    Serial.println("Не найден датчик MPU6050!");
    while (1) {
      delay(10);
    }
  }

  // Настройка диапазона акселерометра (±8g)
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  // Настройка фильтра для подавления лишних шумов
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 готов. Ожидание удара по оси Z...");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Получаем ускорение по оси Z (параллельна земле)
  float z_accel = abs(a.acceleration.z);

  // Проверяем превышение порога удара
  if (z_accel > IMPACT_THRESHOLD) {
    Serial.print("УДАР ЗАРЕГИСТРИРОВАН! Ускорение по Z: ");
    Serial.print(z_accel);
    Serial.println(" m/s^2");
    
    // Задержка во избежание многократного срабатывания на один удар
    delay(200); 
  }

  delay(10); // Частота опроса датчика
}
