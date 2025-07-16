#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <espnow.h>

#define CHANNEL_COM 1      // Channel communication ESP-NOW
#define PIN_LED_BUILTIN 2  // Pin built-in diode
#define PIN_LED 4          // Pin PIN_LED
#define PIN_BUTTON 5       // Pin Button

#define OFF 0
#define ON 1
#define DISCOVER 0
#define RESPONSE_ECHO 1
#define RESPONSE_PRESS 2
#define COMMAND_LED_ON 10
#define COMMAND_LED_OFF 11
#define COMMAND_ASSIGN_ID 12
#define COMMAND_START_GAME 20
#define COMMAND_STOP_GAME 21

#define MAX_DEVICE 10
#define STATUS_OK false
#define STATUS_FAIL true

// Server port
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// Global flag and date
unsigned long lastPressTime = millis();
bool startPressed = false;
bool led = false;
uint8_t nextAvailableId = 1;
uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t StatusMess = 0;


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
  Serial.begin(115200);              //serial monitor
  pinMode(PIN_LED_BUILTIN, OUTPUT);  //init led on esp;
  pinMode(PIN_LED, OUTPUT);          //init led on esp;
  digitalWrite(PIN_LED, led);        //wylączony
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);  //set wi-fi station
  WiFi.channel(CHANNEL_COM);
  //WEB
  WiFi.softAP("ESP_Master");  //create access point

  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/scan", HTTP_GET, handlePeerCount);
  server.on("/ping", HTTP_GET, pingSelectedEsp);

  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWsEvent);

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
  webSocket.loop();

  if (millis() - lastPressTime > 100) {

    bool ButtonStatat = digitalRead(PIN_BUTTON);

    if (led && !ButtonStatat) {
      led = LOW;
      digitalWrite(PIN_LED, led);
      MessageWebStatus(0, led);
    }
    lastPressTime = millis();
  }
  // if (millis() - lastPressTime > 10000) {  //test

  //   for (uint8_t i = 0; i < MAX_DEVICE; i++) {
  //     Serial.printf("%d id: %d \n", i, peers[i].id);
  //   }
  //   Serial.println("koniec id");
  //   lastPressTime = millis();
  // }
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  // aktualnie tylko wysyłamy dane z ESP do strony
  if (type == WStype_CONNECTED) {
    Serial.println("WebSocket klient podłączony");
  }
}

void when_send(uint8_t *mac, uint8_t status) {  //status and to whom message
  status == 0 ? StatusMess=1 : StatusMess=2;
  Serial.println(status == 0 ? " - WYSLANO " : " - NIE POWIODLO SIE WYSLANIE");
}

void when_recv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len == 2) {
    if (esp_now_is_peer_exist(mac) == 0 && data[0] == RESPONSE_ECHO) {  // jesli mac nie istnieje w bazie do go dodaj ... && data == RESPONSE_ECHO
      structMessage msg = { COMMAND_ASSIGN_ID, nextAvailableId };
      esp_now_add_peer(mac, ESP_NOW_ROLE_SLAVE, CHANNEL_COM, NULL, 0);
      addPeer(mac, nextAvailableId++);  //add peer and push numeration
      SendToMac(mac, msg);              //send id to esp
    }

    else if (data[0] == RESPONSE_PRESS) {
      structMessage msg = { COMMAND_LED_OFF, 0 };
      SendToMac(mac, msg);
      peers[data[1]].ledStatus = 0;
      Serial.printf("Wcisnieto przycisk w esp o mac:");
      printMac(mac);
      MessageWebStatus(data[1], 0);
    }
  }
}

bool SendToEsp(structMessage message) {  //dokoncz kod bledu dla innych funkcji
  if (message.id == 0) {
    bool status = STATUS_OK;
    for (uint8_t i = 0; i < MAX_DEVICE; i++) {
      if (peers[i].id != 0) {
        if (SendToMac(peers[i].mac, message)) { //Check parametrs
          // while(StatusMess==0){

          // }
          // if(StatusMess==1){ // check result
          //   //message to website
          //   if (message.command == COMMAND_LED_OFF) {
          //     MessageWebStatus(peers[i].id, 0);  //message for website about led status this of a particular slav
          //   } else if (message.command == COMMAND_LED_ON) {
          //     MessageWebStatus(peers[i].id, 1);
            }
          } else {
            //Serial.printf("Blad wyslania do id: %d\n", peers[i].id);
            status = STATUS_FAIL;
          }
        } else {
          status = STATUS_FAIL;
        } 
        //StatusMess=0; 
      } else {
        break;
      }
    }
    return status;
  } else {
    uint8_t status_sendtomac;
    for (uint8_t i = 0; i < MAX_DEVICE; i++) {
      if (peers[i].id == message.id) {
        //message to slave
         if (SendToMac(peers[i].mac, message)) { //Check parametrs
          // while(StatusMess==0){

          // }
          // if(StatusMess){ // check result
          //message to website
            // if (message.command == COMMAND_LED_OFF) {
            //   MessageWebStatus(peers[i].id, 0);  //message for website about led status this of a particular slave
            // } else if (message.command == COMMAND_LED_ON) {
            //   MessageWebStatus(peers[i].id, 1);
            // }
            //Status_mess=false;
            return STATUS_OK;
          } else {
            Serial.printf("Blad wyslania do id: %d\n", peers[i].id);
            return STATUS_FAIL;  //return status_error
          }
          StatusMess=0; 
        }
      }
    }
    Serial.println("ID esp nie znaleziono w bazie");
    return STATUS_FAIL;  //return status_error
  }
}

void MessageWebStatus(uint8_t id, uint8_t status) {
  String msg = "{\"type\":\"ledStatusUpdate\",\"id\":" + String(id) + ",\"ledStatus\":" + String(status) + "}";

  webSocket.broadcastTXT(msg);
  Serial.println("Wyslano do websocket");
}

bool SendToMac(uint8_t *mac, structMessage message) {
  return esp_now_send(mac, (uint8_t *)&message, sizeof(message)) == ERR_OK;
}

uint8_t searchID(uint8_t *mac) {
  for (uint8_t i = 0; i < MAX_DEVICE; i++) {
    if (memcmp(peers[i].mac, mac, 6) == 0) {
      //Serial.printf("Zaleziono ID: %d po adresie MAC", peers[i].id);
      return peers[i].id;
    }
  }
  return 0;  //Error
}

bool SendToBroadcast(structMessage message) {
  return esp_now_send(broadcastAddress, (uint8_t *)&message, sizeof(message));
}

bool addPeer(uint8_t *mac, uint8_t id) {
  for (uint8_t i = 0; i < MAX_DEVICE; i++) {
    if (peers[i].id == 0) {  // 0 = not used id
      memcpy(peers[i].mac, mac, 6);
      peers[i].id = id;
      peers[i].ledStatus = 0;
      return true;  //STATUS_OK
    }
  }
  return false;  //Failure, probably no space
}

void clearPeers() {
  for (uint8_t i = 0; i < MAX_DEVICE; i++) {  // clean peers from esp memory
    if (peers[i].id != 0) {
      esp_now_del_peer(peers[i].mac);
    } else {
      break;
    }
  }
  nextAvailableId = 1;              // Reset available Id
  memset(peers, 0, sizeof(peers));  // Reset peers space
}

void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
}
void search_esp(void) {
  structMessage msg = { DISCOVER, 0 };

  for (uint8_t i = 0; i < 8; i++) {
    msg.command = DISCOVER;
    SendToBroadcast(msg);        //Send scanning message
    msg.command = 10 + (i % 2);  // ON/OFF
    SendToBroadcast(msg);        //Blink while scanning
    delay(150);
  }
  msg.command = COMMAND_STOP_GAME;
  SendToBroadcast(msg);
}

bool SendPingEsp(uint8_t id) {
  bool status;
  structMessage msg = { DISCOVER, id };
  for (uint8_t i = 0; i < 8; i++) {
    msg.command = DISCOVER;
    SendToEsp(msg);
    msg.command = 10 + (i % 2);  // ON/OFF
    if (!SendToEsp(msg)) {       //Blink while scanning
      status = STATUS_FAIL;
    } else {
      status = STATUS_OK;
    }
    delay(150);
  }
  msg.command = COMMAND_STOP_GAME;
  if (SendToEsp(msg)) {
    return status;
  } else {
    return STATUS_FAIL;
  }
}

String jsonCount() {
  String json = "[";

  for (uint8_t i = 0; i < MAX_DEVICE; i++) {
    if (peers[i].id != 0) {
      if (json.length() > 1) json += ",";
      json += "{\"id\":" + String(peers[i].id) + ",\"ledStatus\":" + String(peers[i].ledStatus) + "}";
    }
  }
  json += "]";
  return json;
}

void changeLedStatus(uint8_t setStatus) {
  for (uint8_t i = 0; i < MAX_DEVICE; i++) {
    if (peers[i].id != 0) {
      peers[i].ledStatus = setStatus;
    } else {
      break;
    }
  }
}

// handler root web
void handleRoot() {
  server.send(200, "text/html", buildHTMLPage());
}

// handler START
void handleStart() {
  if (!startPressed) {
    structMessage msg = { COMMAND_LED_ON, 0 };  // 0 means all saved esps
    SendToEsp(msg);
    changeLedStatus(ON);
    startPressed = true;
    led = HIGH;
    digitalWrite(PIN_LED, led);  // LOW = włączone w ESP
    MessageWebStatus(0, led);
    server.send(200, "text/plain", "Wystartowano");
    Serial.println("START pressed");
  } else {
    server.send(200, "text/plain", "START zablokowany");
  }
}
// handler STOP
void handleStop() {
  structMessage msg = { COMMAND_LED_OFF, 0 };
  SendToEsp(msg);
  changeLedStatus(OFF);
  startPressed = false;
  led = LOW;
  digitalWrite(PIN_LED, led);
  MessageWebStatus(0, led);
  server.send(200, "text/plain", "Zatrzymano");
  Serial.println("STOP pressed");
}

// handler Scan
void handlePeerCount() {
  clearPeers();
  search_esp();
  startPressed = false;
  String json = jsonCount();
  server.send(200, "application/json", json);
}

// Ping esp
void pingSelectedEsp() {
  uint8_t espID = server.arg("id").toInt();
  if (SendPingEsp(espID) == STATUS_OK) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Err-ping");
  }
}

// build page
String buildHTMLPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>UMK - AiR</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #f2f2f2;
      margin: 0;
      padding: 20px;
    }
    h1 {
      text-align: center;
    }
    .container {
      max-width: 600px;
      margin: auto;
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    select, button, input[type="number"] {
      width: 100%;
      padding: 10px;
      margin: 10px 0;
      font-size: 16px;
    }
    #gameInfo {
      font-size: 20px;
      text-align: center;
      margin-bottom: 20px;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 20px;
    }
    th, td {
      padding: 10px;
      border: 1px solid #ccc;
      text-align: center;
    }
    input[type="text"] {
      padding: 5px;
      width: 70%;
    }
    .ping-button {
      padding: 5px 10px;
      font-size: 12px;
      margin-left: 10px;
      margin-right: 500px;
    }
    .device-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 5px 0;
      gap: 10px;
    }
    .reset-button {
      font-size: 12px;
      padding: 5px 10px;
      background-color: #e74c3c;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }

    .reset-button:hover {
      background-color: #c0392b;
    }

    .header-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>UMK- AiR</h1>

    <label for="modeSelect">Wybierz tryb:</label>
    <select id="modeSelect">
      <option value="sprawność">Sprawność</option>
      <option value="zręczność">Zręczność</option>
    </select>

    <div id="timeInputContainer" style="display:none;">
      <label for="customTime">Czas gry (s):</label>
      <input type="number" id="customTime" min="1" value="30" />
    </div>

    <button onclick="startGame()">Rozpocznij grę</button>
    <button onclick="stopGame()">Zatrzymaj grę</button>
    <button id="hitButton" style="display:inline-block;" onclick="simulateHit()">Trafienie (test)</button>
    <button id="increment" style="display:none;"onclick="incrementScore()">Zwiększ wynik (test)</button>

    <div id="gameInfo">
      Czas: <span id="timer">0</span> s | Wynik: <span id="score">0</span>
    </div>

    <div class="header-row">
      <h3>Top 10 wyników</h3>
      <button onclick="resetScores()" class="reset-button">Resetuj wyniki</button>
    </div>
    <table id="scoreTable">
      <thead>
        <tr>
          <th>Miejsce</th>
          <th>Nick</th>
          <th>Wynik</th>
        </tr>
      </thead>
      <tbody>
        <!-- Wyniki będą wstawiane tutaj -->
      </tbody>
    </table>

    <button onclick="scanESP()">Skanuj ESP</button>

    <h3 id="deviceHeader">Nie wykonaniu skanu esp</h3>
    <div id="deviceList">
      <!-- Lista urządzeń będzie wstawiana tutaj -->
    </div>
  </div>

  <script>
    let timerInterval = null;
    let startTime = 0;
    let timeLeft = 0;
    let currentMode = "sprawność";
    let currentScore = 0;

    const savedDevices = localStorage.getItem("espDevices");
    if (savedDevices) {
      updateDeviceList(JSON.parse(savedDevices));
    }

    let scoreList = [];

    let socket = new WebSocket("ws://" + window.location.hostname + ":81/");

    socket.onopen = function () {
      console.log("WebSocket połączony");
    };

    socket.onerror = function (error) {
      console.error("WebSocket błąd:", error);
    };

    socket.onclose = function () {
      console.warn("WebSocket zamknięty");
    };

    socket.onmessage = function (event) {
      console.log("Odebrano WebSocket:", event.data);
      const data = JSON.parse(event.data);

      if (data.type === "ledStatusUpdate") {
        const deviceId = data.id;
        const status = data.ledStatus;

        const statusDot = document.querySelector(`#status-${deviceId}`);
        if (statusDot) {
          statusDot.textContent = status ? "🟢" : "🔴";
        }
      }
    };

    function initializeScoreList(mode) {
      const saved = localStorage.getItem(`scoreList_${mode}`);
      if (saved) {
        scoreList = JSON.parse(saved);
      } else {
        scoreList = [];
        if (mode === "sprawność") {
          for (let i = 1; i <= 10; i++) scoreList.push({ nick: "-----", score: 999 });
        } else {
          for (let i = 1; i <= 10; i++) scoreList.push({ nick: "-----", score: 0 });
        }
      }
      updateScoreTable();
    }

    const modeSelect = document.getElementById("modeSelect");
    const timeInputContainer = document.getElementById("timeInputContainer");
    const hitButton = document.getElementById("hitButton");
    const customTimeInput = document.getElementById("customTime");
    const timerDisplay = document.getElementById("timer");
    const scoreDisplay = document.getElementById("score");
    const scoreTableBody = document.querySelector("#scoreTable tbody");

    modeSelect.addEventListener("change", () => {
      currentMode = modeSelect.value;
      initializeScoreList(currentMode);
      if (currentMode === "zręczność") {
        timeInputContainer.style.display = "block";
        hitButton.style.display = "none";
        increment.style.display = "inline-block";
      } if(currentMode === "sprawność"){
        timeInputContainer.style.display = "none";
        hitButton.style.display = "inline-block";
        increment.style.display = "none";
      }
    });
    function resetScores() {
      if (confirm("Czy na pewno chcesz zresetować wyniki?")) {
        localStorage.removeItem("scoreList_sprawność");
        localStorage.removeItem("scoreList_zręczność");
        initializeScoreList(currentMode);
      }
    }

   function updateDeviceList(devices) {
      const deviceList = document.getElementById("deviceList");
      const deviceHeader = document.getElementById("deviceHeader");
      deviceList.innerHTML = `<span style="white-space: nowrap;">ID: 0 - Master, Stan: <span id="status-0">🔴</span></span>`; // wyczyść

      // Komunikat o liczbie urządzeń
      deviceHeader.textContent = `Połączone Przyciski: ${devices.length+1}`;

      // Wyświetl każde urządzenie
      devices.forEach(dev => {
        const entry = document.createElement("div");
        entry.className = "device-row";
        
        // const statusDot = document.createElement("span");
        // statusDot.id = `status-${dev.id}`;
        // statusDot.textContent = dev.ledStatus ? "🟢" : "🔴";

        // const info = document.createElement("span");
        // info.textContent = `ID: ${dev.id}, Stan: `;
        // info.appendChild(statusDot);

        const statusDot = dev.ledStatus ? "🟢" : "🔴"; 
        entry.innerHTML = `<span style="white-space: nowrap;">ID: ${dev.id}, Stan: <span id="status-${dev.id}">${statusDot}</span></span>`;

        const pingButton = document.createElement("button");
        pingButton.textContent = "Ping";
        pingButton.className = "ping-button";
        pingButton.onclick = () => pingDevice(dev.id); // Przekazujemy ID ESP

        entry.appendChild(pingButton);
        deviceList.appendChild(entry);
      }); 
    }

    function startGame() {
      fetch("/start")
      clearInterval(timerInterval);
      currentScore = 0;
      scoreDisplay.textContent = currentScore;

      if (currentMode === "zręczność") {
        timeLeft = parseInt(customTimeInput.value);
        timerDisplay.textContent = timeLeft;
        scoreDisplay.textContent = currentScore;

        timerInterval = setInterval(() => {
          timeLeft--;
          timerDisplay.textContent = timeLeft;
          if (timeLeft <= 0) {
            clearInterval(timerInterval);
            handleEndGame();
          }
        }, 1000);
      } else {
        startTime = Date.now();
        timerDisplay.textContent = "0";
        scoreDisplay.textContent = "0";

        timerInterval = setInterval(() => {
          const elapsed = Math.floor((Date.now() - startTime) / 1000);
          timerDisplay.textContent = elapsed;
          scoreDisplay.textContent = elapsed;
        }, 1000);
      }
    }

    function pingDevice(deviceId) {
      fetch(`/ping?id=${deviceId}`)
        .then(res => {
          if (res.ok) {
            console.log(`Ping do ESP ${deviceId} wysłany.`);
          } else {
            console.warn(`Błąd podczas pingowania ESP ${deviceId}`);
          }
        })
        .catch(err => console.error("Błąd sieci:", err));
    }

    function stopGame() {
      fetch("/stop")
      clearInterval(timerInterval);
      handleEndGame();
    }

    function incrementScore() {
      if (currentMode === "zręczność" && timeLeft > 0) {
        currentScore++;
        scoreDisplay.textContent = currentScore;
      }
    }

    function scanESP() {
      fetch("/scan")
        .then(res => res.json())
        .then(data => {
          updateDeviceList(data);
          localStorage.setItem("espDevices", JSON.stringify(data));
        });
    }    

    function simulateHit() {
      if (currentMode === "sprawność") {
        clearInterval(timerInterval);
        const finalScore = Math.floor((Date.now() - startTime) / 1000);
        checkAndSaveScore(finalScore);
      }
    }

    function handleEndGame() {
      const finalScore = currentMode === "zręczność"
        ? currentScore
        : Math.floor((Date.now() - startTime) / 1000);

      checkAndSaveScore(finalScore);
    }

    function checkAndSaveScore(finalScore) {
      let isInTop = false;

      if (currentMode === "sprawność") {
        // Im mniej czasu, tym lepiej
        isInTop = finalScore < scoreList[scoreList.length - 1].score;
      } else {
        // Im więcej punktów, tym lepszy wynik
        isInTop = finalScore > scoreList[scoreList.length - 1].score;
      }

      if (isInTop) {
        const nick = prompt(`Gratulacje! Uzyskałeś wynik ${finalScore}. Podaj swój nick:`);
        if (nick && nick.trim() !== "") {
          scoreList.push({ nick, score: finalScore });
          scoreList.sort((a, b) =>
            currentMode === "sprawność"
              ? a.score - b.score
              : b.score - a.score
          );
          if (scoreList.length > 10) scoreList.splice(10);
          updateScoreTable();
        }
      }
    }

    function updateScoreTable() {
      scoreTableBody.innerHTML = "";
      scoreList.forEach((entry, index) => {
        const row = document.createElement("tr");
        row.innerHTML = `
          <td>${index + 1}</td>
          <td>${entry.nick}</td>
          <td>${entry.score}</td>
        `;
        scoreTableBody.appendChild(row);
      });
      localStorage.setItem("scoreList_" + currentMode, JSON.stringify(scoreList));
    }

    // Inicjalizacja przy ładowaniu strony
    initializeScoreList(currentMode);
  </script>
</body>
</html>
)rawliteral";

  return html;
}