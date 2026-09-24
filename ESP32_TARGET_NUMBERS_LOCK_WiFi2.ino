#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32";         // Enter SSID here
const char* password = "12345678";  // Enter Password here

/* Put IP Address details */
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);

int tar = 1;
int hd = 0;
int bd = 0;


// --- Конфигурация пинов ---
#define HEAD_PIN   25
#define BODY_PIN   27
#define SIGNAL_PIN 21

// --- Зоны попадания ---
constexpr int ZONE_HEAD = 1;
constexpr int ZONE_BODY = 2;

// --- Переменные для прерываний ---
volatile bool head_hit = false;
volatile bool body_hit  = false;

// --- Спинлок ---
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

// --- Блокировка мишени ---
volatile unsigned long last_hit_time = 0;
volatile bool is_locked = false;
constexpr unsigned long LOCK_DURATION_MS = 5000;

// --- Сигнальный пин ---
unsigned long signal_start_time = 0;
bool signal_active = false;
constexpr unsigned long SIGNAL_DURATION_MS = 2000;

// --- Счётчик ---
uint32_t total_hits = 0;

// --- Общий обработчик попадания ---
void IRAM_ATTR registerHit(volatile bool& hit_flag) {
  portENTER_CRITICAL_ISR(&myMutex);
  if (!is_locked) {
    hit_flag = true;
    is_locked = true;
    last_hit_time = millis();
  }
  portEXIT_CRITICAL_ISR(&myMutex);
}

void IRAM_ATTR headISR() { registerHit(head_hit); }
void IRAM_ATTR bodyISR() { registerHit(body_hit); }

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

  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);

  // Внешняя подтяжка 1 кОм — внутренняя не нужна
  pinMode(HEAD_PIN, INPUT);
  pinMode(BODY_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(HEAD_PIN), headISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BODY_PIN), bodyISR, FALLING);

  Serial.println("Система двухзонной мишени №1 запущена и готова.");
}

void loop() {
  server.handleClient();

  unsigned long current_millis = millis();

  bool local_head_hit = false;
  bool local_body_hit = false;

  portENTER_CRITICAL(&myMutex);

  if (is_locked && (current_millis - last_hit_time >= LOCK_DURATION_MS)) {
    is_locked = false;
  }

  if (head_hit) {
    local_head_hit = true;
    head_hit = false;
    body_hit = false; // Защита: приоритет головы (фактически недостижимо из-за is_locked)
  } else if (body_hit) {
    local_body_hit = true;
    body_hit = false;
  }

  portEXIT_CRITICAL(&myMutex);

  int hit_zone = 0;
  if (local_head_hit) {
    hit_zone = ZONE_HEAD;
    hd++;
  }
  else if (local_body_hit) {
    hit_zone = ZONE_BODY;
    bd++;
  }

  if (hit_zone > 0) {
    total_hits++;
    Serial.print("1-");
    Serial.print(total_hits);
    Serial.print("-");
    Serial.println(hit_zone);

    digitalWrite(SIGNAL_PIN, HIGH);
    signal_start_time = current_millis;
    signal_active = true;
  }

  if (signal_active && (current_millis - signal_start_time >= SIGNAL_DURATION_MS)) {
    digitalWrite(SIGNAL_PIN, LOW);
    signal_active = false;
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}

void handleUpdate() {
    // Формируем JSON-строку
   // Формируем JSON строку
  String json = "{";
  json += "\"target\":\"" + String(tar) + "\",";
  json += "\"head\":\"" + String(hd) + "\",";
  json += "\"body\":\"" + String(bd) + "\",";
  json += "\"target_cnt\":\"" + String(total_hits) + "\"";
  json += "}";

  server.send(200, "application/json", json); // Отправляем JSON
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
  str +="<p>Всего попаданий: <span id=\"target_cnt\">0</span></p>";
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
  str +="document.getElementById(\"target_cnt\").innerHTML = data.target_cnt;";
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
