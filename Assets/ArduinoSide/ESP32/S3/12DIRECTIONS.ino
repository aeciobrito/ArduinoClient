#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_http_server.h>
#include <ESP32Servo.h>
#include "driver/gpio.h" // <--- ESSENCIAL PARA O FIX DOS PINOS 38/39

/* =========================================================
   1. PINAGEM FIXA – FREENOVE ESP32-S3
   ========================================================= */
// Pinos da Câmera (Não alterar)
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

// Pinos dos Motores (L298N)
#define IN1 38   // Motor Esquerdo
#define IN2 39
#define IN3 40   // Motor Direito
#define IN4 41

// Pinos dos Servos
#define PIN_SERVO_PAN  1
#define PIN_SERVO_TILT 42 

/* =========================================================
   2. CONFIGURAÇÕES
   ========================================================= */
// PWM: Frequência baixa (1kHz) dá mais torque e estabilidade no L298N
#define PWM_FREQ 1000   
#define PWM_RES  8       // Resolução 8 bits (0-255)

const char* ssid     = "SEU_WIFI";
const char* password = "SUA_SENHA";
const int UDP_PORT   = 4210;
const int WATCHDOG_TIMEOUT = 500; // Para após 500ms sem sinal

/* =========================================================
   3. OBJETOS GLOBAIS
   ========================================================= */
Servo panServo;
Servo tiltServo;
WiFiUDP udp;
httpd_handle_t camera_httpd = NULL;

char packetBuffer[255];
unsigned long lastPacketTime = 0;

// Estado anterior dos servos (para evitar comando repetido)
int lastPan = -1;
int lastTilt = -1;

/* =========================================================
   4. FUNÇÕES DE CONTROLE DE MOTORES
   ========================================================= */
void motorEsquerdo(int v) {
  v = constrain(v, -255, 255);
  if (v > 0) {
    ledcWrite(IN1, v); ledcWrite(IN2, 0);
  } else if (v < 0) {
    ledcWrite(IN1, 0); ledcWrite(IN2, -v); // abs(v)
  } else {
    ledcWrite(IN1, 0); ledcWrite(IN2, 0);
  }
}

void motorDireito(int v) {
  v = constrain(v, -255, 255);
  if (v > 0) {
    ledcWrite(IN3, 0); ledcWrite(IN4, v);
  } else if (v < 0) {
    ledcWrite(IN3, -v); ledcWrite(IN4, 0);
  } else {
    ledcWrite(IN3, 0); ledcWrite(IN4, 0);
  }
}

void pararTudo() {
  ledcWrite(IN1, 0); ledcWrite(IN2, 0);
  ledcWrite(IN3, 0); ledcWrite(IN4, 0);
}

/* =========================================================
   5. SERVIDOR DE CÂMERA (MJPEG STREAM)
   ========================================================= */
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char * part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      size_t hlen = snprintf((char *)part_buf, 64, STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
      if (res == ESP_OK) res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
      esp_camera_fb_return(fb);
      fb = NULL;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = { "/", HTTP_GET, stream_handler, NULL };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }
}

/* =========================================================
   6. SETUP (AQUI ESTÃO AS CORREÇÕES DE HARDWARE)
   ========================================================= */
void setup() {
  Serial.begin(115200);

  // --- A. INICIALIZAR CÂMERA (Primeira Prioridade) ---
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = -1; c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_HVGA; // 480x320 (Balanço ideal Qualidade/Lag)
  c.jpeg_quality = 12;           // Menor = Melhor qualidade, mas mais lento
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&c) != ESP_OK) {
    Serial.println("ERRO: Camera falhou!");
    while(1) delay(100);
  }

  // --- B. CORREÇÃO DE PINOS JTAG (Para liberar pinos 38/39) ---
  gpio_reset_pin(GPIO_NUM_38);
  gpio_set_direction(GPIO_NUM_38, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_39);
  gpio_set_direction(GPIO_NUM_39, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_40);
  gpio_set_direction(GPIO_NUM_40, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_41);
  gpio_set_direction(GPIO_NUM_41, GPIO_MODE_OUTPUT);

  // --- C. INICIALIZAR MOTORES ---
  ledcAttach(IN1, PWM_FREQ, PWM_RES);
  ledcAttach(IN2, PWM_FREQ, PWM_RES);
  ledcAttach(IN3, PWM_FREQ, PWM_RES);
  ledcAttach(IN4, PWM_FREQ, PWM_RES);
  pararTudo();

  // --- D. INICIALIZAR SERVOS ---
  panServo.attach(PIN_SERVO_PAN, 500, 2400);
  tiltServo.attach(PIN_SERVO_TILT, 500, 2400);
  panServo.write(90);
  tiltServo.write(90);

  // --- E. WIFI ---
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // Desativa economia de energia (Reduz Lag)
  while (WiFi.status() != WL_CONNECTED) delay(300);

  startCameraServer();
  udp.begin(UDP_PORT);
  lastPacketTime = millis();
  
  Serial.println("ROBO PRONTO! Lógica de Buffer + Fix JTAG Ativos.");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

/* =========================================================
   7. LOOP PRINCIPAL (LIMPEZA DE BUFFER + LÓGICA 12h)
   ========================================================= */
void loop() {
  // Limpeza de Buffer: Lê todos os pacotes pendentes e pega só o último
  int packetSize = udp.parsePacket();
  if (packetSize) {
    while (packetSize) {
      int len = udp.read(packetBuffer, 255);
      if (len > 0) packetBuffer[len] = 0;
      // Verifica se chegou outro pacote imediatamente
      packetSize = udp.parsePacket(); 
    }

    // Agora 'packetBuffer' tem o comando mais novo (Sem Lag)
    int direction = 0, rx_speed = 0, pan = 90, tilt = 90;    
    
    if (sscanf(packetBuffer, "%d,%d,%d,%d", &direction, &rx_speed, &pan, &tilt) >= 2) {
      lastPacketTime = millis();
      
      // --- Controle de Servos (Só envia se mudar) ---
      pan = constrain(pan, 0, 180);
      tilt = constrain(tilt, 0, 180);
      
      if(pan != lastPan) { panServo.write(pan); lastPan = pan; }
      if(tilt != lastTilt) { tiltServo.write(tilt); lastTilt = tilt; }

      // --- Mapeamento de Velocidade (0-100 -> 0-255) ---
      int maxPWM = map(rx_speed, 0, 100, 0, 255);
      
      float coefEsq = 0;
      float coefDir = 0;

      // --- Lógica do Relógio (12 Direções) ---
      switch (direction) {
        case 12: // FRENTE
          coefEsq = 1.0; coefDir = 1.0; 
          break;
        case 1:  // FRENTE + DIREITA SUAVE
          coefEsq = 1.0; coefDir = 0.5; 
          break;
        case 2:  // FRENTE + DIREITA FORTE
          coefEsq = 1.0; coefDir = 0.0; 
          break;
        case 3:  // GIRO EIXO DIREITA (SPIN)
          coefEsq = 1.0; coefDir = -1.0; 
          break;
        case 4:  // RÉ + DIREITA FORTE
          coefEsq = -1.0; coefDir = 0.0; 
          break;
        case 5:  // RÉ + DIREITA SUAVE
          coefEsq = -1.0; coefDir = -0.5; 
          break;
        case 6:  // TRÁS
          coefEsq = -1.0; coefDir = -1.0; 
          break;
        case 7:  // RÉ + ESQUERDA SUAVE
          coefEsq = -0.5; coefDir = -1.0; 
          break;
        case 8:  // RÉ + ESQUERDA FORTE
          coefEsq = 0.0; coefDir = -1.0; 
          break;
        case 9:  // GIRO EIXO ESQUERDA (SPIN)
          coefEsq = -1.0; coefDir = 1.0; 
          break;
        case 10: // FRENTE + ESQUERDA FORTE
          coefEsq = 0.0; coefDir = 1.0; 
          break;
        case 11: // FRENTE + ESQUERDA SUAVE
          coefEsq = 0.5; coefDir = 1.0; 
          break;
        case 0:  // PARADO
        default:
          coefEsq = 0; coefDir = 0; 
          break;
      }

      // Aplica os coeficientes
      int pwmEsq = maxPWM * coefEsq;
      int pwmDir = maxPWM * coefDir;

      motorEsquerdo(pwmEsq);
      motorDireito(pwmDir);

    } 
  }

  // --- Watchdog (Segurança) ---
  if (millis() - lastPacketTime > WATCHDOG_TIMEOUT) {
    pararTudo();
  }
}