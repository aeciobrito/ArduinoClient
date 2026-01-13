#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_http_server.h>
#include <ESP32Servo.h> 

// ================= PINOS FREENOVE ESP32-S3 (Fixo) =================
// CUIDADO: Não altere estes pinos, são hardcoded da placa
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13
#define LED_GPIO_NUM      2 

// ================= CONFIGURAÇÕES DE REDE =================
const char* ssid = "SUA_WIFI";
const char* password = "SUA_SENHA";
const int UDP_PORT = 4210;

// ================= NOVA PINAGEM MOTORES (Lado Direito) =================
// Usando pinos 38-41 que estão livres na Freenove S3
const int IN1 = 38; 
const int IN2 = 39; 
const int IN3 = 40; 
const int IN4 = 41; 

// ================= NOVA PINAGEM SERVOS =================
// IO2 e IO1 estão disponíveis no header
const int PIN_SERVO_PAN = 1;  
const int PIN_SERVO_TILT = 42; 

Servo panServo;
Servo tiltServo;
WiFiUDP udp;
char packetBuffer[255]; 
httpd_handle_t camera_httpd = NULL;
unsigned long lastPacketTime = 0;
const int WATCHDOG_TIMEOUT = 500; // Para se perder sinal

// ================= STREAM HTTP OTIMIZADO =================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    // CAMERA_GRAB_LATEST no setup garante que pegamos sempre o frame mais novo
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (res == ESP_OK) {
        size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      }
      if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
      }
      if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
      }
      esp_camera_fb_return(fb); // Devolve o buffer rápido para pegar o próximo
      fb = NULL;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  // Aumenta prioridade da task do servidor para reduzir lag
  config.ctrl_port = 32123; // Porta diferente para controle
  
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }
}

// ================= CONTROLE DE HARDWARE =================
void setMotor(int pin1, int pin2, int speed) {
  // ESP32 S3 usa analogWrite normalmente na versão 3.0+ da IDE
  speed = constrain(speed, -255, 255);
  
  if (speed > 0) {
    analogWrite(pin1, speed);
    analogWrite(pin2, 0);
  } else if (speed < 0) {
    analogWrite(pin1, 0);
    analogWrite(pin2, -speed);
  } else {
    analogWrite(pin1, 0);
    analogWrite(pin2, 0);
  }
}

void pararTudo() {
  setMotor(IN1, IN2, 0);
  setMotor(IN3, IN4, 0);
}

void moverCarro(int x, int y) {
  // Mixagem simples para direção diferencial (Tank drive simplificado)
  int speedLeft = y + x;
  int speedRight = y - x;

  setMotor(IN1, IN2, speedLeft);
  setMotor(IN3, IN4, speedRight);
}

void moverServos(int pan, int tilt) {
  panServo.write(constrain(pan, 0, 180));
  tiltServo.write(constrain(tilt, 0, 180));
}

// ================= SETUP =================
void setup() {
  // Pequeno delay para estabilização da serial
  delay(1000);
  Serial.begin(115200);
  // Serial.setDebugOutput(true); // Descomente se precisar debugar a câmera

  // 1. Configurar Motores
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pararTudo();

  // 2. Configurar Servos
  // Tentar alocar os servos ANTES da câmera se houver conflito de timer
  panServo.setPeriodHertz(50); 
  panServo.attach(PIN_SERVO_PAN, 500, 2400);
  tiltServo.setPeriodHertz(50);
  tiltServo.attach(PIN_SERVO_TILT, 500, 2400);
  panServo.write(90);
  tiltServo.write(90);

  // 3. Configurar Câmera (Otimizada para ESP32-S3 e Baixa Latência)
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // === AJUSTES DE PERFORMANCE ===
  // HVGA (480x320) é um excelente meio termo para S3
  config.frame_size = FRAMESIZE_HVGA; 
  config.jpeg_quality = 12; // Menor é melhor qualidade. 12 é rápido e decente.
  config.fb_count = 2; // Double buffering para streaming liso
  config.fb_location = CAMERA_FB_IN_PSRAM; // Obrigatório usar PSRAM da S3
  config.grab_mode = CAMERA_GRAB_LATEST; // Descarta frames velhos (Reduz Latência)

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err);
    return;
  }

  // 4. WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // IMPORTANTE: Desliga economia de energia para ping baixo
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("Camera Stream: http://"); Serial.println(WiFi.localIP());
  Serial.print("UDP Port: "); Serial.println(UDP_PORT);

  startCameraServer();
  udp.begin(UDP_PORT);
  lastPacketTime = millis();
}

void loop() {
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;

    int x = 0, y = 0, pan = -1, tilt = -1;
    
    // Protocolo esperado: "x,y,pan,tilt" (ex: "0,255,90,45")
    int parsed = sscanf(packetBuffer, "%d,%d,%d,%d", &x, &y, &pan, &tilt);
    
    if (parsed >= 2) {
       moverCarro(x, y);
       if(parsed >= 4 && pan != -1) {
          moverServos(pan, tilt);
       }
       lastPacketTime = millis(); 
    }
  }

  // Watchdog de Segurança
  if (millis() - lastPacketTime > WATCHDOG_TIMEOUT) {
    pararTudo();
  }
}
