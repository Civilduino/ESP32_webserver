#include <Arduino.h>
#include <WiFi.h>
#include <Ticker.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <my_server.h>
#include <canbus.h>

// GLOBAL: objects
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Ticker pinState;
Ticker tempTimer;
Ticker wsCleanupTimer;

// GLOBAL: variables
const char *ssid = "abcde";
const char *password = "abcde";
const char *PARAM_MESSAGE = "message";

// Flags
uint8_t tempAvailable = 0;
uint8_t wsCleanupAvailable = 0;

// PROTOTYPES functions
void setPin(void);
void ws_tempESP32(String temp);

void ESP32_tempFlag(void);
void ws_cleanupFlag(void);

// Setup
void setup()
{
  Serial.begin(115200);

  // Led
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinState.attach_ms(500, setPin);

  // Temperature
  tempTimer.attach_ms(1000, ESP32_tempFlag);

  // SPI_FFS
  if(!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // WiFi --------------------------------------------------------------------
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("WiFi Failed!");
    return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // mDNS
  start_mDNS("myserver");

  // Webserver ---------------------------------------------------------------
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", "Hello, world");
  });

  server.onNotFound(notFound);

  server.serveStatic("/index.html", SPIFFS, "/index.html").setCacheControl("max-age=5");
  server.serveStatic("/js_code.js", SPIFFS, "/js_code.js");

  server.begin();

  wsCleanupTimer.attach_ms(10000, ws_cleanupFlag);

  // CAN
  CAN_begin();
}

void loop()
{
  // Read internal temperature ESP32
  if(tempAvailable)
  {
    String tempESP32 = String(temperatureRead(), 2);
    Serial.println(tempESP32);
    ws_tempESP32(tempESP32);
    tempAvailable = 0;
  }

  // Clean old websocket clients
  if(wsCleanupAvailable)
  {
    ws.cleanupClients();
    wsCleanupAvailable=0;
  }
  
  // Read CAN bus messages
  CAN_read();

}

// FUNCTIONS

// General
void setPin(void)
{
  digitalWrite(LED_BUILTIN, !(digitalRead(LED_BUILTIN)));
}

void ESP32_tempFlag(void)
{
  tempAvailable = 1;
}

void ws_cleanupFlag(void)
{
  wsCleanupAvailable= 1;
}

void ws_tempESP32(String temp)
{
  DynamicJsonDocument msgServer(50);
  char tempJson[50];

  msgServer["temp"] = temp;
  serializeJson(msgServer, tempJson);
  ws.textAll(tempJson);
  Serial.println(tempJson);
}

