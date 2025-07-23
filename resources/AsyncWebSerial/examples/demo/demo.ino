#include <Arduino.h>
#include <AsyncWebSerial.h>
#include <ESPAsyncWebServer.h>

AsyncWebSerial webSerial;
AsyncWebServer server(80);

void setup()
{
    webSerial.begin(&server);
    server.begin();
}

void loop()
{
    webSerial.loop();
    delay(10);
}