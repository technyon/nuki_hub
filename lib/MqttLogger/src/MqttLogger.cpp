#include "MqttLogger.h"
#include "Arduino.h"

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
        if (this->mode!=MqttLoggerMode::SerialOnly && this->mode!=MqttLoggerMode::SerialAndWeb && this->client != NULL && this->client->connected()) 
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
        if (doWebSerial)
        {
            //WebSerial.write(this->buffer, this->bufferCnt);
            //WebSerial.println();
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