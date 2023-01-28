#include "espMqttClientEthernet.h"

espMqttClientEthernet::espMqttClientEthernet(uint8_t priority, uint8_t core)
: MqttClientSetup(true, priority, core),
  _client()
{
    _transport = &_client;
}
