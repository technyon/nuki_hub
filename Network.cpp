#include "Network.h"
#include "WiFi.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"

Network::Network()
: _mqttClient(_wifiClient)
{

}

void Network::initialize()
{
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        return;
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
    }

    _mqttClient.setServer("192.168.0.100", 1883);
    _mqttClient.publish("nuki/test", "OK");
}


bool Network::reconnect()
{
    while (!_mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (_mqttClient.connect("arduinoClient")) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            _mqttClient.publish("outTopic","hello world");
            // ... and resubscribe
            _mqttClient.subscribe("inTopic");
        } else {
            Serial.print("failed, rc=");
            Serial.print(_mqttClient.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}


void Network::update()
{
    if(!WiFi.isConnected())
    {
        Serial.println(F("WiFi not connected"));
        vTaskDelay( 1000 / portTICK_PERIOD_MS);
    }

    if(!_mqttClient.connected())
    {
        bool success = reconnect();
        if(!success)
        {
            return;
        }
    }

    unsigned long ts = millis();
    if(_publishTs < ts)
    {
        _publishTs = ts + 1000;

        ++_count;

        char cstr[16];
        itoa(_count, cstr, 10);

        _mqttClient.publish("nuki/counter", cstr);
    }

    _mqttClient.loop();

    vTaskDelay( 100 / portTICK_PERIOD_MS);
}
