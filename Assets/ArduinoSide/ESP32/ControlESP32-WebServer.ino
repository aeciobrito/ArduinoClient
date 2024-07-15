#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Car control variables
int speedCar = 200; // 0 to 255 for ESP32
int speed_Coeff = 3; // aux reduce turn speed

#define ENA   26          // Enable/speed motors Right
#define ENB   27          // Enable/speed motors Left
#define IN_1  14          // L298N in1 motors Right
#define IN_2  12          // L298N in2 motors Right
#define IN_3  13          // L298N in3 motors Left
#define IN_4  15          // L298N in4 motors Left

const char* ssid = "Vivo";
const char* password = "d4c3b21a";

WebServer server(80);

const char* htmlPage = R"rawliteral(
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 0;
    }
    h1 {
      color: #333;
      margin-top: 20px;
    }
    .grid-container {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      max-width: 90%;
      margin: 0 auto;
    }
    button {
      padding: 20px;
      font-size: 20px;
      border: none;
      background-color: #4CAF50;
      color: white;
      border-radius: 5px;
      cursor: pointer;
    }
    button:active {
      background-color: #45a049;
    }
    .slider-container {
      margin: 20px auto;
      max-width: 90%;
    }
    .slider-label {
      font-size: 18px;
    }
    .slider {
      width: 100%;
    }
  </style>
</head>
<body>
  <h1>ESP32 Car Control</h1>
  <div class="grid-container">
    <button id="forwardLeft">Forward Left (Q)</button>
    <button id="forward">Forward (W)</button>
    <button id="forwardRight">Forward Right (E)</button>
    <button id="left">Left (A)</button>
    <button id="stop">Stop (S)</button>
    <button id="right">Right (D)</button>
    <button id="backwardLeft">Backward Left (Z)</button>
    <button id="backward">Backward (X)</button>
    <button id="backwardRight">Backward Right (C)</button>
  </div>
  <div class="slider-container">
    <label for="speedSlider" class="slider-label">Speed: <span id="speedValue">200</span></label>
    <input type="range" id="speedSlider" class="slider" min="0" max="255" value="200">
  </div>
  <script>
    function sendCommand(command) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/' + command, true);
      xhr.send();
    }
    function addControlEvents(id, command) {
      var button = document.getElementById(id);
      button.addEventListener('mousedown', function() { sendCommand(command); });
      button.addEventListener('mouseup', function() { sendCommand('stop'); });
      button.addEventListener('touchstart', function() { sendCommand(command); });
      button.addEventListener('touchend', function() { sendCommand('stop'); });
    }
    ['forwardLeft', 'forward', 'forwardRight', 'left', 'right', 'backwardLeft', 'backward', 'backwardRight'].forEach(function(id) {
      addControlEvents(id, id);
    });
    document.getElementById('stop').addEventListener('mousedown', function() { sendCommand('stop'); });
    document.getElementById('stop').addEventListener('touchstart', function() { sendCommand('stop'); });
    
    var slider = document.getElementById('speedSlider');
    var speedValue = document.getElementById('speedValue');
    slider.addEventListener('input', function() {
      speedValue.textContent = slider.value;
    });
    slider.addEventListener('change', function() {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/speed/' + slider.value, true);
      xhr.send();
    });
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  if (MDNS.begin("car4wd")) {
    Serial.println("MDNS responder inicializado");
  }

  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/forwardRight", handleForwardRight);
  server.on("/forwardLeft", handleForwardLeft);
  server.on("/backwardRight", handleBackwardRight);
  server.on("/backwardLeft", handleBackwardLeft);
  server.on("/stop", handleStop);
  server.on("/speed", HTTP_GET, handleSpeed);

  server.begin();
  Serial.println("HTTP server started");

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN_1, OUTPUT);
  pinMode(IN_2, OUTPUT);
  pinMode(IN_3, OUTPUT);
  pinMode(IN_4, OUTPUT);
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleForward() {
  forward();
  server.send(200, "text/plain", "Forward command received.");
}

void handleBackward() {
  backward();
  server.send(200, "text/plain", "Backward command received.");
}

void handleLeft() {
  left();
  server.send(200, "text/plain", "Left command received.");
}

void handleRight() {
  right();
  server.send(200, "text/plain", "Right command received.");
}

void handleStop() {
  stop();
  server.send(200, "text/plain", "Stop command received.");
}

void handleForwardRight() {
  forwardRight();
  server.send(200, "text/plain", "Forward Right command received.");
}

void handleForwardLeft() {
  forwardLeft();
  server.send(200, "text/plain", "Forward Left command received.");
}

void handleBackwardRight() {
  backwardRight();
  server.send(200, "text/plain", "Backward Right command received.");
}

void handleBackwardLeft() {
  backwardLeft();
  server.send(200, "text/plain", "Backward Left command received.");
}

void handleSpeed() {
  if (server.hasArg("plain")) {
    speedCar = server.arg("plain").toInt();
    Serial.printf("Speed set to %d\n", speedCar);
  }
  server.send(200, "text/plain", "Speed command received.");
}

void right() {
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar);
}

void left() {
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar);
}

void backward() {
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar);
}

void forward() {
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar);
}

void forwardRight() {
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar / speed_Coeff);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar);
}

void forwardLeft() {
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar / speed_Coeff);
}

void backwardRight() {
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar / speed_Coeff);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar);
}

void backwardLeft() {
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar / speed_Coeff);
}

void stop() {
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, LOW);
  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, LOW);
}
