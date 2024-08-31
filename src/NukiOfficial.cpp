#include <cstring>
#include "NukiOfficial.h"

void NukiOfficial::buildMqttPath(const char *path, char *outPath)
{
    int offset = 0;
    char inPath[181] = {0};

    memcpy(inPath, offMqttPath, sizeof(offMqttPath));

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
