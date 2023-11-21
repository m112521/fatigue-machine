#include <QuadDisplay2.h>

#define HALL_PIN 7
#define OPTICAL_PIN 8

#define SPEED_1      5 
#define DIR_1        4
QuadDisplay qd(9);

   
volatile unsigned long count = 0;

void setup() {
  pinMode(HALL_PIN, INPUT);
  pinMode(OPTICAL_PIN, INPUT);

  pinMode(SPEED_1, OUTPUT);
  pinMode(DIR_1, OUTPUT); 

  attachInterrupt(digitalPinToInterrupt(HALL_PIN), countCycles, RISING);
  Serial.begin(9600);
  qd.begin();
  qd.displayClear();
  qd.displayInt(0);

  // START MOTOR  
  digitalWrite(DIR_1, HIGH);
  analogWrite(SPEED_1, 255);
  
}

void loop() {
  //Serial.println(count);
  if (digitalRead(OPTICAL_PIN)) {
    //detachInterrupt(digitalPinToInterrupt(HALL_PIN)); 
    // STOP MOTOR
    analogWrite(SPEED_1, 0);
    Serial.println(count);
    qd.displayInt(count);   
  }
}

void countCycles() {
  count++;
}

