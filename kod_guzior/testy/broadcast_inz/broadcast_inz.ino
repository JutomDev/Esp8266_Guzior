#include "config.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// ====== KONFIGURACJA ======
#define LED_PIN 2 // Ustaw pin do diody (np. D1)

// ====== STRUKTURY ======
struct msg_card_id {
  uint8_t card_id[4];
};

struct msg_measurement {
  double value;
};

msg_measurement message_content;
volatile bool value_changed = false;

// ====== CALLBACK: Po wysłaniu ======
void On_data_sent(uint8_t* mac_addr, uint8_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Packet to: ");
  Serial.print(macStr);
  Serial.print(" send status: ");
  Serial.println(status == 0 ? "Delivery Success" : "Delivery Fail");
}

// ====== CALLBACK: Po odebraniu ======
void received_msg_callback(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
  memcpy(&message_content, incomingData, sizeof(message_content));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Measurement value: ");
  Serial.println(message_content.value);
  value_changed = true;
}

// ====== FUNKCJE WSPOMAGAJĄCE (opcjonalne) ======
void send_msg(const uint8_t* address, uint8_t* payload, int length) {
  auto result = esp_now_send((uint8_t*)address, payload, length);
  Serial.println(result == 0 ? "Delivery Success" : "Delivery Fail");
}

void connect(const uint8_t* address) {
  esp_now_add_peer((uint8_t*)address, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Wyłącz diodę na start

  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(1); // Jeśli master działa na kanale 1

  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(On_data_sent);
  esp_now_register_recv_cb(received_msg_callback);

  Serial.println("ESP-NOW ready");
}

// ====== LOOP ======
void loop() {
  if (value_changed) {
    value_changed = false;

    Serial.println("Odebrano nową wartość!");

    if (message_content.value == 1.0) {
      digitalWrite(LED_PIN, HIGH);  // Włącz diodę
      Serial.println("LED ON");
    } else {
      digitalWrite(LED_PIN, LOW);   // Wyłącz diodę
      Serial.println("LED OFF");
    }
  }
}
