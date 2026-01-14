#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_http_server.h>
#include <ESP32Servo.h>

/* =========================================================
   PINAGEM FIXA – FREENOVE ESP32-S3 (CÂMERA)
   ========================================================= */
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

/* =========================================================
   MOTORES – L298N
   ========================================================= */
#define IN1 38   // Motor esquerdo A
#define IN2 39   // Motor esquerdo B
#define IN3 40   // Motor direito A
#define IN4 41   // Motor direito B
// ENA / ENB: jumpers ligados em 5V

/* =========================================================
   PWM (ESP32-S3 – CORE 3.x)
   ========================================================= */
#define PWM_FREQ 20000   // 20 kHz
#define PWM_RES  8       // 0–255

/* =========================================================
   SERVOS
   ========================================================= */
#define PIN_SERVO_PAN  1
#define PIN_SERVO_TILT 42

Servo panServo;
Servo tiltServo;

/* =========================================================
   REDE
   ========================================================= */
const char* ssid     = "SEU_WIFI";
const char* password = "SUA_SENHA";
const int UDP_PORT   = 4210;

WiFiUDP udp;
char packetBuffer[255];
unsigned long lastPacketTime = 0;
const int WATCHDOG_TIMEOUT = 500;

/* =========================================================
   STREAM HTTP – CÂMERA
   ========================================================= */
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;

  if (httpd_resp_set_type(req, STREAM_TYPE) != ESP_OK)
    return ESP_FAIL;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) break;

    char buf[64];
    size_t len = snprintf(buf, sizeof(buf), STREAM_PART, fb->len);
    httpd_resp_send_chunk(req, buf, len);
    httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

    esp_camera_fb_return(fb);
  }
  return ESP_OK;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_uri_t uri = { "/", HTTP_GET, stream_handler, NULL };
  httpd_start(&camera_httpd, &config);
  httpd_register_uri_handler(camera_httpd, &uri);
}

/* =========================================================
   MOTORES – CONTROLE
   ========================================================= */
void motorEsquerdo(int v) {
  v = constrain(v, -255, 255);
  if (v > 0) {
    ledcWrite(IN1, v);
    ledcWrite(IN2, 0);
  } else if (v < 0) {
    ledcWrite(IN1, 0);
    ledcWrite(IN2, -v);
  } else {
    ledcWrite(IN1, 0);
    ledcWrite(IN2, 0);
  }
}

void motorDireito(int v) {
  v = constrain(v, -255, 255);
  if (v > 0) {
    ledcWrite(IN3, 0);
    ledcWrite(IN4, v);
  } else if (v < 0) {
    ledcWrite(IN3, -v);
    ledcWrite(IN4, 0);
  } else {
    ledcWrite(IN3, 0);
    ledcWrite(IN4, 0);
  }
}

void pararTudo() {
  ledcWrite(IN1, 0);
  ledcWrite(IN2, 0);
  ledcWrite(IN3, 0);
  ledcWrite(IN4, 0);
}

void moverCarro(int eixoFrente, int eixoGiro) {
  int minPWM = 80;

  int l = constrain(eixoFrente + eixoGiro, -100, 100);
  int r = constrain(eixoFrente - eixoGiro, -100, 100);

  int pl = l * 2.55;
  int pr = r * 2.55;

  if (abs(pl) > 10 && abs(pl) < minPWM) pl = pl > 0 ? minPWM : -minPWM;
  if (abs(pr) > 10 && abs(pr) < minPWM) pr = pr > 0 ? minPWM : -minPWM;

  motorEsquerdo(pl);
  motorDireito(pr);
}

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
  Serial.begin(115200);

  // =========================================================================
  // 1. INICIALIZAR CÂMERA PRIMEIRO (Prioridade Crítica de Hardware)
  // =========================================================================
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0; // A câmera VAI roubar o canal 0
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = -1;
  c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_HVGA;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err);
    // Loop infinito se a câmera falhar, para você saber
    while(true) { delay(100); } 
  }

  // =========================================================================
  // 2. AGORA INICIALIZAR MOTORES (Vão pegar os canais PWM restantes)
  // =========================================================================
  // Como o Canal 0 já foi tomado pela câmera, o ledcAttach usará os próximos livres.
  ledcAttach(IN1, PWM_FREQ, PWM_RES);
  ledcAttach(IN2, PWM_FREQ, PWM_RES);
  ledcAttach(IN3, PWM_FREQ, PWM_RES);
  ledcAttach(IN4, PWM_FREQ, PWM_RES);
  pararTudo();

  // 3. Servos
  panServo.attach(PIN_SERVO_PAN, 500, 2400);
  tiltServo.attach(PIN_SERVO_TILT, 500, 2400);
  panServo.write(90);
  tiltServo.write(90);

  // 4. Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  startCameraServer();
  udp.begin(UDP_PORT);
  lastPacketTime = millis();
  
  Serial.println("Sistema Pronto! Câmera e Motores Inicializados.");
}

/* =========================================================
   LOOP
   ========================================================= */
void loop() {
  int p = udp.parsePacket();
  if (p) {
    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    packetBuffer[len] = 0;

    int x = 0, y = 0, pan = 90, tilt = 90;
    if (sscanf(packetBuffer, "%d,%d,%d,%d", &x, &y, &pan, &tilt) >= 2) {
      moverCarro(y, x);
      panServo.write(constrain(pan, 0, 180));
      tiltServo.write(constrain(tilt, 0, 180));
      lastPacketTime = millis();
    }
  }

  if (millis() - lastPacketTime > WATCHDOG_TIMEOUT) {
    pararTudo();
  }
}
