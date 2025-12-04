#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_http_server.h>
#include "img_converters.h"
#include <ESP32Servo.h> // <--- NECESSÁRIO INSTALAR ESSA BIBLIOTECA
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ================= DEFINIÇÕES HTTP =================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ================= CONFIGURAÇÕES DE REDE =================
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";
const int UDP_PORT = 4210;

// ================= CONFIGURAÇÕES DOS MOTORES DC =================
// Motor A (Esquerda)
const int IN1 = 12; // Pino físico IO12
const int IN2 = 13; // Pino físico IO13

// Motor B (Direita)
const int IN3 = 14; // Pino físico IO14
const int IN4 = 15; // Pino físico IO15

// ================= CONFIGURAÇÕES DOS SERVO MOTORES =================
const int PIN_SERVO_PAN = 2;  // Pino IO2 (LED pequeno onboard)
const int PIN_SERVO_TILT = 4; // Pino IO4 (LED Flash onboard - vai piscar!)

Servo panServo;
Servo tiltServo;

// ================= PINAGEM DA CÂMERA (AI THINKER) =================
// Não conectar fios aqui. Conexões internas.
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= VARIÁVEIS GLOBAIS =================
WiFiUDP udp;
char packetBuffer[255]; 
httpd_handle_t camera_httpd = NULL;

unsigned long lastPacketTime = 0;
const int WATCHDOG_TIMEOUT = 500; // 500ms sem sinal = PARAR

// ================= FUNÇÕES DE CONTROLE =================

void setMotor(int pin1, int pin2, int speed) {
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
  float speedLeft = (y + x) * 2.55;
  float speedRight = (y - x) * 2.55;

  speedLeft = constrain(speedLeft, -255, 255);
  speedRight = constrain(speedRight, -255, 255);

  setMotor(IN1, IN2, (int)speedLeft);
  setMotor(IN3, IN4, (int)speedRight);
}

void moverServos(int pan, int tilt) {
  // Limita os ângulos para evitar forçar o servo (0 a 180 é o padrão)
  pan = constrain(pan, 0, 180);
  tilt = constrain(tilt, 0, 180);
  
  panServo.write(pan);
  tiltServo.write(tilt);
}

// ================= FUNÇÕES DA CÂMERA (STREAM HTTP) =================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Falha na captura da câmera");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("Erro na compressão JPEG");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
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

// ================= SETUP E LOOP =================

void setup() {
  setCpuFrequencyMhz(160);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);

  // 1. Inicializar Motores DC
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pararTudo();

  // 2. Inicializar Servos
  // Nota: A biblioteca aloca timers. Se der conflito com a câmera, 
  // tente inverter a ordem (inicializar servos DEPOIS da câmera).
  panServo.setPeriodHertz(50); 
  panServo.attach(PIN_SERVO_PAN, 500, 2400); // Pino 2
  tiltServo.setPeriodHertz(50);
  tiltServo.attach(PIN_SERVO_TILT, 500, 2400); // Pino 4
  
  // Posição inicial (Centro)
  panServo.write(90);
  tiltServo.write(90);

  // 3. Configurar Câmera
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
  
  // Frequência ajustada para estabilidade
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; 
  config.jpeg_quality = 20;
  
  if (psramFound()) {
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_camera_init(&config);
  
  // 4. WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("Stream: http://"); Serial.print(WiFi.localIP()); Serial.println("/");
  Serial.print("UDP IP: "); Serial.println(WiFi.localIP()); 

  startCameraServer();
  udp.begin(UDP_PORT);
  
  lastPacketTime = millis();
}

void loop() {
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;

    int x = 0, y = 0, pan = 90, tilt = 90;
    
    // Parse UDP: espera "x,y,pan,tilt"
    // Ex: "0,100,90,45"
    if (sscanf(packetBuffer, "%d,%d,%d,%d", &x, &y, &pan, &tilt) >= 2) {
       moverCarro(x, y);
       // Se o pacote tiver 4 valores, move os servos. Se tiver só 2, ignora servos.
       if(pan >= 0) moverServos(pan, tilt); 
       
       lastPacketTime = millis(); 
    }
  }

  // Watchdog
  if (millis() - lastPacketTime > WATCHDOG_TIMEOUT) {
    pararTudo();
  }
}