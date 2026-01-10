#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32";         // Enter SSID here
const char* password = "12345678";  // Enter Password here

/* Put IP Address details */
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);



int target_cnt = 0;
unsigned long previousMillis = 0;    // Переменная для хранения времени последнего выполнения
unsigned long startMillisTarget = 0;
const long interval = 200; 
const long targetinterval = 2000; //Значение времени для высокого значения на targetPin

const byte targetPin = 21;// Пин, к которому подключен источник напряжения
const byte interruptPin1 = 32; // Пин, к которому подключен источник напряжения
const byte interruptPin2 = 33; // Пин, к которому подключен источник напряжения
// const byte interruptPin3 = 32; // Пин, к которому подключен источник напряжения
volatile int pulseCounter1 = 0; // Переменная, которая изменяется в прерывании (должна быть volatile)
volatile int pulseCounter2 = 0;

volatile unsigned long button_time = 0;
volatile unsigned long last_button_time = 0;

int tar = 1;
int hd = 0;
int bd = 0;


// volatile int pulseCounter3 = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Мьютекс для безопасного доступа к переменной из loop()

void handleUpdate() {
    // Формируем JSON-строку
   // Формируем JSON строку
  String json = "{";
  json += "\"target\":\"" + String(tar) + "\",";
  json += "\"head\":\"" + String(hd) + "\",";
  json += "\"body\":\"" + String(bd) + "\"";
  json += "}";

  server.send(200, "application/json", json); // Отправляем JSON
}

void IRAM_ATTR handleInterrupt1() {
  // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
  portENTER_CRITICAL_ISR(&mux);
  button_time = micros();
  if (button_time - last_button_time > 10) {
  pulseCounter1++;
  last_button_time = button_time;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR handleInterrupt2() {
  // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
  portENTER_CRITICAL_ISR(&mux);
  button_time = micros();
  if (button_time - last_button_time > 10) {
  pulseCounter2++;
  last_button_time = button_time;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

// void IRAM_ATTR handleInterrupt3() {
//   // Функция обработки прерывания. Должна быть максимально короткой и не использовать Serial.print()
//   portENTER_CRITICAL_ISR(&mux);
//   pulseCounter3++;
//   portEXIT_CRITICAL_ISR(&mux);
// }

void setup() {
  Serial.begin(9600);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  server.on("/", handle_OnConnect);
  server.on("/status", handleUpdate);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");


  Serial.println("Ожидание кратковременного напряжения (импульса) на пине GPIO ");

  // Настройка пинов как входа с внутренним подтягивающим резистором
  pinMode(interruptPin1, INPUT_PULLUP);
  pinMode(interruptPin2, INPUT_PULLUP);
  // pinMode(interruptPin3, INPUT_PULLUP);
  pinMode(targetPin, OUTPUT);
  digitalWrite(targetPin, LOW);

  // Прикрепление функции прерывания к пину
  // FALLING - срабатывает при переходе из HIGH в LOW (при нажатии кнопки в данной схеме)
  attachInterrupt(digitalPinToInterrupt(interruptPin1), handleInterrupt1, FALLING);
  attachInterrupt(digitalPinToInterrupt(interruptPin2), handleInterrupt2, FALLING);
  // attachInterrupt(digitalPinToInterrupt(interruptPin3), handleInterrupt3, FALLING);
}

void loop() {
  server.handleClient();

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

    // if (pulseCounter3 > 0) {
    // // Безопасное чтение переменной из прерывания
    // portENTER_CRITICAL(&mux);
    // int currentCount3 = pulseCounter3;
    // pulseCounter3 = 0;
    // portEXIT_CRITICAL(&mux);
    // // Serial.print("Обнаружен импульс3! Всего импульсов: ");
    // // Serial.println(currentCount3);
    // target_sub_number = 3;    
    // }

    if (target_sub_number) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        target_cnt++;
        previousMillis = currentMillis;
        // Serial.print("Попаданий=");
        Serial.print(1); //Номер мишени
        Serial.print("-");
        Serial.print(target_cnt);
        Serial.print("-");
        switch (target_sub_number) {
          case 1:
            // Serial.println(" Альфа");
            Serial.println(1);
            hd++;
            break;
          case 2:
            // Serial.println(" Чарли");
            Serial.println(2); 
            bd++;
            break; 
          // case 3:
          //   // Serial.println(" Дельта"); 
          //   Serial.println(3);
          //   break; 
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

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

void handle_OnConnect() {
  server.send(200, "text/html", createHTML());
}

String createHTML() {
  String str = "<!DOCTYPE html> <html>";
  str +="<html>";
  str +="<head>";
  str +="<meta charset=\"UTF-8\">";
  str +="<title>AJAX Arduino</title>";
  str +="</head>";
  str +="<body>";
  str +="<h1>Данные с датчиков:</h1>";
  str +="<p>Мишень: <span id=\"tar\">0</span></p>";
  str +="<p>Голова: <span id=\"hd\">0</span></p>";
  str +="<p>Туловище: <span id=\"bd\">0</span></p>";
  str +="<script>";
  str +="function updateData() {";
  str +="var xhttp = new XMLHttpRequest();";
  str +="xhttp.onreadystatechange = function() {";
  str +="if (this.readyState == 4 && this.status == 200) {";
  str +="var data = JSON.parse(this.responseText);";
                      // Обновляем текст в тегах span
  str +="document.getElementById(\"tar\").innerHTML = data.target;";
  str +="document.getElementById(\"hd\").innerHTML = data.head;";
  str +="document.getElementById(\"bd\").innerHTML = data.body;";
  str +="}";
  str +="};";
  str +="xhttp.open(\"GET\", \"/status\", true);";
  str +="xhttp.send();";
  str +="}";
  str +="setInterval(updateData, 1000);";
  str +="</script>";
  str +="</body>";
  str +="</html>";
  return str;

}



