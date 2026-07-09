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

// Motor objects
Motor motor1 = Motor(AIN1, AIN2, PWMA, 1, STBY, 5000, 8, 1);
Motor motor2 = Motor(BIN1, BIN2, PWMB, 1, STBY, 5000, 8, 2);

// INA219 current sensor
Adafruit_INA219 ina219;

// Hall sensor settings
const int hallPin = 34; // Использовать только ADC1 (например, GPIO 34)
const int threshold = 3500;

// Настройки фильтра скользящего среднего (Moving Average)
const int FILTER_WINDOW = 10;       // Размер окна фильтрации (чем больше, тем плавнее, но выше задержка)
float currentBuffer[FILTER_WINDOW]; // Массив для хранения последних показаний тока
int bufferIndex = 0;                // Индекс текущей позиции в массиве
float currentSum = 0;               // Сумма элементов в буфере для быстрого расчета

// Variables for counting and state tracking
int sensorValue = 0;
int triggerCount = 0;
bool isTriggered = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize the INA219
  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("INA219 Initialized.");

  // Инициализация буфера фильтра нулями
  for (int i = 0; i < FILTER_WINDOW; i++) {
    currentBuffer[i] = 0.0;
  }

  // Set ADC resolution to 12-bits (0-4095)
  analogReadResolution(12);
  
  // Configure attenuation for full 0-3.3V range
  analogSetAttenuation(ADC_11db); 
  
  // Configure the hall sensor pin
  pinMode(hallPin, INPUT);

  Serial.println("Hall sensor initialized.");
  delay(500);
  
  // Start motors
  forward(motor1, motor2, 100);
  Serial.println("Motors started.");
}

void loop() {
  // 1. Считывание мгновенного значения тока с датчика INA219
  float rawCurrent = ina219.getCurrent_mA();

  // 2. Алгоритм Скользящего Среднего (Moving Average)
  // Вычитаем старое значение, которое сейчас перезапишем
  currentSum -= currentBuffer[bufferIndex];
  // Записываем новое значение в буфер
  currentBuffer[bufferIndex] = rawCurrent;
  // Добавляем новое значение к общей сумме
  currentSum += currentBuffer[bufferIndex];
  
  // Сдвигаем индекс буфера по кругу
  bufferIndex++;
  if (bufferIndex >= FILTER_WINDOW) {
    bufferIndex = 0;
  }

  // Вычисляем итоговое сглаженное значение тока
  float smoothedCurrent = currentSum / FILTER_WINDOW;

  // 3. Чтение датчика Холла и логика триггера
  sensorValue = analogRead(hallPin);
  
  if (sensorValue >= threshold) {
    if (!isTriggered) {
      triggerCount++;
      isTriggered = true;
    }
  } else {
    isTriggered = false;
  }

  // Вывод данных в консоль (используется отфильтрованный ток)
  Serial.print("RawCurrent(mA):");
  Serial.print(rawCurrent);
  Serial.print(" | SmoothCurrent(mA):");
  Serial.print(smoothedCurrent);
  Serial.print(" | HallSensor:");
  Serial.print(sensorValue);
  Serial.print(" | Counts:");
  Serial.println(triggerCount);

  // Для более быстрого отклика датчиков и фильтра задержку можно уменьшить
  delay(50); 
}
