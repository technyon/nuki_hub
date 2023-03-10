#include "espMqttClientW5500.h"

espMqttClientW5500::espMqttClientW5500()
: MqttClientSetup(espMqttClientTypes::UseInternalTask::NO),
  _client()
{
    _transport = &_client;
}

void espMqttClientW5500::update()
{
    loop();
}
