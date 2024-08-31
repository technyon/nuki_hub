#include <cstring>
#include "NukiOfficial.h"
#include <stdlib.h>
#include <ctype.h>

void NukiOfficial::setUid(const uint32_t& uid)
{
    char uidString[20];
    itoa(uid, uidString, 16);

    for(char* c=uidString; *c=toupper(*c); ++c);

    strcpy(mqttPath, "nuki/");
    strcat(mqttPath, uidString);
}

const char *NukiOfficial::GetMqttPath()
{
    return mqttPath;
}

void NukiOfficial::buildMqttPath(const char *path, char *outPath)
{
    int offset = 0;
    char inPath[181] = {0};

    memcpy(inPath, mqttPath, sizeof(mqttPath));

    for(const char& c : inPath)
    {
        if(c == 0x00)
        {
            break;
        }
        outPath[offset] = c;
        ++offset;
    }
    int i=0;
    while(outPath[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }
    outPath[i+1] = 0x00;
}

bool NukiOfficial::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

