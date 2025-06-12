/*
  Actuadores.ino
  Nodo 2 = Nodo Actuadores (relés / bombas / RGB / buzzer)
*/

#include <WiFi.h>
#include <esp_now.h>

// ——————————————————————————
// MAC del Controlador (reemplaza con la que imprimiste)
// ——————————————————————————
uint8_t controllerMac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };

// ——————————————————————————
// Pines de salida
// ——————————————————————————
#define PIN_FAN    22   // Ventilador
#define PIN_PUMP   23   // Bomba
#define PIN_RGB    27   // LED RGB
#define PIN_BUZZER 25   // Buzzer

// ——————————————————————————
// Estructura del comando recibido
// ——————————————————————————
typedef struct {
  bool fanOn;
  bool pumpOn;
  bool rgbOn;
  bool buzzerOn;
} ActuatorCmd_t;

// ——————————————————————————
// Callback: se llama al recibir un comando ESP-NOW
// ——————————————————————————
void onCmdRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(ActuatorCmd_t)) return;
  ActuatorCmd_t cmd;
  memcpy(&cmd, data, sizeof(cmd));

  digitalWrite(PIN_FAN,    cmd.fanOn    ? HIGH : LOW);
  digitalWrite(PIN_PUMP,   cmd.pumpOn   ? HIGH : LOW);
  digitalWrite(PIN_RGB,    cmd.rgbOn    ? HIGH : LOW);
  digitalWrite(PIN_BUZZER, cmd.buzzerOn ? HIGH : LOW);

  Serial.printf("Cmd → Fan:%s Pump:%s RGB:%s Buzzer:%s\n",
                cmd.fanOn    ? "ON" : "OFF",
                cmd.pumpOn   ? "ON" : "OFF",
                cmd.rgbOn    ? "ON" : "OFF",
                cmd.buzzerOn ? "ON" : "OFF");
}

void setup() {
  // Configura puerto serie para debug
  Serial.begin(115200);

  // Configura pines de salida
  pinMode(PIN_FAN,    OUTPUT);
  pinMode(PIN_PUMP,   OUTPUT);
  pinMode(PIN_RGB,    OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Inicia Wi-Fi en modo STA (solo para ESP-NOW)
  WiFi.mode(WIFI_STA);

  // Inicializa ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error al iniciar ESP-NOW");
    while (true) { delay(1000); }
  }

  // Registra callback para recepción
  esp_now_register_recv_cb(onCmdRecv);

  // Añade el controlador como peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, controllerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("⚠️ Error al agregar peer Controlador");
  }
}

void loop() {
  // Todo sucede en onCmdRecv; aquí nada más un pequeño delay
  delay(100);
}
