#include <Servo.h>
#include <ezButton.h>

#define RELAY_PIN 6 // PIN to Relay 
#define SERVO_PIN 8 // PIN to SERVO
#define TARGET_PIN 5 // PIN to Relay for target

Servo myservo; 

int pos = 0;

ezButton actSwitch(A2);

void setup() { 
    pinMode(RELAY_PIN, INPUT);
    Serial.begin(9600); 
    actSwitch.setDebounceTime(40);
    myservo.attach(SERVO_PIN); 
    myservo.write(180); 
    Serial.write("Положение сервопривода при активации=");
    Serial.println(myservo.read());
}  
void loop() {  
   actSwitch.loop();
   int angle = myservo.read();
   
   if (actSwitch.isPressed()) {
      Serial.println("Act Switch pressed");
      Serial.print("Состояние реле=");
      Serial.println(digitalRead(RELAY_PIN));
      if (angle<=30) {
        myservo.write(180);
      //   myservo.writeMicroseconds(2500); 
        delay(100);
      } else {
        myservo.write(0);
        delay(100);
      }
      
      Serial.print("Положение сервопривода из за Act Switch=");
      Serial.println(myservo.read());
   }
   int relayState = digitalRead(RELAY_PIN); 
  //  Serial.println(relayState);
   if (relayState == HIGH) {
    Serial.print("Состояние реле=");
    Serial.println(relayState);
     angle =  myservo.read();
     if (angle<50) {
      myservo.write(180);
      }
      delay(100);
      Serial.print("Положение сервопривода из за реле=");
      Serial.println(myservo.read());
   }
   int targetState = digitalRead(TARGET_PIN); 
   if (targetState == HIGH) {
      Serial.println("Target!!!");
      angle =  myservo.read();
      if (angle>70) {
         myservo.write(0);
         delay(100);
      }
      Serial.println("Положение сервопривода из за реле цели=");
      Serial.println(myservo.read());
   }
}  