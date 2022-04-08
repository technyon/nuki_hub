#include "SpiffsCookie.h"
#include "FS.h"
#include "SPIFFS.h"

SpiffsCookie::SpiffsCookie()
{
    if(!SPIFFS.begin(true))
    {
        Serial.println(F("SPIFFS Mount Failed"));
    }
}

void SpiffsCookie::set()
{
    File file = SPIFFS.open("/cookie", FILE_WRITE);
    if(!file)
    {
        Serial.println(F("- failed to open file for writing"));
        return;
    }

    if(file.write('#'))
    {
        Serial.println(F("- file written"));
    } else {
        Serial.println(F("- write failed"));
    }
    file.close();
}

void SpiffsCookie::clear()
{
    if(!SPIFFS.remove("/cookie"))
    {
        Serial.println(F("Failed to remove file"));
    }

}

const bool SpiffsCookie::isSet()
{
    return SPIFFS.exists("/cookie");
}
