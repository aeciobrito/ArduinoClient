#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ================= CONFIGURAÇÕES DE REDE =================
const char* ssid = "NOME_DA_SUA_REDE";
const char* password = "SENHA_DA_SUA_REDE";
const int UDP_PORT = 4210;

// ================= CONFIGURAÇÕES DOS MOTORES =================
// Ajuste os pinos conforme sua ligação física
// Motor A (Esquerda)
const int IN1 = 12;
const int IN2 = 13;
// Motor B (Direita)
const int IN3 = 14;
const int IN4 = 15;

// ================= PINAGEM DA CÂMERA (AI THINKER) =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= VARIÁVEIS GLOBAIS =================
WiFiUDP udp;
char packetBuffer[255]; 
httpd_handle_t camera_httpd = NULL;

// Variáveis de Segurança (Watchdog)
unsigned long lastPacketTime = 0;
const int WATCHDOG_TIMEOUT = 500; // 500ms sem sinal = PARAR

// ================= FUNÇÕES DE CONTROLE DE MOTOR =================

// Função auxiliar para controlar um único motor
// speed: -255 a 255 (negativo = ré)
void setMotor(int pin1, int pin2, int speed) {
  if (speed > 0) {
    analogWrite(pin1, speed);
    analogWrite(pin2, 0);
  } else if (speed < 0) {
    analogWrite(pin1, 0);
    analogWrite(pin2, -speed); // Inverte o sinal para PWM positivo
  } else {
    analogWrite(pin1, 0);
    analogWrite(pin2, 0);
  }
}

void pararTudo() {
  setMotor(IN1, IN2, 0);
  setMotor(IN3, IN4, 0);
  // Serial.println("Watchdog: Parada de emergência!");
}

// Lógica de "Mistura" (Differential Drive)
// Transforma Joystick X/Y em força para motor Esquerdo/Direito
void moverCarro(int x, int y) {
  // x e y vêm do Unity entre -100 e 100. Vamos converter para PWM (0-255)
  // Multiplicador 2.55 converte a escala 100 para 255
  
  float speedLeft = (y + x) * 2.55;
  float speedRight = (y - x) * 2.55;

  // Limitar valores entre -255 e 255
  speedLeft = constrain(speedLeft, -255, 255);
  speedRight = constrain(speedRight, -255, 255);

  setMotor(IN1, IN2, (int)speedLeft);
  setMotor(IN3, IN4, (int)speedRight);
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
  config.server_port = 80; // Porta HTTP Padrão
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
  // Desativa brownout detector (evita reset quando a bateria oscila)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);

  // 1. Configurar Pinos dos Motores
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pararTudo();

  // 2. Configurar WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Conectado!");
  Serial.print("Stream MJPEG: http://"); Serial.print(WiFi.localIP()); Serial.println("/");
  Serial.print("UDP Server: "); Serial.print(WiFi.localIP()); Serial.printf(":%d\n", UDP_PORT);

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
  config.pin_d8 = XCLK_GPIO_NUM;
  config.pin_d9 = SIOD_GPIO_NUM;
  config.pin_d10 = SIOC_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA; // Tamanho do vídeo (ajuste se ficar lento)
  config.jpeg_quality = 12;          // Qualidade (10-63, menor é melhor)
  config.fb_count = 1;

  esp_camera_init(&config);
  
  // 4. Iniciar Serviços
  startCameraServer();
  udp.begin(UDP_PORT);
  
  lastPacketTime = millis(); // Inicializa timer
}

void loop() {
  // 1. Verificação UDP (Prioridade Máxima)
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0; // Null terminate string

    // Formato esperado: "x,y" (ex: "50,-100")
    int x = 0, y = 0;
    
    // Parse simples da string
    if (sscanf(packetBuffer, "%d,%d", &x, &y) == 2) {
       moverCarro(x, y);
       lastPacketTime = millis(); // Reset Watchdog apenas se o comando for válido
    }
  }

  // 2. Watchdog de Segurança
  // Se não receber NADA (nem 0,0, nem movimento) por 500ms -> Para tudo
  if (millis() - lastPacketTime > WATCHDOG_TIMEOUT) {
    pararTudo();
  }
  
  // O Stream de vídeo roda sozinho via interrupções, não precisa de código no loop
}
