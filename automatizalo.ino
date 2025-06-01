#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>

const char* ssid = "TP_LINK_340_3";
const char* password = "pumu89kli";
const int pins[] = {0, 2, 3, 4, 5}; //Amilyen sorrendben vannak bekotve, NEM dinamikus lesz, aé 16-os--------------------------------------------------------------------------

struct zone {
  int id;
  bool isOn = false;
  bool isRuning = false;
  bool isActivated = false;
  //period* periods = {};
  zone() : id(0), isOn(false), isRuning(false), isActivated(false) {}
};
zone zones[sizeof(pins)];
/*struct period {
  int st_h;
  int st_min;
  int per_min;
}*/
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(1337);
char msg_buf[10];


int zoneExists(int id) {
  for (int i= 0; i < sizeof(zones); i++) {
    if (zones[i].id == id)
      return i;
  }
  return -1;
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
    case WStype_TEXT: { //Format: "cmd.id_num"
      Serial.printf("[%u] Received text: %s\n", client_num, payload);
      char* command = strtok((char *)payload, ".");
      char* number = strtok(NULL, "."); // Get the part after the dot
      if (command != NULL && number != NULL) {
        if (strcmp(command, "new") == 0) {
          int pin_id = atoi(number);
          int zone_num = zoneExists(pin_id);
          if (zone_num == -1)
            Serial.printf("%d.zone does not exists.\n", zone_num);
          else if (zones[zone_num].isActivated)
            Serial.printf("%d.zone is already activated\n", zone_num);
          else {
            zones[zone_num].isActivated = true;
            zones[zone_num].isOn = true;
            Serial.printf("%d.zone is activated\n", zone_num);
          }
         }

        if (strcmp(command, "t_on") == 0) {
          int pin_id = atoi(number);
          int zone_num = zoneExists(pin_id);
          if (zone_num == -1)
            Serial.printf("%d.zone is already activated or not exists.\n", zone_num);
          else if (!zones[zone_num].isActivated)
            Serial.printf("%d.zone is not activated\n", zone_num);
          else if (zones[zone_num].isOn)
            Serial.printf("%d.zone is already on\n", zone_num);
          else {
            zones[zone_num].isOn = true;
            Serial.printf("%d.zone is on\n", zone_num);
            digitalWrite(pin_id, HIGH); //TODO kiszed
          }
         }

        /*else if (strcmp(command, "set") == 0) { //TODO átalakt-> időpont
        int pin_id = atoi(number);
        int zone_num = activateZone(pin_id)
        if (zone_num == -1)
          Serial.printf("%d.zone is already activated or not exists.\n", zone_num);
        else if (!zones[zone_num].isActivated)
          Serial.printf("%d.zone is not activated\n", zone_num);
        else if (!zones[zone_num].isOn) //TODO kiszed
          Serial.printf("%d.zone is not on\n", zone_num);
        else {
          zones[zone_num],isOn = true;
          Serial.printf("%d.zone is activated\n", zone_num);
          digitalWrite(pin_id, HIGH); //TODO kiszed
        }
        }*/

        else if (strcmp(command, "t_off") == 0) {
          int pin_id = atoi(number);
          int zone_num = zoneExists(pin_id);
          if (zone_num == -1)
            Serial.printf("%d.zone is already activated or not exists.\n", zone_num);
          else if (!zones[zone_num].isActivated)
            Serial.printf("%d.zone is not activated\n", zone_num);
          else if (!zones[zone_num].isOn)
            Serial.printf("%d.zone is already off\n", zone_num);
          else {
            zones[zone_num].isOn = false;
            Serial.printf("%d.zone is off\n", zone_num);
            digitalWrite(pin_id, LOW); //TODO kiszed
          }
        }

        else if (strcmp(command, "del") == 0) {
          int pin_id = atoi(number);
          int zone_num = zoneExists(pin_id);
          if (zone_num == -1)
            Serial.printf("%d.zone does not exists.\n", zone_num);
          else if (!zones[zone_num].isActivated)
            Serial.printf("%d.zone is already deactivated\n", zone_num);
          else {
            zones[zone_num].isActivated = false;
            zones[zone_num].isOn = false;
            Serial.printf("%d.zone is deactivated\n", zone_num);
          }
         }
        
        else if (strcmp(command, "get") == 0) {
          int pin_id = atoi(number);
          int zone_num = zoneExists(pin_id);
          if (zone_num == -1)
             Serial.printf("%d.zone does not exists.\n", zone_num);
          else {
            sprintf(msg_buf, "%d", zones[pin_id].isOn); //TODO szerekeszt +isRunning
            Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
            webSocket.sendTXT(client_num, msg_buf);
          }
        } 
        else {
          Serial.printf("[%u] Command not recognized\n", client_num);
        }
      } 
      else {
        Serial.printf("[%u] Invalid message format\n", client_num);
      }
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
  server.on("/", handleRoot);
  server.on("/mystyle.css", HTTP_GET, handleCss);
  server.on("/favicon.ico", HTTP_GET, handleIco);
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
  //delay(2);//allow the cpu to switch to other tasks
}
