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
#define COMMAND_START_GAME 20
#define COMMAND_STOP_GAME 21

uint8_t ControllerAddress[6];
uint8_t my_ID=0;
bool led=false;
volatile bool wifi_sleeping = false;
bool gameActive = false;
const unsigned long wakeInterval = 500; // time sleep ms

typedef struct {
  uint8_t command;
  uint8_t id;
} structMessage;

void setup() {
  Serial.begin(115200);
  //init output/input
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, led); //off led
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

  // wifi_set_sleep_type(LIGHT_SLEEP_T);
  // wifi_fpm_open();
  // wifi_fpm_do_sleep(wakeInterval * 1000); // sleep for 0.5 sec
}

void loop() {

   if (gameActive) {
    unsigned long startTime = millis();
    while (millis() - startTime < 30000) { // timeout after 30 sec
      if(!gameActive){
        break;
      }
      
      if (led && digitalRead(PIN_BUTTON) == LOW) {
        structMessage msg = {RESPONSE_PRESS, my_ID};
        Send_to_Controller(msg);
      }
      delay(50);
    }
  }

  // Sleep for 0.5 sec
  Serial.println("ide spac");
  start_light_sleep(wakeInterval);
  //Serial.println("obudzony");
  delay(100);
}

void start_light_sleep(uint32_t time_ms) {

  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open(); 

  Serial.end();

  wifi_sleeping = true;
  
  wifi_fpm_set_wakeup_cb(light_sleep_wakeup);
  wifi_fpm_do_sleep(time_ms*1000);  // in us, finally we are going to light sleep mode.
  
  esp_delay(time_ms+1, [](){ return wifi_sleeping; }); 
}

void light_sleep_wakeup(void) { 

 // I've read that some functions taking time should be called here. Not sure why.
 // Maybe removing it will break the wake up function.
 // I think the underlying function may call some esp yield or else.
  Serial.begin(115200);

  wifi_fpm_close();  
  wifi_set_opmode(STATION_MODE); 
  wifi_station_connect();   

  wifi_sleeping = false;
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
        gameActive=true;
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
      case COMMAND_START_GAME:
        gameActive=true;
        break;
      case COMMAND_STOP_GAME:
        gameActive=false;
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