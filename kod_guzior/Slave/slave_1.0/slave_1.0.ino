#include <ESP8266WiFi.h>
#include <espnow.h>

#define LED_PIN 2

struct Message {
  uint8_t type; // 0 - potwierdzenie, 1 - komenda LED
};

void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  Message msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type == 1) {
    digitalWrite(LED_PIN, LOW); // Odbiór komendy LED - zapal LED (aktywny stan niski)
    Serial.println("Otrzymano polecenie - LED zapalony");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Domyślnie LED wyłączony (aktywny stan niski)

  WiFi.mode(WIFI_STA);
  wifi_set_channel(6); // Ustaw kanał

  if (esp_now_init() != 0) {
    Serial.println("Błąd inicjalizacji ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);
}

void loop() {
  // Nic tutaj nie musimy robić, Slave czeka na komunikaty
}

