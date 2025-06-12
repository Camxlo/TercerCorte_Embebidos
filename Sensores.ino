/*
  Sensores.ino
  Nodo 1 = Nodo Sensores (DHT11 temp, LDR, sensor suelo)
*/

#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

// Pines sensores
#define DHTPIN   2     // DATA del DHT11
#define DHTTYPE  DHT11
#define LDR_PIN  35    // LDR en ADC35
#define SOIL_PIN 34    // Humedad suelo en ADC34

DHT dht(DHTPIN, DHTTYPE);

// MAC del Controlador (reemplazar con la tuya)
uint8_t controllerMac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };


typedef struct {
  float temp;
  float light;
  float soil;
} SensorData_t;

void setup() {
  Serial.begin(115200);
  dht.begin();
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error al iniciar ESP-NOW");
    while (1) delay(1000);
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, controllerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void loop() {
  SensorData_t msg;
  msg.temp  = dht.readTemperature();
  msg.light = analogRead(LDR_PIN);
  msg.soil  = analogRead(SOIL_PIN);

  esp_err_t res = esp_now_send(controllerMac, (uint8_t*)&msg, sizeof(msg));
  if (res == ESP_OK) {
    Serial.printf("Enviado → T=%.1f  L=%.0f  S=%.0f\n",
                  msg.temp, msg.light, msg.soil);
  } else {
    Serial.printf("Error enviando: %d\n", res);
  }

  delay(3000);
}
