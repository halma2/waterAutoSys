#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
//#include <EEPROM.h>
#include <time.h>
#include <RTClib.h>

const char* ssid = "";
const char* password = "";
const int pins[] = {15, 16, 18, 5, 19}; //Amilyen sorrendben vannak bekotve, NEM dinamikus lesz, aé 16-os--------------------------------------------------------------------------
#define NUM_PINS 5
#define EEPROM_SIZE 512
#define MAX_SCHEDULES 50
#define NAME_SIZE 25 //TODO err
#define ZONE_EEPROM_START 380  // avoid overlap with schedules
#define SCHEDULE_SIZE 5
#define MAX_UNSAVED_SCH 10
#define SAVE_MIN_PERIOD 15


struct ScheduleEntry {
    bool deleted; // <-> deleted already -> can be overwritten
    uint8_t pin;
    uint8_t hour;
    uint8_t minute;
    uint8_t duration; // in minutes
    //uint8_t days;
};
struct zone {
    bool activated;
    bool turnedOn;
    char name[NAME_SIZE];
};

ScheduleEntry schedules[MAX_SCHEDULES];
unsigned long pinTimers[NUM_PINS] = {0};
unsigned long saveTimer = 0;
zone zones[] = {{0,0,""},{0,0,""},{0,0,""},{0,0,""},{0,0,""}};// todo not dynamio
bool zRenamed[5];
bool zturnedChanged[5];
bool zActChanged[5];
int unsavedSchedIndexes[MAX_SCHEDULES]; //can be overflow
int unsavedNum = 0;
RTC_DS3231 rtc;
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(1337);



/*int getScheduleEEPROMAddr(int index) {
    return index * SCHEDULE_SIZE;
}
void loadSchedulesFromEEPROM() {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        int base = getScheduleEEPROMAddr(i);
        schedules[i].deleted = EEPROM.read(base);
        schedules[i].pin = EEPROM.read(base + 1);
        schedules[i].hour = EEPROM.read(base + 2);
        schedules[i].minute = EEPROM.read(base + 3);
        schedules[i].duration = EEPROM.read(base + 4) << 8 | EEPROM.read(base + 5);
    }
}

void saveScheduleToEEPROM(int i, bool edited, bool deleted) {
    int base = getScheduleEEPROMAddr(i);
    if (deleted)
        EEPROM.write(base, 1); //schedules[i].deleted
    else {
        if (!edited) { //created
            EEPROM.write(base, 0);
            EEPROM.write(base + 1, schedules[i].pin);
        }
        EEPROM.write(base + 2, schedules[i].hour);
        EEPROM.write(base + 3, schedules[i].minute);
        EEPROM.write(base + 4, (schedules[i].duration >> 8) & 0xFF);
        EEPROM.write(base + 5, schedules[i].duration & 0xFF);
    }
    EEPROM.commit();
}**/

void addSchedule(uint8_t pin, uint8_t hour, uint8_t minute, uint8_t duration) {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].deleted) { // means that place is free
            schedules[i] = {0, pin, hour, minute, duration};
            bool inUnsaved = false; // means that place was recently deleted <-> index still in unsavedArr
            for (int j = 0; j < MAX_UNSAVED_SCH; j++) {
              if (unsavedSchedIndexes[j] == -i) {
                unsavedSchedIndexes[j] = i;
                inUnsaved = true;
                break;
              }
            }
            if (!inUnsaved){
              unsavedSchedIndexes[unsavedNum] = i;
              unsavedNum++;
            }
            break;
        }
    }
}
void deleteSchedule(int index) {
    if (index < 0 || index >= MAX_SCHEDULES) return;
    schedules[index].deleted = true;
    unsavedSchedIndexes[unsavedNum] = -index; // negative means to be deleted
    unsavedNum++;
}
void editSchedule(int index) { //todo delete function
    if (index < 0 || index >= MAX_SCHEDULES) return;
    unsavedSchedIndexes[unsavedNum] = index;
    unsavedNum++;
}


/*int getZoneEEPROMAddr(int zoneIndex) {
    return ZONE_EEPROM_START + zoneIndex * (2 + NAME_SIZE);
}
void saveZoneActivatedToEEPROM(int zIndex) {
    int addr = getZoneEEPROMAddr(zIndex);
    EEPROM.write(addr, zones[zIndex].activated ? 1 : 0);
    EEPROM.commit();
}
void saveZoneTurnedOnToEEPROM(int zIndex) {
    int addr = getZoneEEPROMAddr(zIndex) + 1;
    EEPROM.write(addr, zones[zIndex].turnedOn ? 1 : 0);
    EEPROM.commit();
}
void saveZoneNameToEEPROM(int zIndex) {
    int addr = getZoneEEPROMAddr(zIndex) + 2;
    for (int j = 0; j < NAME_SIZE; j++) {
        EEPROM.write(addr + j, zones[zIndex].name[j]);
    }
    EEPROM.commit();
}
void loadZonesFromEEPROM() {
    for (int i = 0; i < NUM_PINS; i++) {
        int base = getZoneEEPROMAddr(i);
        zones[i].activated = EEPROM.read(base);
        zones[i].turnedOn  = EEPROM.read(base + 1);
        for (int j = 0; j < NAME_SIZE; j++) {
            zones[i].name[j] = EEPROM.read(base + 2 + j);
        }
    }
}*/


void saveData(){
  /*for (int i = 0; i < sizeof(unsavedSchedIndexes); i++) {
    if (unsavedSchedIndexes[i] >= 0)
      saveScheduleToEEPROM(unsavedSchedIndexes[i], false, false);
    else
      saveScheduleToEEPROM(-unsavedSchedIndexes[i], false, true); //to be deleted
  }
  for (int i = 0; i < sizeof(zones); i++) {
    if (zRenamed[i]) {
      saveZoneNameToEEPROM(i);
      zRenamed[i] = false;
    }
    if (zturnedChanged[i]) {
      saveZoneTurnedOnToEEPROM(i);
      zturnedChanged[i] = false;
    }
    if (zActChanged[i]) {
      saveZoneActivatedToEEPROM(i);
      zActChanged[i] = false;
    }      
  }*/
  unsavedNum = 0;
}


void checkScheduledTasks() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    DateTime now = rtc.now();
    timeinfo.tm_hour = now.hour();
    timeinfo.tm_min = now.minute();
  }

  for (int i = 0; i < MAX_SCHEDULES; i++) {
    int idx = getPinIndex(schedules[i].pin);
    if (zones[idx].activated && zones[idx].turnedOn &&
        schedules[i].hour == timeinfo.tm_hour &&
        schedules[i].minute == timeinfo.tm_min) {

      digitalWrite(schedules[i].pin, HIGH);
      pinTimers[idx] = millis() + schedules[i].duration * 60000UL;
    }
  }
  if (saveTimer == 0) {
    saveTimer = millis() + 60000UL * SAVE_MIN_PERIOD;
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
  if (now > saveTimer || unsavedNum == MAX_UNSAVED_SCH) {
    saveTimer = 0;
    saveData();
  }
}


int getPinIndex(int pin) { //if not exists, returns -1
  for (int i = 0; i < NUM_PINS; i++) {
    if (pins[i] == pin) return i;
  }
  return -1;
}

void handleCommand(char* payload, uint8_t client_num) {
  Serial.printf("Received text: %s\n", payload);
  char* cmd = strtok(payload, ".");
  char* pinStr = strtok(NULL, ".");// Get the part after the dot
  int pin = pinStr ? atoi(pinStr) : -1;
  char* timeStr = strtok(NULL, ".");
  int time = timeStr ? atoi(timeStr) : 0;

  if (strcmp(cmd, "t_on") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0 || !zones[getPinIndex(pin)].activated)
      Serial.printf("[t_on] %d.zone does not exists.\n", pin);
    else if (!zones[getPinIndex(pin)].turnedOn)
        Serial.printf("[%u] %d.zone is turned off.\n", client_num, pin);
    else {
      digitalWrite(pin, HIGH);
      pinTimers[getPinIndex(pin)] = millis() + time * 60000UL;
    }
  }
  
  else if (strcmp(cmd, "t_off") == 0 && pin >= 0) {
    if (getPinIndex(pin) < 0 || !zones[getPinIndex(pin)].activated)
      Serial.printf("[t_off] %d.zone does not exists.\n", pin);
    else if (!zones[getPinIndex(pin)].turnedOn)
        Serial.printf("[%u] %d.zone is turned off.\n", client_num, pin);
    else {
      digitalWrite(pin, LOW);
      pinTimers[getPinIndex(pin)] = 0;
    }
  }

  else if (strcmp(cmd, "z_on") == 0 && pin >= 0) {
    int idx = getPinIndex(pin);
    if (getPinIndex(pin) < 0 || !zones[idx].activated)
      Serial.printf("[z_on] %d.zone does not exists.\n", pin);
    else {
      zones[idx].turnedOn = true;
      zturnedChanged[idx] = !zturnedChanged[idx];
    }
  }

  else if (strcmp(cmd, "z_off") == 0 && pin >= 0) {
    int idx = getPinIndex(pin);
    if (getPinIndex(pin) < 0 || !zones[idx].activated)
      Serial.printf("[z_off] %d.zone does not exists.\n", pin);
    else {
      zones[idx].turnedOn = false;
      zturnedChanged[idx] = !zturnedChanged[idx];
    }
  }

  else if (strcmp(cmd, "z_new") == 0 && pin >= 0) {
    int idx = getPinIndex(pin);
    if (idx < 0)
      Serial.printf("[z_new] %d.zone does not exists.\n", pin);
    else if (zones[idx].activated)
      Serial.printf("[z_new] %d.zone is already activated\n", pin);
    else {
      if (!timeStr){ //no extra
        zones[idx].activated = true;
        zones[idx].turnedOn = true;
        zActChanged[idx] = !zActChanged[idx];
        Serial.printf("[%u] %d.zone is activated\n", client_num, pin);
      }
    }
  }

  else if (strcmp(cmd, "z_del") == 0 && pin >= 0) {
    int idx = getPinIndex(pin);
    if (getPinIndex(pin) < 0 || !zones[idx].activated)
      Serial.printf("[z_del] %d.zone does not exists.\n", pin);
    else {
      zones[idx].activated = false;
      zActChanged[idx] = !zActChanged[idx];
        for (int i = 0; i < sizeof(schedules); ++i) { //also deletes its all schedules
            if (!schedules[i].deleted && schedules[i].pin == pin) {
               //deleteSchedule(i);
            }
        }
      Serial.printf("[%u] %d.zone is deactivated\n", client_num, pin);
    }
  }

  else if (strcmp(cmd, "sn") == 0 && pin >= 0 && timeStr) { // new schedule
      if (getPinIndex(pin) < 0 || !zones[getPinIndex(pin)].activated)
          Serial.printf("[sn] %d.zone does not exists.\n", pin);
      else {
          // format: s_on.5.08:30.20
          char* hm = timeStr;
          char* durationStr = strtok(NULL, ".");
          if (!durationStr) return;

          int hour = atoi(strtok(hm, ":"));
          int minute = atoi(strtok(NULL, ":"));
          int duration = atoi(durationStr);
          //addSchedule(pin, hour, minute, duration);
      }
  }

  else if (strcmp(cmd, "sd") == 0 && pin >= 0 && timeStr) { // delete schedule
      if (getPinIndex(pin) < 0 || !zones[getPinIndex(pin)].activated)
          Serial.printf("[sd] %d.zone does not exists.\n", pin);
      else {
          // format: sd.5.08:30 todo check err
          char* hm = timeStr;
          int hour = atoi(strtok(hm, ":"));
          int minute = atoi(strtok(NULL, ":"));
          int idx;
          for (idx = 0; idx < sizeof(schedules); idx++) {
              if (schedules[idx].pin == pin &&
              schedules[idx].hour == hour &&
              schedules[idx].minute == minute) {
                  break;
              }
          }
          //deleteSchedule(idx);
      }
  }

  else if (strcmp(cmd, "se") == 0 && pin >= 0 && timeStr) { // edit schedule
      if (getPinIndex(pin) < 0 || !zones[getPinIndex(pin)].activated)
          Serial.printf("[se] %d.zone does not exists.\n", pin);
      else {
          // format: se.5.08:30.20 todo check err
          char* hm = timeStr;
          char* durationStr = strtok(NULL, ".");
          if (!durationStr) return;
          int hour = atoi(strtok(hm, ":"));
          int minute = atoi(strtok(NULL, ":"));
          int duration = atoi(durationStr);
          int idx = 0;
          for (int i = 0; i < sizeof(schedules); ++i) {
              if (schedules[i].pin == pin &&
              schedules[i].hour == hour &&
              schedules[i].minute == minute) {
                  idx = i;
                  break;
              }
          }
          schedules[idx].hour = hour;
          schedules[idx].minute = minute;
          schedules[idx].duration = duration;
          //editSchedule(idx);
      }
  }

  else if (strcmp(cmd, "nm") == 0 && pin >= 0) { // rename zone
    char* namePart = timeStr;
    if (!namePart) return;

    String nameStr = namePart;

    // Reconstruct full name if it includes dots (e.g. "Back.Lawn")
    char* nextPart;
    while ((nextPart = strtok(NULL, ".")) != NULL) {
      nameStr = nameStr + "." + String(nextPart);
    }
    int index = getPinIndex(pin);
    strncpy(zones[index].name, nameStr.c_str(), NAME_SIZE - 1);
    zones[index].name[NAME_SIZE - 1] = '\0'; //= zones[getPinIndex(pin)].name = nameStr;
    zRenamed[index] = true;

    // Format JSON message to server
    String msg = "{\"type\":\"name\",";
    msg += "\"pin\":" + String(pin) + ",";
    msg += "\"name\":\"" + nameStr + "\"";
    msg += "}";


    Serial.printf("[%u] Sending zone name update to server: %s\n", client_num, msg.c_str());
    webSocket.sendTXT(client_num, msg);  // server handles name saving
  }

  else if (strcmp(cmd, "get") == 0 && pin >= 0) {
    int idx = getPinIndex(pin);
    if (getPinIndex(pin) < 0 || !zones[idx].activated)
      Serial.printf("[get] %d.zone does not exists.\n", pin);
    else {
      // Build JSON response
      String response = "{\"type\":\"zone-schedules\",";
      response += "\"pin\":" + String(pin) + "," +
        "\"name\":\"" + zones[idx].name + "\"," +
        "\"turnedOn\":\"" + zones[idx].turnedOn + "\"," +
        "\"status\":" + String(digitalRead(pin)) + ",";

      // Is there a schedule currently running? TODO on client side------------------------
      //bool isRunning = (pinTimers[idx] > 0);
      //response = response + "\"schedule_running\":" + String(isRunning ? "true" : "false") + "\",
      response += "\"schedules\":[";
      bool first = true;
      for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!schedules[i].deleted && schedules[i].pin == pin) {
          if (!first) response += ",";
          response = response + "{" +
            "\"hour\":" + String(schedules[i].hour) + "," +
            "\"minute\":" + String(schedules[i].minute) + "," +
            "\"duration\":" + String(schedules[i].duration) +
            "}";
          first = false;
        }
      }
      response += "]}";

      Serial.printf("[%u] Sending: %s\n", client_num, response.c_str());
      webSocket.sendTXT(client_num, response.c_str());
    }
  }

  else if (strcmp(cmd, "zones") == 0) {
    if (pin != -1)
      Serial.printf("[zones] Invalid command.\n");
    else {
      bool first = true;
      // Build JSON response
      String response = "{\"type\":\"zones\",\"zones\":[";
      for (int i = 0; i < NUM_PINS; i++) {        
          if (zones[i].activated) {
            if (!first) response += ",";
            response += "{\"pin\":" + String(pins[i]) + 
            //",\"activated\":" + String((zones[i].activated)?"true":"false") +
            ",\"turnedOn\":" + String((zones[i].turnedOn)?"true":"false") +
            ",\"isRunning\":" + String((pinTimers[i] > 0)?"true":"false") + "}";
            first = false;
          }
        }
      response += "]}";
      Serial.printf("Sending active zones: %s\n", response.c_str());
      webSocket.sendTXT(client_num, response);
    }
  }

  else if (strcmp(cmd, "new_zones") == 0) {
    if (pin != -1)
      Serial.printf("[new_zones] Invalid command.\n");
    else {
      bool first = true;
      // Build JSON response
      String response = "{\"type\":\"off_zones\",\"off_zones\":[";
      for (int i = 0; i < NUM_PINS; i++) {
          if (!zones[i].activated) {
            if (!first) response += ",";
            response += String(pins[i]);
            first = false;
          }
        }
      response += "]}";
      Serial.printf("[%u] Sending inactive zones: %s\n", client_num, response.c_str());
      webSocket.sendTXT(client_num, response);
    }
  }
  
  else if (strcmp(cmd, "ip") == 0) {
    String response = "{\"type\":\"ip\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    webSocket.sendTXT(client_num, response);
  }

  else
    Serial.printf("[%u] Command not recognized\n", client_num);
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
      handleCommand((char*)payload, client_num);
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
void handleJs(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/site.js", "text/js");
}
void handleIco(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/favicon.ico", "image/x-icon");
}
void handleManifest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/manifest.json", "text/json");
}
void handleNotFound(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() + "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}

void setup() {
  for (int i=0; i < sizeof(pins); i++) {
    pinMode(pins[i], OUTPUT);
  }
  Serial.begin(115200);  
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  //EEPROM.begin(EEPROM_SIZE);
  
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
  /*delay(100);  // Let TCP/IP stack stabilize
  if (MDNS.begin("locsolás"))
    Serial.println("MDNS responder started");
  delay(100);  // Safe delay before WebSocket begin*/
  //loadSchedulesFromEEPROM();
  //loadZonesFromEEPROM();

  server.on("/", handleRoot);
  //server.on("/mystyle.css", HTTP_GET, handleCss);
  //server.on("/site.js", HTTP_GET, handleJs);
  //server.on("/favicon.ico", HTTP_GET, handleIco);
  //server.on("/manifest.json", HTTP_GET, handleManifest);
  /*server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });*/
  //server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  //webSocket.begin();
  //webSocket.onEvent(onWebSocketEvent);
  //Serial.println("websocket started");

  // NTP Setup
  /*configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // Start RTC
  if (!rtc.begin()) Serial.println("RTC not found!");
  else {
    if (rtc.lostPower()) Serial.println("RTC lost power.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Wait for NTP (max 5s)
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 1000000000 && retries < 10) {
      delay(500);
      now = time(nullptr);
      retries++;
    }
    if (now > 1000000000) {
    Serial.println("NTP time obtained!");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
  
    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec));
    }
  }*/
  /*else {
    Serial.println("Failed to get time from NTP, falling back to RTC.");
    DateTime rtcNow = rtc.now();
    struct timeval tv = {
      .tv_sec = rtcNow.unixtime(),
      .tv_usec = 0
    };
    settimeofday(&tv, nullptr);  // Set system time from RTC
  }*/
}
void loop() {
  webSocket.loop();
  //checkScheduledTasks();
  //handleTimers();
  delay(100);//allow the cpu to switch to other tasks
}
