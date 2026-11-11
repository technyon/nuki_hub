#include "MqttLogger.h"
#include "Arduino.h"
#include "esp_heap_caps.h"

MqttLogger::MqttLogger(MqttLoggerMode mode)
{
    this->setMode(mode);
    this->setBufferSize(MQTT_MAX_PACKET_SIZE);
}

MqttLogger::MqttLogger(MqttClient& client, const char* topic, MqttLoggerMode mode)
{
    this->setClient(client);
    this->setTopic(topic);
    this->setMode(mode);
    this->setBufferSize(MQTT_MAX_PACKET_SIZE);
}

MqttLogger::~MqttLogger()
{
}

void MqttLogger::setClient(MqttClient& client)
{
    this->client = &client;
}

void MqttLogger::setTopic(const char* topic)
{
    this->topic = topic;
}

void MqttLogger::setMode(MqttLoggerMode mode)
{
    this->mode = mode;
}

uint16_t MqttLogger::getBufferSize()
{
    return this->bufferSize;
}

// allocate or reallocate local buffer, reset end to start of buffer
boolean MqttLogger::setBufferSize(uint16_t size)
{
    if (size == 0)
    {
        return false;
    }
    if (this->bufferSize == 0)
    {
        this->buffer = (uint8_t *)malloc(size);
        this->bufferEnd = this->buffer;
    }
    else
    {
        uint8_t *newBuffer = (uint8_t *)realloc(this->buffer, size);
        if (newBuffer != NULL)
        {
            this->buffer = newBuffer;
            this->bufferEnd = this->buffer;
        }
        else
        {
            return false;
        }
    }
    this->bufferSize = size;
    return (this->buffer != NULL);
}

// send & reset current buffer
void MqttLogger::sendBuffer()
{
    if (this->bufferCnt > 0)
    {
        bool doSerial = this->mode==MqttLoggerMode::SerialOnly || this->mode==MqttLoggerMode::MqttAndSerial || this->mode==MqttLoggerMode::MqttAndSerialAndWeb || this->mode==MqttLoggerMode::SerialAndWeb;
        bool doWebSerial = this->mode==MqttLoggerMode::MqttAndSerialAndWeb || this->mode==MqttLoggerMode::SerialAndWeb;
        bool doMqtt = this->mode!=MqttLoggerMode::SerialOnly && this->mode!=MqttLoggerMode::SerialAndWeb;
        if (doMqtt && this->backlog != nullptr)
        {
            this->storeBacklog();
        }
        if (doMqtt && this->client != NULL && this->client->connected())
        {
            this->client->publish(topic, 0, true, this->buffer, this->bufferCnt);
        }
        else if (this->mode == MqttLoggerMode::MqttAndSerialFallback)
        {
            doSerial = true;
        }
        if (doSerial && coredumpPrinted)
        {
            Serial.write(this->buffer, this->bufferCnt);
            Serial.println();
        }
        if (doWebSerial && wsMessages != nullptr && webSerialClientCount > 0)
        {
            // Do NOT send from here: sendBuffer() runs in whatever task is logging
            // (BLE, WiFi, httpd, ...), while sendAll() walks the websocket client
            // list that the httpd task modifies (and frees entries of) on
            // connect/disconnect. Enqueue the line instead; it is broadcast from
            // the httpd task (WebCfgServer::updateWebSerial()). Non-blocking: the
            // line is dropped if the queue is full.
            WebsocketMessage wm;
            wm.socket = -1; // broadcast
            wm.len = this->bufferCnt;
            wm.buffer = (char *)malloc(this->bufferCnt);
            if (wm.buffer != NULL)
            {
                memcpy(wm.buffer, this->buffer, this->bufferCnt);
                if (xQueueSend(wsMessages, &wm, 0) != pdTRUE)
                {
                    free(wm.buffer);
                }
            }
        }
        this->bufferCnt=0;
    }
    this->bufferEnd=this->buffer;
}

// implement Print::write(uint8_t c): store into a buffer until \n or buffer full
size_t MqttLogger::write(uint8_t character)
{
    if (character == '\n') // when newline is printed we send the buffer
    {
        this->sendBuffer();
    }
    else
    {
        if (this->bufferCnt < this->bufferSize) // add char to end of buffer
        {
            *(this->bufferEnd++) = character;
            this->bufferCnt++;
        }
        else // buffer is full, first send&reset buffer and then add char to buffer
        {
            this->sendBuffer();
            *(this->bufferEnd++) = character;
            this->bufferCnt++;
        }
    }
    return 1;
}

size_t MqttLogger::write(const uint8_t *buffer, size_t size) {
  size_t n = 0;
  while (size--) {
    n += write(*buffer++);
  }
  return n;
}
boolean MqttLogger::enableBacklog(uint16_t lines, bool psram)
{
    if (this->backlog != nullptr || lines == 0)
    {
        return false;
    }
    this->backlog = (char*)heap_caps_malloc((size_t)lines * MQTT_LOGGER_BACKLOG_LINE_LEN, psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_8BIT);
    if (this->backlog == nullptr)
    {
        return false;
    }
    this->backlogLines = lines;
    return true;
}

// Called from whatever task is logging: stamp the line with the uptime and keep
// it in the ring, overwriting the oldest line when full. Every line is kept, not
// only those written while disconnected, because a line published into a stalled
// TCP connection is accepted by the client but never reaches the broker.
void MqttLogger::storeBacklog()
{
    char line[MQTT_LOGGER_BACKLOG_LINE_LEN];
    int len = snprintf(line, sizeof(line), "[+%lu ms] ", (unsigned long)millis());
    if (len < 0 || len >= (int)sizeof(line))
    {
        return;
    }
    size_t copy = this->bufferCnt;
    if (copy > sizeof(line) - 1 - len)
    {
        copy = sizeof(line) - 1 - len;
    }
    memcpy(line + len, this->buffer, copy);
    line[len + copy] = '\0';

    portENTER_CRITICAL(&this->backlogMux);
    uint16_t slot = (this->backlogHead + this->backlogCount) % this->backlogLines;
    if (this->backlogCount == this->backlogLines)
    {
        this->backlogHead = (this->backlogHead + 1) % this->backlogLines;
        this->backlogDropped = true;
    }
    else
    {
        this->backlogCount++;
    }
    memcpy(this->backlog + (size_t)slot * MQTT_LOGGER_BACKLOG_LINE_LEN, line, len + copy + 1);
    portEXIT_CRITICAL(&this->backlogMux);
}

// Called from the MQTT disconnect callback; the network task may reconnect
// within one loop and never observe the disconnected state itself.
void MqttLogger::noteDisconnect()
{
    this->backlogWasDisconnected = true;
}

// Called from the network task once the client is connected again.
void MqttLogger::flushBacklog()
{
    if (this->backlog == nullptr || this->client == NULL)
    {
        return;
    }
    if (!this->client->connected())
    {
        this->backlogWasDisconnected = true;
        return;
    }
    if (!this->backlogWasDisconnected || this->backlogCount == 0)
    {
        return;
    }
    this->backlogWasDisconnected = false;
    char line[MQTT_LOGGER_BACKLOG_LINE_LEN];
    snprintf(line, sizeof(line), "Replaying the last %u log lines after reconnect%s", this->backlogCount, this->backlogDropped ? " (older lines were dropped)" : "");
    this->client->publish(topic, 0, true, line);
    while (true)
    {
        portENTER_CRITICAL(&this->backlogMux);
        if (this->backlogCount == 0)
        {
            this->backlogDropped = false;
            portEXIT_CRITICAL(&this->backlogMux);
            break;
        }
        memcpy(line, this->backlog + (size_t)this->backlogHead * MQTT_LOGGER_BACKLOG_LINE_LEN, MQTT_LOGGER_BACKLOG_LINE_LEN);
        this->backlogHead = (this->backlogHead + 1) % this->backlogLines;
        this->backlogCount--;
        portEXIT_CRITICAL(&this->backlogMux);
        this->client->publish(topic, 0, true, line);
    }
    this->client->publish(topic, 0, true, "End of replayed log");
}
