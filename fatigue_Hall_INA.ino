#include <Wire.h>
#include <Adafruit_INA219.h>
#include <TB6612_ESP32.h>

// Motor pins
#define AIN1 13
#define BIN1 12
#define AIN2 14
#define BIN2 27
#define PWMA 26
#define PWMB 25
#define STBY 33

Motor motor1 = Motor(AIN1, AIN2, PWMA, 1, STBY, 5000, 8, 1);
Motor motor2 = Motor(BIN1, BIN2, PWMB, 1, STBY, 5000, 8, 2);

Adafruit_INA219 ina219;

const int hallPin = 4; 
const int SPEED = 120;

// Переменные для прерывания
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile int triggerCount = 0;
volatile bool hallTriggered = false; // Флаг: магнит только что пролетел датчик

unsigned long lastInterruptTime = 0;
const unsigned long MIN_REVOLUTION_TIME_MS = 150; // Настоящий антидребезг

// Логика состояний
bool isForwardStroke = true; // Направление хода (true - вперед/давление, false - назад/возврат)
float peakCurrentThisStroke = 0.0; 
const float BREAK_CURRENT_THRESHOLD = 120.0; 
bool testActive = true;

void IRAM_ATTR handleHallInterrupt() {
  unsigned long currentTime = millis();
  // Аппаратный антидребезг по времени
  if (currentTime - lastInterruptTime >= MIN_REVOLUTION_TIME_MS) {
    triggerCount++;
    hallTriggered = true; // Сигнализируем в loop
    lastInterruptTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  pinMode(hallPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallPin), handleHallInterrupt, FALLING);

  Serial.println("Starting test in 2 seconds...");
  delay(2000);
  
  forward(motor1, motor2, SPEED); 
  Serial.println("Motors started.");
}

void loop() {
  if (!testActive) return;

  // 1. Постоянно фиксируем пиковый ток в текущей фазе движения
  float rawCurrent = ina219.getCurrent_mA();
  if (rawCurrent > peakCurrentThisStroke) {
    peakCurrentThisStroke = rawCurrent;
  }

  // Безопасное чтение флага из прерывания
  bool localTriggered = false;
  portENTER_CRITICAL(&mux);
  localTriggered = hallTriggered;
  hallTriggered = false; // Сбрасываем флаг
  portEXIT_CRITICAL(&mux);

  // 2. Если датчик Холла сработал — значит фаза движения сменилась!
  if (localTriggered) {
    
    if (isForwardStroke) {
      // Мы закончили ход ВПЕРЕД (нажатие). Время анализировать накопленный пик тока!
      Serial.print("📊 Конец рабочего хода. Пиковый ток: ");
      Serial.print(peakCurrentThisStroke);
      Serial.println(" мА");

      if (peakCurrentThisStroke < BREAK_CURRENT_THRESHOLD) {
        stopTestProcedure();
        return; // Выходим из loop, тест завершен
      }
      
      // Если все ок, готовимся к холостому ходу (назад)
      isForwardStroke = false; 
      peakCurrentThisStroke = 0.0; // Сбрасываем пик для хода назад
      
    } else {
      // Мы закончили ход НАЗАД (возврат). Ток здесь не проверяем, просто меняем фазу.
      isForwardStroke = true;
      peakCurrentThisStroke = 0.0; // Сбрасываем пик для следующего рабочего хода
    }
  }
}

void stopTestProcedure() {
  detachInterrupt(digitalPinToInterrupt(hallPin));
  testActive = false;
  
  motor1.brake();
  motor2.brake();
  digitalWrite(STBY, LOW); 
  
  Serial.println("\n=============================================");
  Serial.println("❌❌❌ ТЕСТ ЗАВЕРШЕН: ФАНЕРА РАЗРУШЕНА! ❌❌❌");
  Serial.print("Фактических полных циклов: ");
  Serial.println(triggerCount / 2);
  Serial.println("=============================================\n");
}
