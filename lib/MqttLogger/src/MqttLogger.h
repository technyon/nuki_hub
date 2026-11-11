/*
  MqttLogger - offer print() interface like Serial but by publishing to a given mqtt topic.
               Uses Serial as a fallback when no mqtt connection is available.

  Claus Denk
  https://androbi.com
*/

#ifndef MqttLogger_h
#define MqttLogger_h

#include <Arduino.h>
#include <Print.h>
#include <espMqttClient.h>
#include "PsychicWebSocket.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>

#define MQTT_MAX_PACKET_SIZE 1024
#define MQTT_LOGGER_BACKLOG_LINE_LEN 160

extern PsychicWebSocketHandler* websocketHandler;
extern bool coredumpPrinted;

// A single line of log output handed from the logging path (any task) to the
// web server task via a queue. socket == -1 means "broadcast to all WebSerial
// clients"; a real socket number is used for point-to-point replies (ping/pong).
typedef struct
{
    int socket;
    char *buffer;
    size_t len;
} WebsocketMessage;

extern QueueHandle_t wsMessages;
// Number of connected WebSerial clients. Updated by the web server on
// connect/close and read (best-effort) from the logging path to skip work when
// nobody is listening.
extern std::atomic<uint8_t> webSerialClientCount;

enum MqttLoggerMode {
    MqttAndSerialFallback = 0,
    SerialOnly = 1,
    MqttOnly = 2,
    MqttAndSerial = 3,
    MqttAndSerialAndWeb = 4,
    SerialAndWeb = 5,
};

class MqttLogger : public Print
{
private:
    const char* topic;
    uint8_t* buffer;
    uint8_t* bufferEnd;
    uint16_t bufferCnt = 0;
    uint16_t bufferSize = 0;
    MqttClient* client;
    MqttLoggerMode mode;
    char* backlog = nullptr;
    uint16_t backlogLines = 0;
    uint16_t backlogHead = 0;
    uint16_t backlogCount = 0;
    bool backlogDropped = false;
    bool backlogWasDisconnected = false;
    portMUX_TYPE backlogMux = portMUX_INITIALIZER_UNLOCKED;
    void sendBuffer();
    void storeBacklog();

public:
    MqttLogger(MqttLoggerMode mode=MqttLoggerMode::MqttAndSerialFallback);
    MqttLogger(MqttClient& client, const char* topic, MqttLoggerMode mode=MqttLoggerMode::MqttAndSerialFallback);
    ~MqttLogger();

    void setClient(MqttClient& client);
    void setTopic(const char* topic);
    void setMode(MqttLoggerMode mode);
    void setRetained(boolean retained);

    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);
    using Print::write;

    uint16_t getBufferSize();
    boolean setBufferSize(uint16_t size);

    // Keep the last lines logged while the MQTT client is disconnected and
    // publish them once it is connected again, so the log of an outage is not lost.
    boolean enableBacklog(uint16_t lines, bool psram);
    void noteDisconnect();
    void flushBacklog();
};

extern MqttLogger* mqttLogger;

#endif
