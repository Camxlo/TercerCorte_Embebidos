/*
  Controlador.ino
  Nodo 3 = Controlador (AI Thinker ESP32-CAM)
*/

#include <WiFi.h>
#include <esp_now.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ——————————————————————————
//  
// ——————————————————————————
uint8_t sensorMac[6]   = { 0x24, 0x05, 0x0E, 0x10, 0x20, 0x30 };
uint8_t actuatorMac[6] = { 0x24, 0x05, 0x0E, 0x10, 0x20, 0x31 };

// ——————————————————————————
//  ESTRUCTURAS DE MENSAJE
// ——————————————————————————
typedef struct {
  float temp;
  float light;
  float soil;
} SensorData_t;

typedef struct {
  bool fanOn;
  bool pumpOn;
  bool rgbOn;
  bool buzzerOn;
} ActuatorCmd_t;

// Cola FreeRTOS para intercambio de datos
QueueHandle_t sensorQueue;

// ——————————————————————————
//  CREDENCIALES Wi-Fi & Telegram
// ——————————————————————————
#define WIFI_SSID     "Llanten_Castrillon_5G"
#define WIFI_PASSWORD "AlexAleida1967"
#define BOT_TOKEN     "7509148504:AAGPQtShVO3GK0OvuqCqYwK9wxnv6j6nX0s"
const String CHAT_ID = "1784417973";

WiFiClientSecure secured;
UniversalTelegramBot bot(BOT_TOKEN, secured);

// ——————————————————————————
//  UMBRALES
// ——————————————————————————
float T_THRESH = 24.0;
float L_THRESH = 500.0;
float S_THRESH = 50.0;

// ——————————————————————————
//  SD CAM (SD_MMC)
// ——————————————————————————
void initSDCard() {
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("❌ Fallo al montar SD");
  } else {
    Serial.println("✅ SD lista");
  }
}

// ——————————————————————————
//  TIMESTAMP Y RUTINAS DE LOG
// ——————————————————————————
String timestamp() {
  struct tm tm; getLocalTime(&tm);
  char buf[16]; strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
  return String(buf);
}
String fechaCarpeta() {
  struct tm tm; getLocalTime(&tm);
  char buf[16]; strftime(buf, sizeof(buf), "/%Y%m%d", &tm);
  return String(buf);
}
String horaFichero() {
  struct tm tm; getLocalTime(&tm);
  char buf[8]; strftime(buf, sizeof(buf), "/%H.txt", &tm);
  return String(buf);
}

void logToSD(const SensorData_t &d) {
  String dir = fechaCarpeta();
  if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
  String file = dir + horaFichero();
  File f = SD_MMC.open(file, FILE_APPEND);
  if (!f) {
    Serial.println("❌ Error abriendo " + file);
    return;
  }
  // Formato CSV: hora,temp,light,soil
  f.printf("%s,%.2f,%.0f,%.0f\n",
           timestamp().c_str(), d.temp, d.light, d.soil);
  f.close();
}

// ——————————————————————————
//  CALLBACK recepción desde Sensores
// ——————————————————————————
void onSensorRecv(const esp_now_recv_info_t* info,
                  const uint8_t* data, int len) {
  if (len != sizeof(SensorData_t)) return;
  SensorData_t d;
  memcpy(&d, data, sizeof(d));
  xQueueSendFromISR(sensorQueue, &d, NULL);
}

// ——————————————————————————
//  TAREA FreeRTOS: procesa datos y envía comandos
// ——————————————————————————
void TaskProcess(void *pv) {
  SensorData_t d;
  ActuatorCmd_t cmd;
  for (;;) {
    if (xQueueReceive(sensorQueue, &d, portMAX_DELAY)) {
      // 1) Log en SD
      logToSD(d);

      // 2) Alertas Telegram
      if (d.temp  > T_THRESH) bot.sendMessage(CHAT_ID, "🌡 Temp > "  + String(d.temp), "");
      if (d.light > L_THRESH) bot.sendMessage(CHAT_ID, "💡 Luz > "   + String(d.light), "");
      if (d.soil  > S_THRESH) bot.sendMessage(CHAT_ID, "💦 Suelo > " + String(d.soil),  "");

      // 3) Decisión de actuadores
      cmd.fanOn    = (d.temp  > T_THRESH);
      cmd.pumpOn   = (d.soil  > S_THRESH);
      cmd.rgbOn    = (d.light > L_THRESH);
      cmd.buzzerOn = (d.soil  > S_THRESH);

      // 4) Envío comando a Actuadores
      esp_now_send(actuatorMac, (uint8_t*)&cmd, sizeof(cmd));
    }
  }
}

// ——————————————————————————
//  SETUP principal
// ——————————————————————————
void setup() {
  // 1) Inicializar puerto serie
  Serial.begin(115200);
  delay(500);

  // 2) Imprimir MAC del Controlador
  WiFi.mode(WIFI_STA);
  Serial.print("MAC Controlador: ");
  Serial.println(WiFi.macAddress());

  // 3) SD + Wi-Fi (para Telegram)
  initSDCard();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  secured.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  configTime(0,0,"pool.ntp.org");
  while (time(nullptr) < 8*3600) delay(200);

  // 4) Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error al iniciar ESP-NOW");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(onSensorRecv);

  // 5) Añadir peer del nodo Sensores
  {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, sensorMac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  // 6) Añadir peer del nodo Actuadores
  {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, actuatorMac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }

  // 7) Crear cola y lanzar tarea de procesamiento
  sensorQueue = xQueueCreate(4, sizeof(SensorData_t));
  xTaskCreatePinnedToCore(
    TaskProcess,    // función
    "ProcSens",     // nombre
    8192,           // stack
    NULL,           // parámetro
    1,              // prioridad
    NULL,           // handle
    1               // core
  );
}

void loop() {
  // no hace falta nada aquí
  delay(100);
}

