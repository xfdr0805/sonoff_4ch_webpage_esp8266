# sonoff_4ch_webpage_esp8266
## 注意Time这个库，需要把Time.h删除 把文件里的DateStrings.cpp和Time.cpp里的头文件修改为TimeLib.h,否则会和ESP Async WebServer 冲突
1. 程序支持4路继电器设置
2. 带web服务器，可以通过网页控制
3. 程序支持websocket 
4. 程序支持tcp
5. 程序 支持MQTT协议
6. 程序支持NTP对时，默认ntp1.aliyun.com