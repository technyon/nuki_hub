#include <WiFi.h>
#include <PubSubClient.h>
#include <MqttLogger.h>
#include "wifi_secrets.h"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqtt_server =  "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

// default mode is MqttLoggerMode::MqttAndSerialFallback
MqttLogger mqttLogger(client,"mqttlogger/log");
// other available modes:
// MqttLogger mqttLogger(client,"mqttlogger/log",MqttLoggerMode::MqttAndSerial);
// MqttLogger mqttLogger(client,"mqttlogger/log",MqttLoggerMode::MqttOnly);
// MqttLogger mqttLogger(client,"mqttlogger/log",MqttLoggerMode::SerialOnly);

// connect to wifi network
void wifiConnect()
{
    mqttLogger.print("Connecting to WiFi: ");
    mqttLogger.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        mqttLogger.print(".");
    }
    mqttLogger.print("WiFi connected: ");
    mqttLogger.println(WiFi.localIP());
}

// establish mqtt client connection
void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        mqttLogger.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP32Logger"))
        {
            // as we have a connection here, this will be the first message published to the mqtt server
            mqttLogger.println("connected.");  

        }
        else
        {
            mqttLogger.print("failed, rc=");
            mqttLogger.print(client.state());
            mqttLogger.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// arduino setup
void setup()
{
    Serial.begin(115200);
    // MqttLogger uses mqtt when available and Serial as a fallback. Before any connection is established, 
    // MqttLogger works just like Serial 
    mqttLogger.println("Starting setup..");
 
    // connect to wifi
    wifiConnect();
 
    // mqtt client
    client.setServer(mqtt_server, 1883);
    //client.setCallback(mqttIncomingCallback);
    
} 

void loop()
{
    // here the mqtt connection is established
    if (!client.connected())
    {
        reconnect();
    }
    // with a connection, subsequent print() publish on the mqtt broker, but in a buffered fashion
    
    mqttLogger.print(millis()/1000);
    delay(1000);
    mqttLogger.print(" seconds");
    delay(1000);
    mqttLogger.print(" online");
    delay(1000);
    // this finally flushes the buffer and published on the mqtt broker
    mqttLogger.println();
}
