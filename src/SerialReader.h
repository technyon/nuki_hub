#pragma once

#include "Arduino.h"
#include "ImportExport.h"
#include "NukiNetwork.h"
#include <ArduinoJson.h>

class SerialReader
{
public:
    explicit SerialReader(ImportExport* importExport, NukiNetwork* network);

    void update();

private:
    String config = "";
    JsonDocument json;
    bool _receivingConfig = false;
    int64_t _lastCommandTs = 0;

    ImportExport* _importExport = nullptr;
    NukiNetwork* _network = nullptr;
};