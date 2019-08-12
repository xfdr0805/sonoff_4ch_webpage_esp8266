#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <FS.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <ESP8266httpUpdate.h>
#include <OneButton.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#define LED_BUILD 2
#define CONFIG_BTN 0
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif
const char *ssid = "shengji3";
const char *password = "sonavox168";
const char *mqtt_server = "sonavox.top";
const char *ntp_server = "ntp1.aliyun.com";
const uint16_t mqtt_port = 1883;
static WiFiEventHandler e1, e2, e3;
boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent;            // Last triggered event
int8_t timeZone = 8;
int8_t minutesTimeZone = 0;
bool wifiFirstConnected = false;
WiFiClient espClient;
PubSubClient client(espClient);
String topic_barcode = "/barcode";
String topic_info = "/info";
String topic_leave = "/leave";
long lastMsg = 0;
char msg[50];
char buffer[128];
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");           // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)
String mac = "";

OneButton button = OneButton(CONFIG_BTN, true, true);
DynamicJsonBuffer jsonBuffer;
void onSTAConnected(WiFiEventStationModeConnected ipInfo)
{
  DEBUG_PRINT("Connected to %s\r\n", ipInfo.ssid.c_str());
}

// Start NTP only after IP network is connected
void onSTAGotIP(WiFiEventStationModeGotIP ipInfo)
{
  DEBUG_PRINT("Got IP: %s\r\n", ipInfo.ip.toString().c_str());
  DEBUG_PRINT("Connected: %s\r\n", WiFi.status() == WL_CONNECTED ? "yes" : "no");
  digitalWrite(LED_BUILD, LOW); // Turn on LED
  wifiFirstConnected = true;
}

// Manage network disconnection
void onSTADisconnected(WiFiEventStationModeDisconnected event_info)
{
  DEBUG_PRINT("Disconnected from SSID: %s\n", event_info.ssid.c_str());
  DEBUG_PRINT("Reason: %d\n", event_info.reason);
  digitalWrite(LED_BUILD, HIGH);
}
//长按3秒种进入配网模式
void smart_config()
{
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  //Wait for SmartConfig packet from mobile
  DEBUG_PRINT("\r\nWaiting for SmartConfig.");
  while (!WiFi.smartConfigDone())
  {
    delay(200);
    //Serial.print(".");
    digitalWrite(LED_BUILD, !digitalRead(LED_BUILD));
  }
  DEBUG_PRINT("SmartConfig Received.");
  //Wait for WiFi to connect to AP
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(600);
    Serial.print(".");
    digitalWrite(LED_BUILD, !digitalRead(LED_BUILD));
  }
  DEBUG_PRINT("\r\nWiFi Connected.");
  DEBUG_PRINT("ip:%s\r\n", WiFi.localIP().toString().c_str());
  digitalWrite(LED_BUILD, 0);
  delay(1000);
  DEBUG_PRINT("Reset CPU\r\n");
  ESP.restart();
}
void callback(char *topic, byte *payload, unsigned int length)
{
  // if (strcmp_P(topic, topic_info.c_str()) == 0)
  // {
  //     return;
  // }
  memset(buffer, 0, 128);
  memcpy_P(buffer, payload, length);
  DEBUG_PRINT(topic);
  Serial.print(buffer);

  jsonBuffer.clear();
  JsonObject &root = jsonBuffer.parseObject(buffer);
  if (!root.success())
  {
    DEBUG_PRINT("parseObject() failed");
    return;
  }
}
void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {

    // Attempt to connect
    if (client.connect(mac.c_str(), "iot", "Iot_123"))
    {
      DEBUG_PRINT("MQTT Connected\r\n");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe(topic_info.c_str());
      client.subscribe(topic_leave.c_str());
    }
    else
    {
      Serial.print("Attempting MQTT connection...");
      Serial.print("failed, rc=");
      Serial.print(client.state());
      DEBUG_PRINT(" try again in 3 seconds");
    }
  }
}

void update()
{
  ESPhttpUpdate.setLedPin(LED_BUILD, LOW);
  t_httpUpdate_return ret = ESPhttpUpdate.update(espClient, "http://192.168.1.8/barcode.bin");
  //t_httpUpdate_return  ret = ESPhttpUpdate.update("https://server/file.bin", "", "fingerprint");

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    DEBUG_PRINT("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    DEBUG_PRINT("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    DEBUG_PRINT("HTTP_UPDATE_OK");
    break;
  }
}
void Short_click()
{
}
void long_click()
{
  smart_config();
}
void IntCallback()
{
}
void processSyncEvent(NTPSyncEvent_t ntpEvent)
{
  if (ntpEvent < 0)
  {
    DEBUG_PRINT("Time Sync error: %d\n", ntpEvent);
    if (ntpEvent == noResponse)
      DEBUG_PRINT("NTP server not reachable");
    else if (ntpEvent == invalidAddress)
      DEBUG_PRINT("Invalid NTP server address");
    else if (ntpEvent == errorSending)
      DEBUG_PRINT("Error sending request");
    else if (ntpEvent == responseError)
      DEBUG_PRINT("NTP response error");
  }
  else
  {
    if (ntpEvent == timeSyncd)
    {
      DEBUG_PRINT("Got NTP time: ");
      DEBUG_PRINT(NTP.getTimeDateString(NTP.getLastNTPSync()).c_str());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    //client connected
    DEBUG_PRINT("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    //client disconnected
    DEBUG_PRINT("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    //error was received from the other end
    DEBUG_PRINT("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    //pong message was received (in response to a ping request maybe)
    DEBUG_PRINT("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    //data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      DEBUG_PRINT("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;
        DEBUG_PRINT("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < info->len; i++)
        {
          DEBUG_PRINT("%02x ", data[i]);
        }
        DEBUG_PRINT("\n");
      }
      if (info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          DEBUG_PRINT("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        DEBUG_PRINT("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      DEBUG_PRINT("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT)
      {
        data[len] = 0;
        DEBUG_PRINT("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          DEBUG_PRINT("%02x ", data[i]);
        }
        DEBUG_PRINT("\n");
      }

      if ((info->index + len) == info->len)
      {
        DEBUG_PRINT("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          DEBUG_PRINT("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}
void setup()
{
  Serial.begin(115200);

  pinMode(LED_BUILD, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(15, OUTPUT);
  digitalWrite(16, 1);
  digitalWrite(15, 1);
  digitalWrite(LED_BUILD, HIGH);
  button.attachClick(Short_click);
  button.attachLongPressStart(long_click);
  WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  // long lastMsg = millis();
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(50);
  //   DEBUG_PRINT(".");
  //   digitalWrite(LED_BUILD, !digitalRead(LED_BUILD));
  //   //这里如果没有网络会陷入死循环
  //   if (millis() - lastMsg > 10000)
  //   {
  //     break;
  //   }
  // }
  WiFi.begin();
  e1 = WiFi.onStationModeGotIP(onSTAGotIP); // As soon WiFi is connected, start NTP Client
  e2 = WiFi.onStationModeDisconnected(onSTADisconnected);
  e3 = WiFi.onStationModeConnected(onSTAConnected);
  DEBUG_PRINT("Compiled DateTime: %s %s\n", __DATE__, __TIME__);
  mac = WiFi.macAddress();
  //mac.replace(":", "");                //去掉：号
  mac.toLowerCase();                   //转为小写
  topic_barcode = mac + topic_barcode; //发布条码信息
  topic_info = mac + topic_info;       //获取参数
  topic_leave = mac + topic_leave;     //获取参数
  DEBUG_PRINT("topic barcode:%s\n", topic_barcode.c_str());
  DEBUG_PRINT("topic info:%s\n", topic_info.c_str());
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  //NTP对时
  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });
  // NTP.begin(ntp_server, timeZone, true, minutesTimeZone);
  // NTP.setInterval(60); //60S对一次

  //irrecv.enableIRIn(); // Start the receiver
  //https://www.jianshu.com/p/014bcae94c8b
  //https://github.com/pellepl/spiffs/wiki/Using-spiffs
  bool ok = SPIFFS.begin();
  if (ok)
  {
    DEBUG_PRINT("Spiffs Mount Ok\r\n");
    //SPIFFS.format();
    //SPIFFS 1M
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
      File f = dir.openFile("r");
      DEBUG_PRINT("File Name:%s File Size:%d\r\n", dir.fileName().c_str(), f.size());
    }
    //模式为w时，是不能读取的，必须分开处理
    File f = SPIFFS.open("/remote_code.cfg", "r");
    if (!f)
    {
      DEBUG_PRINT("Remote Code Config Is Not Exists\r\n");
    }
    else
    {
      String json = "";
      while (f.available())
      {
        json += char(f.read());
      }
      DEBUG_PRINT("Remote Data:%s\r\n", json.c_str());
      JsonObject &root = jsonBuffer.parseObject(json);
      if (!root.success())
      {
        f.close();
      }
      else
      {
      }
    }
  }
  else
  {
    DEBUG_PRINT("Spiffs Mount Failed\r\n");
  }

  //attachInterrupt(digitalPinToInterrupt(CS8422_INT_PIN), IntCallback, RISING);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("info.html").setAuthentication("admin", "admin");
  //server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setAuthentication("admin", "admin");

  //server.serveStatic("/js", SPIFFS, "/js");
  //server.serveStatic("/style", SPIFFS, "/style");
  //server.serveStatic("/fonts", SPIFFS, "/fonts");
  server.onNotFound([](AsyncWebServerRequest *request) {
    DEBUG_PRINT("NOT_FOUND: ");
    DEBUG_PRINT(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength())
    {
      DEBUG_PRINT("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      DEBUG_PRINT("_CONTENT_LENGTH: %u\n", request->contentLength());
    }
    int headers = request->headers();
    for (int i = 0; i < headers; i++)
    {
      AsyncWebHeader *h = request->getHeader(i);
      DEBUG_PRINT("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }
    request->send(404);
  });

  server.on("/get_info", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    //int index = p->value().toInt();
    root["heap"] = ESP.getFreeHeap();

    response->setLength();
    request->send(response);
  });
  server.on("/get_remote_cfg", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    //root["remote_code"] = remote_code;

    response->setLength();
    request->send(response);
  });
  server.on("/set_remote_cfg", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebParameter *p = request->getParam(0);
    DEBUG_PRINT("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &rsp = response->getRoot();
    JsonObject &root = jsonBuffer.parseObject(p->value());
    if (!root.success())
    {
      rsp["status"] = "failed";
      response->setLength();
      request->send(response);
      return;
    }
    //保存参数
    File f = SPIFFS.open("/remote_code.cfg", "w");
    f.print(p->value());
    f.close();

    response->setLength();
    request->send(response);
  });
  // server.on("/info.html", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   //String amp_name = request->getParam(0)->value();
  //   request->send(SPIFFS, "/burn.html");
  // });

  server.on("/get_time", HTTP_POST, [](AsyncWebServerRequest *request) {
    //int params = request->params();
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();
    root["rtc_time"] = "2019/05/30 12：00：00";
    response->setLength();
    request->send(response);
  });
  server.on("/get_dac_status", HTTP_POST, [](AsyncWebServerRequest *request) {
    //int params = request->params();
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &root = response->getRoot();

    response->setLength();
    request->send(response);
  });
  server.on("/set_volume", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebParameter *p = request->getParam(0);
    DEBUG_PRINT("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &rsp = response->getRoot();
    JsonObject &root = jsonBuffer.parseObject(p->value());
    if (!root.success())
    {
      rsp["status"] = "failed";
      response->setLength();
      request->send(response);
      return;
    }
    //master_volume = root["volume"];
    rsp["status"] = "success";
    response->setLength();
    request->send(response);
  });
  server.on("/set_source", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebParameter *p = request->getParam(0);
    DEBUG_PRINT("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    AsyncJsonResponse *response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonObject &rsp = response->getRoot();
    JsonObject &root = jsonBuffer.parseObject(p->value());
    if (!root.success())
    {
      rsp["status"] = "failed";
      response->setLength();
      request->send(response);
      return;
    }
    rsp["status"] = "success";
    response->setLength();
    request->send(response);
  });

  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  // attach AsyncEventSource
  server.addHandler(&events);
  server.begin();
}
void loop()
{
  client.loop();
  button.tick();
  if (wifiFirstConnected)
  {
    wifiFirstConnected = false;
    NTP.setInterval(60);
    NTP.begin(ntp_server, timeZone, true, minutesTimeZone);
  }
  if (syncEventTriggered)
  {
    processSyncEvent(ntpEvent);
    syncEventTriggered = false;
  }
  if (!client.connected() && (WiFi.status() == WL_CONNECTED))
  {
    reconnect();
  }
}
