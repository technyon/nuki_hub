#pragma once

#include "Arduino.h"
#include "ImportExport.h"
#include <ArduinoJson.h>

class SerialReader
{
public:
    explicit SerialReader(ImportExport* importExport);

    void update();

private:
    String config = "";
    JsonDocument json;
    bool receivingConfig = false;

    ImportExport* _importExport = nullptr;
};