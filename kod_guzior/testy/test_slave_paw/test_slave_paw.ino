#include <ESP8266WiFi.h>
#include <espnow.h>

#define LED_PIN 2  // Dioda na pinie GPIO2

typedef struct {
  uint8_t type;  // 0 = DISCOVER, 1 = HELLO, 2 = TOGGLE LED
} Message;

#define DISCOVER 0
#define HELLO    1
#define COMMAND  2

void OnDataSent(uint8_t *mac, uint8_t status) {
  // Opcjonalne potwierdzenie wysłania
}

void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  Message msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type == DISCOVER) {
    // Wyślij odpowiedź HELLO do mastera
    Message helloMsg;
    helloMsg.type = HELLO;
    
    // Dodaj mastera jako peer (jeśli nie istnieje)
    if (!esp_now_is_peer_exist(mac)) {
      esp_now_add_peer(mac, ESP_NOW_ROLE_CONTROLLER, 6, NULL, 0);
    }
    
    esp_now_send(mac, (uint8_t*)&helloMsg, sizeof(helloMsg));
    Serial.println("Wysłano HELLO");
    
  } else if (msg.type == COMMAND) {
    // Przełącz diodę LED
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    Serial.println("Przełączono LED");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  WiFi.mode(WIFI_STA);
  WiFi.channel(1);  // TEN SAM KANAŁ CO MASTER!
  
  if (esp_now_init() != 0) {
    Serial.println("Błąd inicjalizacji ESP-NOW");
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("Slave gotowy");
}

void loop() {



}
