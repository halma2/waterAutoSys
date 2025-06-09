#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <time.h>
#include <RTClib.h>

const char* ssid = "";
const char* password = "";
const int pins[] = {0, 2, 3, 4, 5}; //Amilyen sorrendben vannak bekotve, NEM dinamikus lesz, aé 16-os--------------------------------------------------------------------------
#define NUM_PINS 5
#define EEPROM_SIZE 1024
#define MAX_SCHEDULES 50
#define NAME_SIZE 32 //TODO err
#define NAME_START_ADDR 4000  // avoid overlap with schedules


struct ScheduleEntry {
  int pin;
  int hour;
  int minute;
  int duration; // in minutes
  bool active; //todo isOn vs. isRunning vs. isActivated TODO OOOOOOOOOOOOOOOOOO: isOn zonActive olvas
};

ScheduleEntry schedules[MAX_SCHEDULES];
unsigned long pinTimers[NUM_PINS] = {0};
bool zoneActive[] = {0, 0, 0, 0, 0}; // not dynamio
String zoneNames[NUM_PINS]; // Indexed by pins[i]
RTC_DS3231 rtc;
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(1337);
char msg_buf[10];



// === Schedule Logic ===
void checkScheduledTasks() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    DateTime now = rtc.now();
    timeinfo.tm_hour = now.hour();
    timeinfo.tm_min = now.minute();
  }

  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (schedules[i].active &&
        schedules[i].hour == timeinfo.tm_hour &&
        schedules[i].minute == timeinfo.tm_min) {
      int idx = getPinIndex(schedules[i].pin);
      digitalWrite(schedules[i].pin, HIGH);
      pinTimers[idx] = millis() + schedules[i].duration * 60000UL;
    }
  }
}

void handleTimers() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_PINS; i++) {
    if (pinTimers[i] > 0 && now > pinTimers[i]) {
      digitalWrite(pins[i], LOW);
      pinTimers[i] = 0;
    }
  }
}

void loadSchedulesFromEEPROM() {
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    EEPROM.get(i * sizeof(ScheduleEntry), schedules[i]);
  }
}

void saveSchedulesToEEPROM() {
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    EEPROM.put(i * sizeof(ScheduleEntry), schedules[i]);
  }
  EEPROM.commit();
}

void addSchedule(int pin, int hour, int minute, int duration) {
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (!schedules[i].active) {///----------------------------------del-> mindden sh-ját töröljön------------------------------------------------------
      schedules[i] = {pin, hour, minute, duration, true};
      saveSchedulesToEEPROM();
      break;
    }
  }
}

void saveZoneNamesToEEPROM() {
  for (int i = 0; i < NUM_PINS; i++) {
    saveZoneNameToEEPROM(i);
  }
  //EEPROM.commit();
}

void saveZoneNameToEEPROM(int zoneIndex) { //todo lancolt listaban a ramban-> elég 1et törölni
  int base = NAME_START_ADDR + zoneIndex * NAME_SIZE;
  const char* name = zoneNames[pinNum].c_str();
  for (int j = 0; j < NAME_SIZE; j++) {
    EEPROM.write(base + j, name[j]);  // writes 0 if past length
  }
  EEPROM.commit();
}

void loadZoneNamesFromEEPROM() {
  for (int i = 0; i < NUM_PINS; i++) {
    int base = NAME_START_ADDR + i * NAME_SIZE;
    char buf[NAME_SIZE + 1];
    for (int j = 0; j < NAME_SIZE; j++) {
      buf[j] = EEPROM.read(base + j);
    }
    buf[NAME_SIZE] = '\0';  // always null-terminate
    zoneNames[i] = String(buf);
  }
}




int getPinIndex(int pin) { //if not exists, returns -1
  for (int i = 0; i < NUM_PINS; i++) {
    if (pins[i] == pin) return i;
  }
  return -1;
}

void handleCommand(char* payload) {
  Serial.printf("[%u] Received text: %s\n", client_num, payload);
  char* cmd = strtok(payload, ".");
  char* pinStr = strtok(NULL, ".");// Get the part after the dot
  int pin = pinStr ? atoi(pinStr) : -1;
  char* timeStr = strtok(NULL, ".");
  int time = timeStr ? atoi(timeStr) : 0;

  if (strcmp(cmd, "t_on") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0 || !zoneActive[getPinIndex(pin)])
      Serial.printf("%d.zone does not exists.\n", zone_num)
    else {
      digitalWrite(pin, HIGH);
      pinTimers[getPinIndex(pin)] = millis() + time * 60000UL;
    }
  }
  
  else if (strcmp(cmd, "t_off") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0 || !zoneActive[getPinIndex(pin)])
      Serial.printf("%d.zone does not exists.\n", zone_num)
    else {
      digitalWrite(pin, LOW);
      pinTimers[getPinIndex(pin)] = 0;
    }
  }

  else if (strcmp(cmd, "s_on") == 0 && pin >= 0 && timeStr) {
    if (getPinIndex(pin) < 0 || !zoneActive[getPinIndex(pin)])
      Serial.printf("%d.zone does not exists.\n", zone_num)
    else {
      // format: s_on.5.08:30.20
      char* hm = timeStr;
      char* durationStr = strtok(NULL, ".");
      if (!durationStr) return;

      int hour = atoi(strtok(hm, ":"));
      int minute = atoi(strtok(NULL, ":"));
      int duration = atoi(durationStr);
      addSchedule(pin, hour, minute, duration);
    }
  }

  else if (strcmp(cmd, "new") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0)
      Serial.printf("%d.zone does not exists.\n", zone_num)
    else if (zoneActive[getPinIndex(pin)])
      Serial.printf("%d.zone is already activated\n", zone_num);
    else {
      if (timeStr != NULL){
        zoneActive[getPinIndex(pin)] = true;
        Serial.printf("%d.zone is deactivated\n", zone_num);
      }
    }
  }

  else if (strcmp(cmd, "del") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0 || !zoneActive[getPinIndex(pin)])
      Serial.printf("%d.zone does not exists.\n", zone_num)
    else {
      zoneActive[getPinIndex(pin)] = false;
      Serial.printf("%d.zone is deactivated\n", zone_num);
    }
  }

  else if (strcmp(command, "name") == 0 && pin >= 0) {
    char* namePart = timeStr;
    if (!namePart) return;

    String nameStr = namePart;

    // Reconstruct full name if it includes dots (e.g. "Back.Lawn")
    char* nextPart;
    while ((nextPart = strtok(NULL, ".")) != NULL) {
      nameStr += "." + String(nextPart);
    }
    zoneNames[getPinIndex(pin)] = nameStr;
    saveZoneNameToEEPROM(getPinIndex(i));

    // Format JSON message to server
    String msg = "{";
    msg += "\"pin\":" + String(pin) + ",";
    msg += "\"name\":\"" + nameStr + "\"";
    msg += "}";

    Serial.printf("Sending zone name update to server: %s\n", msg.c_str());
    webSocket.sendTXT(msg);  // server handles name saving
  }


  else if (strcmp(command, "get") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0 || !zoneActive[getPinIndex(pin)])
      Serial.printf("%d.zone does not exists.\n", zone_num)
    else {
      //sprintf(msg_buf, "%d", zoneActive[getPinIndex(pin)]); //zones[pin_id].isOn TODO szerekeszt +isRunning (mindenkiét)
      // Build JSON response
      String response = "{"; //TODO String or char*
      response += "\"pin\":" + String(pin) + ",";
      response += "\"name\":\"" + zoneNames[i] + "\",";
      response += "\"status\":" + String(digitalRead(pin)) + ",";

      // Is there a schedule currently running? TODO on client side------------------------
      bool isRunning = (pinTimers[getPinIndex(pin)] > 0);
      response += "\"schedule_running\":" + String(isRunning ? "true" : "false") + ",";

      response += "\"schedules\":[";
      bool first = true;
      for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].active && schedules[i].pin == pin) {
          if (!first) response += ",";
          response += "{";
          response += "\"hour\":" + String(schedules[i].hour) + ",";
          response += "\"minute\":" + String(schedules[i].minute) + ",";
          response += "\"duration\":" + String(schedules[i].duration);
          response += "}";
          first = false;
        }
      }
      response += "]}";

      Serial.printf("Sending to [%u]: %s\n", client_num, response.c_str());
      webSocket.sendTXT(client_num, response);
    }
  }

  else if (strcmp(command, "zones") == 0) {
    if (pin != -1)
      Serial.printf("Invalid command.\n")
    else {
      // Build JSON response
      String response = "{"; //TODO String or char*
      for (int i = 0; i < NUM_PINS; i++) {
          int pin = pins[i];
          if (zoneActive[i]) {
            if (!first) response += ",";
            response += "\"" + i + "\":" + String(pin);
          }
        }
      response += "]}";
      Serial.printf("Sending active zones: %s\n", response.c_str());
      webSocket.sendTXT(client_num, response);
    }
  }

  else
    Serial.printf("[%u] Command not recognized\n", client_num);
}


void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void onWebSocketEvent(uint8_t client_num, WStype_t type, uint8_t* payload, size_t length){
  switch(type) {
    case WStype_DISCONNECTED: {
      Serial.printf("[%u] Disconnected!\n", client_num);
      break;
    }
    case WStype_CONNECTED:{
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from ", client_num);
        Serial.println(ip.toString());
      break;
    }
    case WStype_TEXT: {
      handleCommand((char*)payload);
      break;
    }
  }
}

void handleRoot(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/index.html", "text/html");
}
void handleCss(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/mystyle.css", "text/css");
}
void handleIco(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/favicon.ico", "image/x-icon");
}
void handleManifest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/manifest.json", "text/json"); //todo ell--------------------------
}
void handleNotFound(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}

void setup() {
  for (int i=0; i < sizeof(pins); i++) {
    pinMode(pins[i], OUTPUT);
    zones[i].id = pins[i];
  }
  zones[0].isActivated = true;
  zones[0].isOn = true;
  Serial.begin(115200);  
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  EEPROM.begin(EEPROM_SIZE);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  if (MDNS.begin("locsolás"))
    Serial.println("MDNS responder started");

  syncTime();//before that: connectWiFi();
  if (!rtc.begin()) Serial.println("RTC not found!");

  loadSchedulesFromEEPROM();
  loadZoneNamesFromEEPROM();

  server.on("/", handleRoot);
  server.on("/mystyle.css", HTTP_GET, handleCss);
  server.on("/favicon.ico", HTTP_GET, handleIco);
  server.on("/manifest.json", HTTP_GET, handleManifest);
  /*server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });*/
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("websocket started");
}
void loop() {
  webSocket.loop();
  checkScheduledTasks();
  handleTimers();
  //delay(2);//allow the cpu to switch to other tasks
}
