#include <ESP8266WiFi.h>
#include <espnow.h>


#define channel_com 1   //kanal komunukacji
#define PIN_LED 4  // Pin PIN_LED
#define PIN_BUTTON 5    // Pin Button

#define DISCOVER 0
#define RESPONSE_ECHO 1
#define RESPONSE_PRESS 2
#define COMMAND_LED_ON 10
#define COMMAND_LED_OFF 11
#define COMMAND_ASSIGN_ID 12

uint8_t ControllerAddress[6];
uint8_t my_ID=0;
bool led=false;
unsigned long lastPressTime  = millis();

typedef struct {
  uint8_t command;
  uint8_t id;
} structMessage;

void setup() {
  Serial.begin(115200);
  //init output/input
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, led); //wylączony
  pinMode(PIN_BUTTON, INPUT_PULLUP);


  //ESP-NOW SLAVE
  WiFi.mode(WIFI_STA);
  WiFi.channel(channel_com);

  if (esp_now_init() != 0)  //init esp-now
  {
    Serial.println("\nBlad inicjalizacji ESP-NOW");
    return;
  }
  //set roles station
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  //set function when send/received
  esp_now_register_send_cb(when_send);
  esp_now_register_recv_cb(when_recv);

  Serial.println("\nSlave gotowy");
}

void loop() {

  if(millis()-lastPressTime>100){

    bool ButtonStatat = digitalRead(PIN_BUTTON);
    
    if(led && !ButtonStatat){
      structMessage msg = {RESPONSE_PRESS, my_ID};
      Send_to_Controller(msg);
      //change_led(!led);
    }
    lastPressTime = millis();
  }

}

void change_led(bool status){
  led=status;
  digitalWrite(PIN_LED,led);
}

void when_send(uint8_t *mac, uint8_t status) {
  Serial.print("Status Wiadomość: ");
  Serial.println(status == 0 ? " - SUKCES" : " - NIE POWIODŁO SIĘ");
}

void when_recv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if(len==2){
    structMessage msg;
    switch (data[0]) {
      case DISCOVER:
        esp_now_add_peer(mac, ESP_NOW_ROLE_CONTROLLER, channel_com, NULL, 0);
        memcpy(ControllerAddress, mac, sizeof(ControllerAddress));
        msg = {RESPONSE_ECHO, my_ID};
        Send_to_Controller(msg);
        Serial.println("Otrzymano DISCOVER");
        break;
      case COMMAND_LED_ON:
        change_led(HIGH);
        // Obsługuje COMMAND_LED_ON
        Serial.println("Włączono PIN_LED");
        break;
      case COMMAND_LED_OFF:
        change_led(LOW);
        // Obsługuje COMMAND_LED_OFF
        Serial.println("Wyłączono PIN_LED");
        break;
      case COMMAND_ASSIGN_ID:
        my_ID = data[1];
        break;
      default:
        Serial.println("Otrzymano nieznana komende");
        break;
    }
  }else{
    Serial.println("Blad dlugosci otrzymanej wiadomosci");
  }
}

void Send_to_Controller(structMessage message) {
  esp_now_send(ControllerAddress, (uint8_t *)&message, sizeof(message));
  Serial.printf("Wysłano do master wiadomosc: %d\n", message.command); //0 - discover..
}

// void printMac(const uint8_t *mac) {
//   for (int i = 0; i < 6; i++) {
//     Serial.printf("%02X", mac[i]);
//     if (i < 5) Serial.print(":");
//   }
// }