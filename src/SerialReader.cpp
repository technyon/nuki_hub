#include "SerialReader.h"
#include "RestartReason.h"
#include "EspMillis.h"

SerialReader::SerialReader(ImportExport *importExport, NukiNetwork* network)
: _importExport(importExport),
  _network(network)
{
}


void SerialReader::update()
{
    if(Serial.available())
    {
        String line = Serial.readStringUntil('\n');
//        Serial.println(line);

        if(!receivingConfig && line == "reset")
        {
            restartEsp(RestartReason::RequestedViaSerial);
        }

        if(!receivingConfig && line == "uptime")
        {
            Serial.print("Uptime (seconds): ");
            Serial.println(espMillis() / 1000);
        }

        if(!receivingConfig && line == "ip")
        {
            Serial.print("IP address: ");
            Serial.println(_network->localIP());
        }

        if(!receivingConfig && line == "printcfg")
        {
            Serial.println();
            serializeJsonPretty(json, Serial);
            Serial.println();
        }

        if(!receivingConfig && line == "savecfg")
        {
            _importExport->importJson(json);
            Serial.println("Configuration saved");
        }

        if(line == "-- NUKI HUB CONFIG END --")
        {
            Serial.println("Receive config end");
            receivingConfig = false;
            json.clear();
            DeserializationError error = deserializeJson(json, config);
        }

        if(receivingConfig)
        {
            config = config + line;
        }

        if(line == "-- NUKI HUB CONFIG START --")
        {
            Serial.println("Receive config start");
            config = "";
            receivingConfig = true;
        }
    }
}

