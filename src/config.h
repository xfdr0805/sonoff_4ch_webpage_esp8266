#ifndef __CONFIG__H__
#define __CONFIG__H__
#define RELAY1 12
#define RELAY2 5
#define RELAY3 4
#define RELAY4 15
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
String topic_barcode = "/barcode";
String topic_info = "/info";
String topic_leave = "/leave";
int8_t timeZone = 8;
int8_t minutesTimeZone = 0;
#endif