#include "Network.h"
#include "WiFi.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"

Network* nwInst;

Network::Network(Preferences* preferences)
: _mqttClient(_wifiClient),
  _preferences(preferences)
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

    const char* brokerAddr = _preferences->getString(preference_mqtt_broker).c_str();
    strcpy(_mqttBrokerAddr, brokerAddr);

    int port = _preferences->getInt(preference_mqtt_broker_port);
    if(port == 0)
    {
        port = 1883;
        _preferences->putInt(preference_mqtt_broker_port, port);
    }

    Serial.print(F("MQTT Broker: "));
    Serial.print(_mqttBrokerAddr);
    Serial.print(F(":"));
    Serial.println(port);
    _mqttClient.setServer(_mqttBrokerAddr, port);
    _mqttClient.setCallback(Network::onMqttDataReceivedCallback);
}


bool Network::reconnect()
{
    while (!_mqttClient.connected() && millis() > _nextReconnect)
    {
        Serial.println("Attempting MQTT connection");
        // Attempt to connect
        if (_mqttClient.connect("nukiHub")) {
            Serial.println(F("MQTT connected"));

            // ... and resubscribe
            _mqttClient.subscribe(mqtt_topic_lockstate_action);
        }
        else
        {
            Serial.print(F("MQTT connect failed, rc="));
            Serial.println(_mqttClient.state());
            _nextReconnect = millis() + 5000;
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

    if(strcmp(topic, mqtt_topic_lockstate_action) == 0)
    {
        if(strcmp(value, "") == 0) return;

        Serial.print(F("Lockstate action received: "));
        Serial.println(value);
        if(_lockActionReceivedCallback != NULL)
        {
            _lockActionReceivedCallback(value);
        }
        _mqttClient.publish(mqtt_topic_lockstate_action, "");
    }
}

void Network::publishKeyTurnerState(const char* state, const char* trigger, const char* completionStatus)
{
    _mqttClient.publish(mqtt_topic_lockstate_state, state);
    _mqttClient.publish(mqtt_topic_lockstate_trigger, trigger);
    _mqttClient.publish(mqtt_topic_lockstate_completionStatus, completionStatus);
}

void Network::setLockActionReceived(void (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void Network::publishBatteryReport(const BatteryReport& batteryReport)
{
    publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0);
    publishInt(mqtt_topic_battery_drain, batteryReport.batteryDrain); // milliwatt seconds
    publishFloat(mqtt_topic_battery_max_turn_current, (float)batteryReport.maxTurnCurrent / 1000.0);
}

void Network::publishFloat(const char* topic, const float value, const uint8_t precision)
{
    char str[30];
    dtostrf(value, 0, precision, str);
    _mqttClient.publish(topic, str);
}

void Network::publishInt(const char *topic, const int value)
{
    char str[30];
    itoa(value, str, 10);
    _mqttClient.publish(topic, str);
}
