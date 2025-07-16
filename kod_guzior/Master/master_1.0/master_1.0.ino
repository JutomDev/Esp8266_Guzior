#include <ESP8266WiFi.h>
#include <espnow.h>

#define LED_BUILTIN 2  // wbudowana dioda

struct DeviceInfo {
  uint8_t mac[6];
};

DeviceInfo devices[20];  // maksymalnie 20 urządzeń
int device_count = 0;

void onSent(uint8_t* mac_addr, uint8_t sendStatus) {
  Serial.print("Wiadomość wysłana do: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println(sendStatus == 0 ? " - SUKCES" : " - NIE POWIODŁO SIĘ");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  wifi_set_channel(6); // ten sam w masterze i slave


  if (esp_now_init() != 0) {
    Serial.println("Błąd inicjalizacji ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onSent);

  Serial.println("Master gotowy.");
}

void loop() {
  // Wysyłamy broadcast wiadomość co 3 sekundy
  static unsigned long lastScan = 0;
  if (millis() - lastScan > 3000) {
    lastScan = millis();

    scanForDevices();
    sendLEDCommand();
  }
}

void scanForDevices() {
  Serial.println("\nSkanowanie urządzeń ESP-NOW w pobliżu...");

  device_count = 0; // resetujemy licznik

  int n = WiFi.scanNetworks(false, true);  // tylko urządzenia ukryte (broadcast)
  if (n == 0) {
    Serial.println("Brak urządzeń.");
    return;
  }

  for (int i = 0; i < n; ++i) {
    String SSID = WiFi.SSID(i);
    if (SSID == "") {  // ignorujemy normalne Wi-Fi
      uint8_t* bssid = WiFi.BSSID(i);

      // dodajemy nowy MAC
      memcpy(devices[device_count].mac, bssid, 6);
      Serial.print("Znaleziono ESP: ");
      printMac(bssid);
      device_count++;

      if (device_count >= 20) break;
    }
  }

  Serial.print("Łącznie znaleziono urządzeń: ");
  Serial.println(device_count);
}

void sendLEDCommand() {
  if (device_count == 0) {
    Serial.println("Brak urządzeń do wysłania wiadomości.");
    return;
  }

  uint8_t ledCommand = 1;  // przykładowy sygnał - 1 oznacza zapal diodę

  for (int i = 0; i < device_count; i++) {
    esp_now_add_peer(devices[i].mac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
    esp_now_send(devices[i].mac, &ledCommand, sizeof(ledCommand));
  }
}

void printMac(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

