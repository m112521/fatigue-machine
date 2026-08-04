#include <Wire.h>
#include <Adafruit_INA219.h>
#include <TB6612_ESP32.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Fatigue_Tester_ESP32";
const char* password = "1234567890";

WebServer server(80);

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
const int hallPin2 = 5;  

float breakCurrentThreshold = 120.0; 
const float STALL_CURRENT_THRESHOLD = 950.0; 
int requestedSpeed = 120; 

unsigned long totalCycles = 0;               
float peakCurrentForward = 0.0;              
float currentCurrentLive = 0.0;
float calibrationMaxPeak = 0.0;     
String machineStatus = "ГОТОВ К ТЕСТУ";
String currentMode = "TEST";        
bool testActive = false;

volatile unsigned long timeFar = 0;   
volatile unsigned long timeNear = 0;  
volatile bool sequenceStarted = false; 

// --- HTML + CSS + JS СТРАНИЦА ИНТЕРФЕЙСА (ПОЛНОСТЬЮ АВТОНОМНАЯ БЕЗ ИНТЕРНЕТА) ---
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Fatigue Tester Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; background: #f4f6f9; color: #333; text-align: center; margin: 0; padding: 15px; }
        .container { max-width: 650px; margin: 0 auto; background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; font-size: 22px; margin-bottom: 15px; }
        .mode-selector { display: flex; justify-content: space-around; background: #e0e0e0; padding: 6px; border-radius: 8px; margin-bottom: 20px; }
        .mode-btn { flex: 1; border: none; padding: 10px; font-weight: bold; cursor: pointer; border-radius: 6px; background: none; transition: 0.2s; }
        .mode-btn.active { background: #2980b9; color: white; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 20px; }
        .card { background: #ecf0f1; padding: 12px; border-radius: 8px; font-weight: bold; font-size: 13px; }
        .card span { display: block; font-size: 24px; color: #2c3e50; margin-top: 5px; }
        .control-panel { margin-bottom: 20px; }
        .btn { border: none; padding: 12px 20px; font-size: 15px; font-weight: bold; border-radius: 6px; cursor: pointer; margin: 4px; color: white; transition: 0.1s; }
        .btn-start { background: #27ae60; } .btn-start:hover { background: #219150; }
        .btn-stop { background: #c0392b; } .btn-stop:hover { background: #962d22; }
        .btn-new { background: #f39c12; }  .btn-new:hover { background: #d35400; }
        .slider-section { background: #f8f9fa; padding: 15px; border-radius: 8px; margin-bottom: 15px; text-align: left; }
        .slider-label { font-weight: bold; display: flex; justify-content: space-between; }
        .slider { width: 100%; margin-top: 10px; cursor: pointer; }
        .graph-box { background: #fff; border: 1px solid #bdc3c7; border-radius: 8px; padding: 10px; margin-top: 15px; }
        canvas { width: 100%; height: 180px; display: block; background: #fafafa; }
        .highlight { color: #e74c3c !important; }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚙️ Стенд усталостных испытаний</h1>
        <div class="mode-selector">
            <button id="modeTest" class="mode-btn active" onclick="setMode('TEST')">📊 РЕЖИМ ТЕСТА</button>
            <button id="modeCalib" class="mode-btn" onclick="setMode('CALIBRATION')">📐 КАЛИБРОВКА</button>
        </div>
        <div class="grid">
            <div class="card">СТАТУС СИСТЕМЫ <span id="status" style="font-size:18px; color:#e67e22;">ГОТОВ К ТЕСТУ</span></div>
            <div class="card">ПОЛНЫХ ЦИКЛОВ <span id="cycles">0</span></div>
            <div class="card">ТЕКУЩИЙ ТОК <span id="current" style="color:#2980b9;">0.0 мА</span></div>
            <div class="card">РАСЧЕТНЫЙ ПОРОГ <span id="threshold" class="highlight">120.0 мА</span></div>
        </div>
        <div class="slider-section">
            <div class="slider-label"><span>Заданная скорость (ШИМ):</span><span id="speedVal" style="color:#2980b9;">120</span></div>
            <input type="range" min="100" max="210" value="120" class="slider" id="speedSlider" oninput="updateSliderLabel(this.value)" onchange="sendSpeed(this.value)">
        </div>
        <div class="control-panel">
            <button class="btn btn-start" onclick="sendCommand('start')">СТАРТ</button>
            <button class="btn btn-stop" onclick="sendCommand('stop')">СТОП</button>
            <button class="btn btn-new" onclick="sendCommand('newtest')">НОВЫЙ ТЕСТ</button>
        </div>
        <div class="graph-box">
            <div style="font-size: 12px; font-weight: bold; margin-bottom: 5px; color:#7f8c8d;">Осциллограмма тока нагрузки (0 - 1000 мА)</div>
            <canvas id="liveGraph" width="500" height="150"></canvas>
        </div>
    </div>
    <script>
        var currentDataPoints = [];
        var maxPoints = 50;

        function updateSliderLabel(val) { document.getElementById('speedVal').innerText = val; }
        
        function sendCommand(cmd) { 
            fetch('/' + cmd + '?t=' + Date.now()).catch(err => console.log(err));
        }
        
        function sendSpeed(val) {
            fetch('/setspeed?val=' + val + '&t=' + Date.now())
                .then(res => res.text())
                .then(safeVal => {
                    document.getElementById('speedSlider').value = safeVal;
                    document.getElementById('speedVal').innerText = safeVal;
                }).catch(err => console.log(err));
        }
        
        function setMode(mode) {
            fetch('/setmode?mode=' + mode + '&t=' + Date.now())
                .then(res => {
                    if(res.ok) {
                        document.getElementById('modeTest').classList.toggle('active', mode === 'TEST');
                        document.getElementById('modeCalib').classList.toggle('active', mode === 'CALIBRATION');
                    } else {
                        alert("Нельзя сменить режим во время активного теста! Нажмите СТОП.");
                    }
                }).catch(err => console.log(err));
        }
        
        function drawGraph() {
            var canvas = document.getElementById("liveGraph");
            var ctx = canvas.getContext("2d");
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Сетка
            ctx.strokeStyle = "#eaeded"; ctx.lineWidth = 1;
            for(var i=0; i<canvas.height; i+=30) {
                ctx.beginPath(); ctx.moveTo(0, i); ctx.lineTo(canvas.width, i); ctx.stroke();
            }
            
            if(currentDataPoints.length < 2) return;
            
            ctx.strokeStyle = "#2980b9"; ctx.lineWidth = 2; ctx.beginPath();
            for(var i = 0; i < currentDataPoints.length; i++) {
                var x = (canvas.width / (maxPoints - 1)) * i;
                // Масштабируем ток (0 - 1000мА) под высоту канваса (150px)
                var val = parseFloat(currentDataPoints[i]);
                if (val > 1000) val = 1000;
                var y = canvas.height - (val * (canvas.height / 1000));
                
                if(i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        setInterval(function() {
            fetch('/data?t=' + Date.now())
                .then(res => res.json())
                .then(data => {
                    document.getElementById('status').innerText = data.status;
                    document.getElementById('cycles').innerText = data.cycles;
                    document.getElementById('current').innerText = data.current + " мА";
                    document.getElementById('threshold').innerText = data.threshold + " мА";
                    document.getElementById('modeTest').classList.toggle('active', data.mode === 'TEST');
                    document.getElementById('modeCalib').classList.toggle('active', data.mode === 'CALIBRATION');
                    
                    currentDataPoints.push(data.current);
                    if(currentDataPoints.length > maxPoints) currentDataPoints.shift();
                    drawGraph();
                }).catch(err => console.log("Сбой связи"));
        }, 400);
    </script>
</body>
</html>
)rawliteral";



int getSafeSpeed(int speed) {
    if (speed < 100) return 100;
    if (speed >= 125 && speed <= 135) return 120; 
    if (speed > 135 && speed < 155)   return 160; 
    if (speed >= 195 && speed <= 205) return 190; 
    if (speed > 210)                  return 190; 
    return speed;
}

void IRAM_ATTR ISR_Far() { timeFar = millis(); sequenceStarted = true; }
void IRAM_ATTR ISR_Near() { timeNear = millis(); sequenceStarted = true; }

void startTestProcedure() {
    if (testActive) return;
    timeFar = 0; timeNear = 0; sequenceStarted = false;
    int safeSpeed = getSafeSpeed(requestedSpeed);
    forward(motor1, motor2, safeSpeed); 
    testActive = true;
    if (currentMode == "CALIBRATION") {
        calibrationMaxPeak = 0.0;
        machineStatus = "КАЛИБРОВКА: ИДЕТ ЗАМЕР";
    } else {
        peakCurrentForward = 0.0;
        machineStatus = "ИСПЫТАНИЕ ИДЕТ";
    }
}

void stopTestProcedure(String reason) {
    testActive = false; machineStatus = reason;
    motor1.brake(); motor2.brake(); digitalWrite(STBY, LOW); 
}

void newTestProcedure() {
    stopTestProcedure("ГОТОВ К ТЕСТУ");
    totalCycles = 0; peakCurrentForward = 0.0;
    if (calibrationMaxPeak == 0.0) breakCurrentThreshold = 120.0; 
}

// --- СЕРВЕРНЫЕ МЕТОДЫ ОБРАБОТКИ ЗАПРОСОВ ---
void handleRoot() { server.send_P(200, "text/html", HTML_PAGE); }

void handleData() {
    String json = "{";
    json += "\"status\":\"" + machineStatus + "\",";
    json += "\"cycles\":" + String(totalCycles) + ",";
    json += "\"current\":" + String(currentCurrentLive, 1) + ",";
    json += "\"threshold\":" + String(breakCurrentThreshold, 1) + ",";
    json += "\"mode\":\"" + currentMode + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleStart() { 
    startTestProcedure(); 
    server.send(200, "text/plain", "OK"); 
}

void handleStop() { 
    stopTestProcedure("ОСТАНОВЛЕН ПОЛЬЗОВАТЕЛЕМ"); 
    server.send(200, "text/plain", "OK"); 
}

void handleNewTest() { 
    newTestProcedure(); 
    server.send(200, "text/plain", "OK"); 
}

void handleSetSpeed() {
    if (server.hasArg("val")) {
        requestedSpeed = server.arg("val").toInt();
        int safeSpeed = getSafeSpeed(requestedSpeed);
        if (testActive) forward(motor1, motor2, safeSpeed);
        server.send(200, "text/plain", String(safeSpeed));
    }
}

void handleSetMode() {
    if (server.hasArg("mode") && !testActive) {
        currentMode = server.arg("mode"); 
        server.send(200, "text/plain", "OK");
    } else { 
        server.send(400, "text/plain", "Busy"); 
    }
}

// --- ИНИЦИАЛИЗАЦИЯ И СТАРТ СЕРВЕРА (Теперь строго ниже всех обработчиков!) ---
void initWebServer() {
    server.on("/", handleRoot); 
    server.on("/data", handleData);
    server.on("/start", handleStart); 
    server.on("/stop", handleStop);
    server.on("/newtest", handleNewTest); 
    server.on("/setspeed", handleSetSpeed);
    server.on("/setmode", handleSetMode); 
    server.begin();
    Serial.println("Веб-сервер успешно запущен.");
}

void setup() {
    Serial.begin(115200); 
    if (!ina219.begin()) { while (1) { delay(10); } }
    
    pinMode(hallPin, INPUT_PULLUP); 
    pinMode(hallPin2, INPUT_PULLUP);
    
    attachInterrupt(digitalPinToInterrupt(hallPin), ISR_Far, FALLING);
    attachInterrupt(digitalPinToInterrupt(hallPin2), ISR_Near, FALLING);

    WiFi.softAP(ssid, password);
    
    // Вызываем настроенный сервер
    initWebServer(); 
}

void loop() {
    server.handleClient();
    if (!testActive) return;

    currentCurrentLive = ina219.getCurrent_mA();
    if (totalCycles >= 3 && currentCurrentLive > STALL_CURRENT_THRESHOLD) {
        stopTestProcedure("ЗАКЛИНИВАНИЕ (" + String(currentCurrentLive, 1) + " мА)"); return;
    }
    if (currentCurrentLive > peakCurrentForward) peakCurrentForward = currentCurrentLive;

    if (timeFar > 0 && timeNear > 0) {
        noInterrupts();
        unsigned long tFar = timeFar; unsigned long tNear = timeNear;
        timeFar = 0; timeNear = 0; sequenceStarted = false;
        interrupts();

        if (tNear < tFar) { // КОНЕЦ ХОДА НАЗАД
            totalCycles++;
            if (currentMode == "CALIBRATION") {
                if (totalCycles <= 10) {
                    machineStatus = "КАЛИБРОВКА (" + String(totalCycles) + "/10 ц.)";
                } else {
                    breakCurrentThreshold = calibrationMaxPeak * 0.70;
                    currentMode = "TEST"; stopTestProcedure("КАЛИБРОВКА УСПЕШНА"); return;
                }
            }
        } else if (tFar < tNear) { // КОНЕЦ ХОДА ВПЕРЕД
            if (currentMode == "CALIBRATION") {
                if (totalCycles > 3 && peakCurrentForward > calibrationMaxPeak && peakCurrentForward < STALL_CURRENT_THRESHOLD) {
                    calibrationMaxPeak = peakCurrentForward;
                }
            } else if (currentMode == "TEST" && totalCycles >= 3) {
                if (peakCurrentForward < breakCurrentThreshold) {
                    stopTestProcedure("ФАНЕРА РАЗРУШЕНА"); return;
                }
            }
            peakCurrentForward = 0.0; 
        }
    }
    if (sequenceStarted && (millis() - max(timeFar, timeNear) > 1500)) {
        noInterrupts(); timeFar = 0; timeNear = 0; sequenceStarted = false; interrupts();
    }
}
