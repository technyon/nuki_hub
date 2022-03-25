#include "Network.h"
#include "WiFi.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include "MqttTopics.h"

Network* nwInst;

Network::Network()
: _mqttClient(_wifiClient)
{
    nwInst = this;
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

    bool res = wm.autoConnect(); // password protected ap

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
    _mqttClient.setCallback(Network::onMqttDataReceivedCallback);
    _mqttClient.publish("nuki/test", "OK");
}


bool Network::reconnect()
{
    while (!_mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (_mqttClient.connect("arduinoClient")) {
            Serial.println("connected");

            // ... and resubscribe
            _mqttClient.subscribe("nuki/cmd");
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

//    unsigned long ts = millis();
//    if(_publishTs < ts)
//    {
//        _publishTs = ts + 1000;
//
//        ++_count;
//
//        char cstr[16];
//        itoa(_count, cstr, 10);
//
//        _mqttClient.publish("nuki/counter", cstr);
//    }

    _mqttClient.loop();

    vTaskDelay( 100 / portTICK_PERIOD_MS);
}

void Network::onMqttDataReceivedCallback(char *topic, byte *payload, unsigned int length)
{
    nwInst->onMqttDataReceived(topic, payload, length);
}

void Network::onMqttDataReceived(char *&topic, byte *&payload, unsigned int &length)
{
    char value[50];
    size_t l = min(length, sizeof(value)-1);

    for(int i=0; i<l; i++)
    {
        value[i] = payload[i];
    }

    value[l] = 0;

    if(strcmp(topic, "nuki/cmd") == 0)
    {
        Serial.println(value);
    }
}

void Network::publishKeyTurnerState(const char* state)
{
    _mqttClient.publish(mqtt_topc_lockstate, state);
}
