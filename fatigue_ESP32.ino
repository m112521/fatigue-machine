#include <TB6612_ESP32.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

#define HALL_PIN 18
#define END_PIN 16
#define MOSFET 15

const char* ssid = "GalaxyAzamat";
const char* password = "vhrg8328";

int fire = 0;
int hall = 0;
int end = 0;

volatile int count = 0;

int hallState = LOW; 
int magState = -1; 

long lastDebounceTime = 0;  
long debounceDelay = 50;


AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

JSONVar readings;

const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<style>
    body {height: 100vh;overflow: hidden;margin: 0;}
    .parent {
        display: flex;flex-direction: column;
        }
        .div2{display: flex;align-content: center;justify-content: center;}
        .div3{display: flex;align-content: center;justify-content: center;}
        .analog-value{font-size:15rem;}
        .circ-btn{border-radius:100%;border:1px solid red;background:none;width:280px;height:280px;font-size:2rem;}
</style>
<div class="parent">
    <div class="div2">
        <div class="analog-value" id="slider-txt">0</div>
    </div>
    <div class="div3">
        <button class="circ-btn" id="btn-fire">START</button>
    </div>
</div>

<script>
let gateway = `ws://${window.location.hostname}/ws`;

let sliderTxt = document.querySelector("#slider-txt");
let fireBtn = document.querySelector("#btn-fire");
let fireState = 0;

let websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
    initButtons();
}

function initButtons() {  
  document.querySelector('#btn-fire').addEventListener('click', ()=>{ 
    fireState = !fireState;
    toggleBg(fireBtn, fireState);
    websocket.send(JSON.stringify({fire:fireState ? 1 : 0})); 
  });
}

function toggleBg(btn, state) {
    if (state) {
        btn.style.background = '#ff0000';
    }
    else {
        btn.style.background = '#ffffff';
    }
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function getReadings(){
    websocket.send("getReadings");
}

function onMessage(event) {
    //console.log(event.data);
    let json = JSON.parse(event.data)
    document.querySelector("#slider-txt").innerHTML = json.count;

    if (parseInt(json.end) == 1) {
      fireState = !fireState;
      toggleBg(fireBtn, fireState);
    }
    websocket.send("getReadings");
}</script>
)rawliteral";

// void IRAM_ATTR ISR(){
//   count++;
// }

// void IRAM_ATTR ISR2(){
//   digitalWrite(MOSFET, LOW);
// }

String getSensorReadings(){
  readings["count"] = String(count);
  readings["end"] = String(end);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    
    JSONVar myObject = JSON.parse((const char*)data);
    if (myObject.hasOwnProperty("fire")) {
      fire = (int)myObject["fire"];      
    }

    String sensorReadings = getSensorReadings();
    notifyClients(sensorReadings);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  pinMode(HALL_PIN, INPUT);
  pinMode(END_PIN, INPUT);
  pinMode(MOSFET, OUTPUT);
  
  //attachInterrupt(digitalPinToInterrupt(HALL_PIN), ISR, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(END_PIN), ISR2, CHANGE);

  initWiFi();
  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.begin();
}

void loop() {
  hallState = digitalRead(HALL_PIN);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if ((hallState == LOW) && (magState < 0)) {
      count++;
      magState = -magState;
      lastDebounceTime = millis();
    }
    else if ((hallState == LOW) && (magState > 0)) {
      count++;
      magState = -magState;
      lastDebounceTime = millis(); 
    }
  }

  if (fire == 1) {
    digitalWrite(MOSFET, HIGH);
    count = 0;
    end = 0;
  }

  if (digitalRead(END_PIN) == HIGH) {
    end = 1;
    digitalWrite(MOSFET, LOW);
  }

  //Serial.println(count);
}

