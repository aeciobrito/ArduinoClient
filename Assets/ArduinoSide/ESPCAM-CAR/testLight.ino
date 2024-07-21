#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#define LED_PIN 4

const char* routerSsid = "Vivo";
const char* routerPassword = "d4c3b21a";

WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang='pt-br'>
  <head>
    <title>Controle de LED</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial, sans-serif; text-align: center; }
      button { padding: 10px 20px; font-size: 20px; }
    </style>
  </head>
  <body>
    <h1>Controle de LED</h1>
    <button onclick="toggleLED()">Toggle LED</button>
    <script>
      function toggleLED() {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/toggle", true);
        xhr.send();
      }
    </script>
  </body>
</html>
)rawliteral";

// Function to handle the root URL "/"
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Function to handle the "/toggle" URL
void handleToggle() {
  int state = digitalRead(LED_PIN);
  digitalWrite(LED_PIN, !state); // Toggle LED state
  server.send(200, "text/plain", "LED Toggled");
}

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(routerSsid, routerPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("Connecting to WiFi...");
  }
  Serial.print("Connected to WiFi: ");
  Serial.println(WiFi.localIP());

  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Initialize the LED to be off

  if (MDNS.begin("esplight")) {
    Serial.println("MDNS responder initialized");
  }

  // Define the routes and handlers
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
