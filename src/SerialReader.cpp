#include "SerialReader.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "hal/wdt_hal.h"

SerialReader::SerialReader(ImportExport *importExport, NukiNetwork* network)
: _importExport(importExport),
  _network(network)
{
}


void SerialReader::update()
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    
    if(Serial.available())
    {
        String line = Serial.readStringUntil('\n');
//        Serial.println(line);

        int64_t ts = espMillis();

        if(ts - _lastCommandTs > 3000)
        {
            _receivingConfig = false;
        }
        _lastCommandTs = ts;


        if(line == "reset")
        {
            restartEsp(RestartReason::RequestedViaSerial);
        }

        if(line == "uptime")
        {
            Serial.print("Uptime (seconds): ");
            Serial.println(espMillis() / 1000);
        }

        if(line == "printerror")
        {
            Serial.println(_deserializationError);
        }

        if(line == "printcfgstr")
        {
            Serial.println();
            Serial.println(config);
            Serial.println();
        }

        if(line == "printcfg")
        {
            Serial.println();
            serializeJsonPretty(json, Serial);
            Serial.println();
        }

        if(line == "savecfg")
        {
            _importExport->importJson(json);
            Serial.println("Configuration saved");
        }

        if(line == "-- NUKI HUB CONFIG END --")
        {
            Serial.println("Receive config end");
            _receivingConfig = false;
            json.clear();
            DeserializationError error = deserializeJson(json, config);
            _deserializationError = (int)error.code();
        }

        if(_receivingConfig)
        {
            config = config + line;
        }

        if(line == "-- NUKI HUB CONFIG START --")
        {
            Serial.println("Receive config start");
            config = "";
            _receivingConfig = true;
        }
    }
}

