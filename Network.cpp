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
        Serial.println("Attempting MQTT connection");
        // Attempt to connect
        if (_mqttClient.connect("nukiHub")) {
            Serial.println("MQTT connected");

            // ... and resubscribe
            _mqttClient.subscribe(mqtt_topc_lockstate_action);
        } else {
            Serial.print("MQTT connect failed, rc=");
            Serial.println(_mqttClient.state());
            vTaskDelay( 5000 / portTICK_PERIOD_MS);
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

    if(strcmp(topic, mqtt_topc_lockstate_action) == 0)
    {
        if(strcmp(value, "") == 0) return;

        Serial.print("lockstate setpoint received: ");
        Serial.println(value);
        if(_lockActionReceivedCallback != NULL)
        {
            _lockActionReceivedCallback(value);
        }
        _mqttClient.publish(mqtt_topc_lockstate_action, "");
    }
}

void Network::publishKeyTurnerState(const char* state)
{
    _mqttClient.publish(mqtt_topc_lockstate, state);
}

void Network::setLockActionReceived(void (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void Network::publishBatteryVoltage(const float &value)
{
    char str[30];
    dtostrf(value, 0, 2, str);
    _mqttClient.publish(mqtt_topc_voltage, str);
}
