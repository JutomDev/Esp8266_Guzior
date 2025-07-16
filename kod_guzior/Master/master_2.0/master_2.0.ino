#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <espnow.h>

// Dane sieci Wi-Fi Access Point
const char *ssid = "ESP_Master";
const char *password = "";
ESP8266WebServer server(80);

#define CHANNEL_COM 1  //kanal komunukacji
#define LED 2          // wbudowana dioda LED
#define DISCOVER 0
#define RESPONSE_ECHO 1
#define RESPONSE_PRESS 2
#define COMMAND_LED_ON 10
#define COMMAND_LED_OFF 11
#define COMMAND_ASSIGN_ID 12
#define MAX_DEVICE 20
#define Success false
#define Error true

bool startPressed = false;                                                   // false = OFF, true = ON
static uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };  //broadcast address
uint8_t nextAvailableId = 1;

typedef struct {
  uint8_t mac[6];
  uint8_t ledStatus;
  uint8_t id;
} structPeer;

structPeer peers[MAX_DEVICE];

typedef struct {
  uint8_t command;
  uint8_t id;
} structMessage;

void setup() {
  Serial.begin(115200);  //serial monitor
  pinMode(LED, OUTPUT);  //init led on esp;
  WiFi.mode(WIFI_STA);   //set wi-fi station
  WiFi.channel(CHANNEL_COM);
  //WEB
  WiFi.softAP(ssid, password);  //create access point

  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/scan", HTTP_GET, handlePeerCount);

  server.begin();

  //ESP-NOW
  if (esp_now_init() != 0)  //init esp-now
  {
    Serial.println("Blad inicjalizacji ESP-NOW");
    return;
  }
  //set roles station
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  //set function when send/received
  esp_now_register_send_cb(when_send);
  esp_now_register_recv_cb(when_recv);
  //add broadcast to base peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, CHANNEL_COM, NULL, 0);

  Serial.println("Master gotowy.");
}

void loop() {
  server.handleClient();  // obsługuje klientów automatycznie
}

void when_send(uint8_t *mac, uint8_t status) {  //status and to whom message
  Serial.print("Wiadomość wysłana do Esp o ID: ");
  uint8_t id=searchID(mac);
  if(id!=0){
    Serial.println(id);
  }
  else{
    Serial.println("nie znaleziono ID");
  }
  Serial.println(status == 0 ? " - SUKCES" : " - NIE POWIODŁO SIĘ");
}

void when_recv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (esp_now_is_peer_exist(mac) == 0 && data[0] == RESPONSE_ECHO) {  // jesli mac nie istnieje w bazie do go dodaj ... && data == RESPONSE_ECHO
    structMessage msg = {COMMAND_ASSIGN_ID,nextAvailableId};
    esp_now_add_peer(mac, ESP_NOW_ROLE_SLAVE, CHANNEL_COM, NULL, 0);
    addPeer(mac, nextAvailableId++); //add peer and push numeration
    SendToMac(mac,msg); //send id to esp
  } 
  
  else if (data[0] == RESPONSE_PRESS) {
    structMessage msg = {COMMAND_LED_OFF,0};
    SendToMac(mac,msg);
    Serial.printf("Wcisnieto przycisk w esp o mac:");
    printMac(mac);
  }
}

bool SendToEsp(structMessage message) {  //dokoncz kod bledu dla innych funkcji
  if(message.id==0){
    bool status = Success;
    for(int i=0;i<MAX_DEVICE;i++){
        if (peers[i].id != 0) {
          if(!SendToMac(peers[i].mac,message)){
            Serial.printf("Blad wysylu do id: %d\n",peers[i].id);
            status = Error;
          }
        }
        else{
          Serial.println("koniec wysylania do wszystkich");
          break;
        }
      }
      return status;
  }
  else{
    for (int i = 0; i < MAX_DEVICE; i++) {
      if (peers[i].id == message.id) {
        if (!SendToMac(peers[i].mac,message)) { //Send message and check result
          Serial.printf("Blad wysylu do id: %d\n",peers[i].id);
          return Error; //return error
        }
        else{
          Serial.println("koniec wysylania do konkretnego");
          return Success; 
        }
      }
    }
    Serial.println("ID esp nie znaleziono w bazie");
    return Error; //return error
  }
}

bool SendToMac(uint8_t *mac, structMessage message) {
  return esp_now_send(mac, (uint8_t *)&message, sizeof(message));
}

uint8_t searchID(uint8_t *mac){
  for(int i=0;i< MAX_DEVICE; i++){
    if (memcmp(peers[i].mac, mac, 6) == 0){
      Serial.printf("Zaleziono ID: %d po adresie MAC",peers[i].id);
      return peers[i].id;
    }
  }
  return Error;
}

bool addPeer(uint8_t *mac, uint8_t id) {
  for (int i = 0; i < MAX_DEVICE; i++) {
    if (peers[i].id == 0) {  // 0 = not used id
      memcpy(peers[i].mac, mac, 6);
      peers[i].id = id;
      peers[i].ledStatus = 0;
      return true;  //success
    }
  }
  return false;  //Failure, probably no space
}

bool SendToBroadcast(structMessage msg) {
  return esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
}

void clearPeers(){
  esp_now_del_peer(NULL); // Delete peers space
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, CHANNEL_COM, NULL, 0); //add peer broadcast 
  nextAvailableId = 1; // Reset available Id
  memset(peers, 0, sizeof(peers)); // Reset peers space
}

void printMac(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
}

// handler root web
void handleRoot() {
  server.send(200, "text/html", buildHTMLPage());
}
// handler START
void handleStart() {
  if (!startPressed) {
    structMessage msg = {COMMAND_LED_ON, 0}; // do wszystkich
    SendToEsp(msg);
    startPressed = true;
    //digitalWrite(LED_BUILTIN, LOW);  // LOW = włączone w ESP
    server.send(200, "text/plain", "Wystartowano");
    Serial.println("START pressed");
    // tutaj możesz np. rozsyłać info do ESPów!
  } else {
    server.send(200, "text/plain", "START zablokowany (najpierw STOP)");
  }
}
// handler STOP
void handleStop() {
  structMessage msg = {COMMAND_LED_OFF,0};
  SendToEsp(msg);
  startPressed = false;
  digitalWrite(LED_BUILTIN, HIGH);
  server.send(200, "text/plain", "Zatrzymano");
  Serial.println("STOP pressed");
}
// handler Scan
void handlePeerCount() {
  clearPeers();
  structMessage msg = {DISCOVER,0};
  for (int i = 0; i < 6; i++) {
    msg.command = DISCOVER;
    SendToBroadcast(msg);     //Send scanning message
    msg.command = 10 + (i % 2); // ON/OFF
    SendToBroadcast(msg);  //Blink while scanning
    delay(100);
  }

  u8 count_peer = 0;
  u8 encryp_peer = 0;
  if (esp_now_get_cnt_info(&count_peer, &encryp_peer) == 0) {
    server.send(200, "text/plain", String(count_peer - 1));
    Serial.printf("Liczba połączeń: %d\n", count_peer - 1);
  } else {
    //Error read counter peer
    Serial.printf("nope cnt info\n");
  }
}
// build page
String buildHTMLPage() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>ESP Master Panel</title>
    <style>
      body { font-family: Arial; background-color: #f0f0f0; text-align: center; padding-top: 30px; }
      button { font-size: 20px; padding: 10px 20px; margin: 10px; border: none; border-radius: 10px; cursor: pointer; }
      #start { background-color: #4CAF50; color: white; }
      #stop { background-color: #f44336; color: white; }
      #scan { background-color: #2196F3; color: white; }
      table { margin: 20px auto; border-collapse: collapse; }
      td, th { border: 1px solid #888; padding: 10px; }
      #status { margin-top: 20px; font-size: 18px; }
    </style>
  </head>
  <body>
    <h1>ESP Master Control Panel</h1>
    <button id="start" onclick="sendCommand('start')">START</button>
    <button id="stop" onclick="sendCommand('stop')">STOP</button>
    <button id="scan" onclick="scanPeers()">SKANUJ ESP</button>

    <div id="status">Status: Czekam...</div>

    <table>
      <tr><th>Podłączone ESP</th></tr>
      <tr><td id="peerCount">0</td></tr>
    </table>

    <script>
      var startDisabled = false;

      function sendCommand(cmd) {
        if (cmd === 'start' && startDisabled) return;
        fetch('/' + cmd)
          .then(response => response.text())
          .then(data => {
            document.getElementById('status').innerText = 'Status: ' + data;
            if (cmd === 'start') startDisabled = true;
            if (cmd === 'stop') startDisabled = false;
          });
      }

      function updatePeerCount() {
        fetch('/peer_count')
          .then(response => response.text())
          .then(count => {
            document.getElementById('peerCount').innerText = count;
          });
      }

      function scanPeers() {
        fetch('/scan') // Wywołujemy skanowanie
          .then(response => response.text())
          .then(data => {
            document.getElementById('status').innerText = 'Status: ' + data;
            updatePeerCount(); // Aktualizuj liczbę peerów
          });
      }

      setInterval(updatePeerCount, 2000); // odświeżaj co 2 sekundy
    </script>
  </body>
  </html>
  )rawliteral";

  return html;
}