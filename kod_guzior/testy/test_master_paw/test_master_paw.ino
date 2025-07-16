#include <ESP8266WiFi.h>
#include <espnow.h>

// Adres broadcastowy
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Struktura wiadomości
typedef struct {
  uint8_t type;  // 0 = DISCOVER, 1 = HELLO, 2 = TOGGLE LED
} Message;

#define DISCOVER 0
#define HELLO    1
#define COMMAND  2

// Lista znanych urządzeń
uint8_t knownDevices[20][6];  // Maksymalnie 20 urządzeń
int deviceCount = 0;

void OnDataSent(uint8_t *mac, uint8_t status) {
  Serial.print("Wysłano do: ");
  printMac(mac);
  Serial.println(status == 0 ? " OK" : " BŁĄD");
}

void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  Message msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type == HELLO) {
    // Sprawdź czy urządzenie jest już na liście
    bool exists = false;
    for (int i=0; i<deviceCount; i++) {
      if (memcmp(knownDevices[i], mac, 6) == 0) exists = true;
    }
    
    if (!exists && deviceCount < 20) {
      memcpy(knownDevices[deviceCount], mac, 6);
      deviceCount++;
      Serial.print("Nowe urządzenie: ");
      printMac(mac);
      
      // Dodaj urządzenie jako peer
      esp_now_add_peer(mac, ESP_NOW_ROLE_SLAVE, 6, NULL, 0);  
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.channel(6);  // Ustaw ten sam kanał co slave!

  if (esp_now_init() != 0) {
    Serial.println("Błąd inicjalizacji ESP-NOW");
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Dodaj adres broadcast jako peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 6, NULL, 0);
  
  Serial.println("Master gotowy. Szukam urządzeń...");
}

void loop() {
  static unsigned long lastDiscover = 0;
  if (millis() - lastDiscover > 3000) {
    lastDiscover = millis();
    
    // Wyślij broadcast DISCOVER
    Message discoverMsg;
    discoverMsg.type = DISCOVER;
    esp_now_send(broadcastAddress, (uint8_t*)&discoverMsg, sizeof(discoverMsg));
    Serial.println("Wysłano DISCOVER");
  }

  // Co 5 sekund wyślij komendę TOGGLE LED do wszystkich urządzeń
  static unsigned long lastCommand = 0;
  if (deviceCount > 0 && millis() - lastCommand > 5000) {
    lastCommand = millis();
    
    Message commandMsg;
    commandMsg.type = COMMAND;
    
    for (int i=0; i<deviceCount; i++) {
      esp_now_send(knownDevices[i], (uint8_t*)&commandMsg, sizeof(commandMsg));
    }
    Serial.println("Wysłano TOGGLE LED");
  }
}

void printMac(const uint8_t *mac) {
  for (int i=0; i<6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
}
