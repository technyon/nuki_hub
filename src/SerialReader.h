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
    bool receivingConfig = false;

    ImportExport* _importExport = nullptr;
    NukiNetwork* _network = nullptr;
};