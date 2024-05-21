#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Values for motor to stop when closing and opening
const int START_STEPS = 100;
const int END_STEPS = 1460;
int position = 0; // current position in steps. Initially the curtain is fully open. Default number of steps for the open state is 100 [START_STEPS]

// WiFi credentials
const char* ssid = "aaa";
const char* password = "122525625";

// Light sensor
int lightThreshold = 500;
const int PHOTODIODE = A0;

// Webserver
ESP8266WebServer server(80);

// Indication LED
const int LED = 16; // D0
int ledCounter = 0;
const int LED_MAX_COUNTER = 50;
bool ledOn = false;

// H-bridge inputs
const int IN1 = 14; // D5
const int IN2 = 12; // D6
const int IN3 = 5; // D1
const int IN4 = 4; // D2

// Curtain's state
bool open = true;
bool moving = false;

// control mode
bool manual = true;

void setup() {
  startUtils();
  setServerActions(server);
  initPins();
  connectToWiFi();
  printConnectionInfo();
  startServer(server);
  Serial.println("All is SET");
}


void loop() {
  static int coilNumber = 1;
  
  energizeCoilNumber(coilNumber);

  // if automatic mode
  if (!manual) {
    //Serial.println(analogRead(PHOTODIODE));
    // if the the curtain is closed and the light threshold is exceeded or if the curtain is open and the light level is lower then the threshold
    if (!moving && (analogRead(PHOTODIODE) > lightThreshold && !open || analogRead(PHOTODIODE) < lightThreshold && open)) {
      moving = true;
      Serial.println("Moving!");
    }

    //Serial.print("ledCounter: ");
    //Serial.println(ledCounter);
    //Serial.print("photo level: ");
    //Serial.println(analogRead(PHOTODIODE));

    if (abs(analogRead(PHOTODIODE) - lightThreshold) < 200) {
      
      if (ledCounter >= LED_MAX_COUNTER) {
        if (ledOn) {
          digitalWrite(LED, LOW);
          ledOn = false;
          ledCounter = 0;
        } else {
          digitalWrite(LED, HIGH);
          ledOn = true;
          ledCounter = 0;
        }
      
        //Serial.println(analogRead(PHOTODIODE));
      }
      
      ledCounter++;
    } else {
      digitalWrite(LED, LOW);
    }


  }
  
  server.handleClient();

  if (moving && open) { // close the curtain
      coilNumber = (coilNumber + 1) % 4;
      position += 1;
  }

  if (moving && !open) { // open the curtain
      coilNumber = ((coilNumber - 1) % 4);
      position -= 1;
  }

  if (position == END_STEPS && moving && open) {
    moving = false;
    open = false;
    server.send(200);
    Serial.println("Stop. CLOSED");
  }

  if (position == START_STEPS && moving && !open) {
    moving = false;
    open = true;
    server.send(200);
    Serial.println("Stop. OPEN");
  }

  delay(10);
}

void startServer(ESP8266WebServer &server) {
  server.begin(80);
  Serial.println("Server is UP");
}

void connectToWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

// init utils
void startUtils() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Serial and WiFi are UP");
}

// set server handlers
void setServerActions(ESP8266WebServer &server) {
  server.on("/", handleHome);
  server.on("/change", onChange);
  server.on("/set", onSet);
  server.on("/switch", switchMode);
  server.on("/illumination", onIlluminationRequest);
  Serial.println("Server actions are SET");
}

// set pins mode
void initPins() {
  pinMode(LED, OUTPUT); // LED
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(PHOTODIODE, INPUT);
  Serial.println("Pins are SET");
}

// WiFi connection info
void printConnectionInfo() {
  Serial.print("Connected to Wi-Fi: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// motor coils controls
void energizeCoilNumber(int n) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  if (n < 0)
    n += 4;
    
  switch (n) {
    case 0:
      digitalWrite(IN1, HIGH);
      //Serial.println("IN1 HIGH");
      break;
    case 1:
      digitalWrite(IN2, HIGH);
      //Serial.println("IN2 HIGH");
      break;
    case 2:
      digitalWrite(IN3, HIGH);
      //Serial.println("IN3 HIGH");
      break;
    case 3:
      digitalWrite(IN4, HIGH);
      //Serial.println("IN4 HIGH");
      break;
  }
}

// "/" handler
void handleHome() {
  const char * page = "<!DOCTYPE html><html lang=\"en\"> <head> <meta charset=\"utf-8\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <title>Smart Curtain WEB</title> <style> .flex { display: flex; } .align-center { align-items: center; } .message { font-weight: bold; } .invisible { display: none; } .good { color: green; } .bad { color: red; } p { /*margin-right: 10px;*/ margin: 0 10px 0 0; padding: 0 5px; } div > button { margin-right: 10px; } div { padding: 10px 5px; } </style> </head> <body> <div class=\"flex align-center invisible\" id=\"automatic_mode_div\"> <p>Set the light level: </p> <input id=\"light_in\" type=\"text\"> <button id=\"set_light_btn\" onclick=\"onSetButtonClick()\">Set</button> <p>Current level: <span id=\"light_level\"></span></p> </div> <p id=\"light_message\" class=\"message invisible\"></p> <div class=\"flex align-center\" id=\"manual_mode_div\"> <p id=\"led_status\">Curtain is OPEN:</p> <button id=\"led_btn\" value=\"open\" onclick=\"onLedButtonClick()\">CLOSE</button> </div> <p id=\"led_message\" class=\"message invisible\"></p> <div class=\"flex align-center\"> <p id=\"switch_p\">You are in Manual mode: </p> <button id=\"switch_btn\" value=\"manual\" onclick=\"onSwitchMode()\">Switch Mode</button> </div> </body> <script> let curtainMode = \"manual\"; let intervalId = -1; let intervalIntId = -1; if (intervalIntId === -1) { console.log(\"Watchdog set\"); intervalIntId = setInterval(() => { if (intervalId === -1 && curtainMode === \"auto\") { console.log(\"Inter inter\"); intervalId = setInterval(() => { fetch(\"/illumination\") .then((response) => { if (response.ok) { return response.json(); } } ) .then((data) => { const light = document.getElementById(\"light_level\"); light.innerText = data.value; } ); }, 1000); } if (curtainMode === \"manual\" && intervalId !== -1) { console.log(\"inter cleared\"); clearInterval(intervalId); intervalId = -1; } }) } function requestLightLevel() { intervalId = this.intervalId; fetch(\"/illumination\") .then((response) => { if (response.ok) { return response.json(); } } ) .then((data) => { const light = document.getElementById(\"light_level\"); light.innerText = data.value; } ); } function setLightLevelRequestInterval() { console.log(\"Interval set interval set\"); if (document.getElementById(\"switch_btn\").value === \"auto\" && !intervalTwoSet) { setInterval(requestLightLevel, 1000); } else if (intervalId !== -1) { clearInterval(intervalId); intervalTwoSet = false; } } function makeBad(message) { message.classList.remove(\"good\", \"invisible\"); message.classList.add(\"bad\"); setTimeout((message) => { message.classList.add(\"invisible\"); }, 5000, message); } function makeGood(message) { message.classList.remove(\"bad\", \"invisible\"); message.classList.add(\"good\"); setTimeout((message) => { message.classList.add(\"invisible\"); }, 5000, message); } function setDisabled(state, ...elements) { for (let o of elements) { o.disabled = state; if (state) console.log(o.innerText + \" is disabled\"); else console.log(o.innerText + \" is enabled\"); } } function onSwitchMode() { const switchBtn = document.getElementById(\"switch_btn\"); const mode = document.getElementById(\"switch_p\"); const autoDiv = document.getElementById(\"automatic_mode_div\"); const manualDiv = document.getElementById(\"manual_mode_div\"); fetch(\"/switch\") .then(respose => { if (respose.status === 200) { if (switchBtn.value === \"manual\") { switchBtn.value = \"auto\"; mode.innerText = \"You are in Auto mode: \"; manualDiv.classList.add(\"invisible\"); autoDiv.classList.remove(\"invisible\"); curtainMode = \"auto\"; } else { switchBtn.value = \"manual\"; mode.innerText = \"You are in Manual mode: \"; manualDiv.classList.remove(\"invisible\"); autoDiv.classList.add(\"invisible\"); curtainMode = \"manual\"; } } else { console.error(\"Something went wrong. Status: \" + respose.status); } }); } function onLedButtonClick() { console.log(\"Click LED\"); const ledBtn = document.getElementById(\"led_btn\"); const ledStatus = document.getElementById(\"led_status\"); const message = document.getElementById(\"led_message\"); const setBtn = document.getElementById(\"set_light_btn\"); setDisabled(true, ledBtn, setBtn); fetch(\"/change\") .then(respose => { if (respose.status === 200) { if (ledBtn.value === \"open\") { ledBtn.value = \"closed\"; ledBtn.innerText = \"OPEN\"; ledStatus.innerText = \"Curtain is CLOSED\"; message.innerText = \"The curtain is Closing\" } else { ledBtn.value = \"open\"; ledBtn.innerText = \"CLOSE\"; ledStatus.innerText = \"Curtain is OPEN\"; } } else { console.error(\"Something went wrong. Status: \" + respose.status); message.innerText = \"Something went wrong. Status: \" + respose.status; makeBad(message); } }); setDisabled(false, ledBtn, setBtn); } function onSetButtonClick() { console.log(\"Click SET\"); const input = document.getElementById(\"light_in\"); const message = document.getElementById(\"light_message\"); let value = parseInt(input.value); if (isNaN(value) || value < 0 || value > 100) { input.value = \"\"; message.innerText = \"Invalid value!\"; makeBad(message); return; } value = 1023 / 100 * value; console.log(\"Value sent: \" + value); fetch(\"/set?val=\" + value) .then(response => { if (response.status === 200) { input.value = \"\"; message.innerText = \"Successfully set!\"; makeGood(message); } else { input.value = \"\"; message.innerText = \"Something went wrong. Response status: \" + response.status; makeBad(message); return; } }); } </script></html>";
  server.send(200, "text/html", page);
}

// "/mode" handler
void switchMode() {
  if (manual)
    manual = false;
  else
    manual = true;
  Serial.print("Mode switched to: ");
  Serial.println(manual ? "Manual" : "Auto");
  server.send(200);
}

// "/change" handler
void onChange() {
  if (!moving) {
    moving = true;
    Serial.println("Moving!");
  }
}

// "/set" handler 
void onSet() {
    String value = server.arg("val");
    Serial.println("Received value: " + value);
    lightThreshold = value.toInt();
    server.send(200, "text/plain", "Received value: " + value);
}

void onIlluminationRequest() {
  int value = analogRead(PHOTODIODE);
  String strValue = "{\"value\":\"" + String(value * 100 / 1024) + "%\"}";
  Serial.println(value);
  Serial.println(value / 1024 * 100);
  Serial.print("Value sent: ");
  Serial.println(strValue);
  server.send(200, "application/json", strValue);
}