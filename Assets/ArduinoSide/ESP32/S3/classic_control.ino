#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_http_server.h"

// =================================================================================
// 1. CONFIGURAÇÕES
// =================================================================================
const char* ssid = "Vivo 6G";
const char* password = "d4c3b21A";
unsigned int udpPort = 4210;

// Ajustes de Física
const int MIN_MOTOR_PWM = 100; 
const int INPUT_DEADZONE = 10;
float accelRate = 8.0;   
float brakeRate = 16.0;  

// Loop de 60Hz (16ms)
const int TARGET_FPS = 60;
const int SCREEN_DELAY = 1000 / TARGET_FPS; 

// =================================================================================
// 2. HARDWARE
// =================================================================================
const int IN1 = 38;
const int IN2 = 39;
const int IN3 = 40;
const int IN4 = 41;

// Pinos Câmera (Freenove S3)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y2_GPIO_NUM       11
#define Y3_GPIO_NUM       9
#define Y4_GPIO_NUM       8
#define Y5_GPIO_NUM       10
#define Y6_GPIO_NUM       12
#define Y7_GPIO_NUM       18
#define Y8_GPIO_NUM       17
#define Y9_GPIO_NUM       16
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// =================================================================================
// 3. VARIÁVEIS GLOBAIS
// =================================================================================
WiFiUDP udp;
volatile int targetSpeed = 0;
volatile int targetDir = 0;
float currentSpeed = 0.0;
unsigned long lastPacketTime = 0;
const unsigned long TIMEOUT_MS = 500; 

// =================================================================================
// 4. WEBPAGE (Minificada para economizar memória)
// =================================================================================
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{margin:0;background:#000;display:flex;justify-content:center;align-items:center;height:100vh}img{max-width:100%;max-height:100%;object-fit:contain}</style></head><body><img src="/stream"></body></html>)rawliteral";

// =================================================================================
// 5. FUNÇÕES AUXILIARES (INLINE para Performance)
// =================================================================================

// "inline" sugere ao compilador copiar o código direto onde é chamado, evitando overhead de pulo
inline int mapPWM(int val) {
  if (abs(val) < INPUT_DEADZONE) return 0;
  return map(abs(val), INPUT_DEADZONE, 255, MIN_MOTOR_PWM, 255);
}

void applyMotors(int speed, int direction) {
  float leftMod = 1.0;
  float rightMod = 1.0;

  if (direction > 0) { 
    rightMod = 1.0 - (abs(direction) / 127.0); 
  } else if (direction < 0) { 
    leftMod = 1.0 - (abs(direction) / 127.0);  
  }

  int rawLeft = speed * leftMod;
  int rawRight = speed * rightMod;

  int pwmLeft = mapPWM(rawLeft);
  int pwmRight = mapPWM(rawRight);

  // Motor Esquerdo
  if (rawLeft > 0) { 
    analogWrite(IN1, pwmLeft);
    digitalWrite(IN2, LOW);
  } else if (rawLeft < 0) {
    digitalWrite(IN1, LOW);
    analogWrite(IN2, pwmLeft);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  // Motor Direito
  if (rawRight > 0) {
    analogWrite(IN3, pwmRight);
    digitalWrite(IN4, LOW);
  } else if (rawRight < 0) {
    digitalWrite(IN3, LOW);
    analogWrite(IN4, pwmRight);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }
}

// =================================================================================
// 6. STREAMING HANDLERS
// =================================================================================
esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];
  static const char* _STREAM_BOUNDARY = "123456789000000000000987654321";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      size_t hlen = snprintf((char *)part_buf, 64, "\r\n--%s\r\n", _STREAM_BOUNDARY);
      httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
      httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
    if (res != ESP_OK) break;
    // OTIMIZAÇÃO: Delay mínimo absoluto para dar fôlego ao Watchdog e TCP/IP
    vTaskDelay(1); 
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  
  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
  }
}

// =================================================================================
// 7. SETUP OTIMIZADO
// =================================================================================
void setup() {
  // Config Motors
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);

  // Config Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; 
  config.jpeg_quality = 12;           
  config.fb_count = 2;               
  config.grab_mode = CAMERA_GRAB_LATEST;

  psramInit(); // Tenta iniciar PSRAM silenciosamente
  esp_camera_init(&config);

  // Config WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // MAX PERFORMANCE MODE
  
  // Bloqueio silencioso até conectar (Opcional: piscar um LED aqui seria útil)
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  // Serviços
  udp.begin(udpPort);
  startCameraServer();
}

// =================================================================================
// 8. LOOP PRINCIPAL
// =================================================================================
void loop() {
  unsigned long loopStart = millis();

  // 1. UDP FLUSH (Lê até esvaziar o buffer)
  int packetSize = udp.parsePacket();
  
  while (packetSize) {
    char packetBuffer[255];
    int len = udp.read(packetBuffer, 254);
    
    if (len > 0) {
      packetBuffer[len] = 0;
      int v, d, p, t;
      // Parsing "Vel,Dir,Pan,Tilt"
      if (sscanf(packetBuffer, "%d,%d,%d,%d", &v, &d, &p, &t) == 4) {
        targetSpeed = v;
        targetDir = d;
        lastPacketTime = millis();
      }
    }
    packetSize = udp.parsePacket(); 
  }

  // 2. FAILSAFE
  if (millis() - lastPacketTime > TIMEOUT_MS) {
    targetSpeed = 0;
    targetDir = 0;
  }

  // 3. FÍSICA
  if (currentSpeed < targetSpeed) {
    currentSpeed += accelRate;
    if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  } else if (currentSpeed > targetSpeed) {
    currentSpeed -= brakeRate;
    if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  if (abs(currentSpeed) < 1.0) currentSpeed = 0;

  // 4. HARDWARE UPDATE
  applyMotors((int)currentSpeed, targetDir);

  // 5. FPS CONTROL
  unsigned long loopTime = millis() - loopStart;
  if (loopTime < SCREEN_DELAY) {
    delay(SCREEN_DELAY - loopTime); 
  }
}
