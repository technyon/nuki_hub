#include "PresenceDetection.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "CharBuffer.h"
#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"
#include "NukiUtils.h"

PresenceDetection::PresenceDetection(Preferences* preferences, BleScanner::Scanner *bleScanner, char* buffer, size_t bufferSize)
: _preferences(preferences),
  _bleScanner(bleScanner),
  _csv(buffer),
  _bufferSize(bufferSize)
{
    _timeout = _preferences->getInt(preference_presence_detection_timeout) * 1000;
    if(_timeout == 0)
    {
        _timeout = 60000;
        _preferences->putInt(preference_presence_detection_timeout, 60);
    }

    Log->print(F("Presence detection timeout (ms): "));
    Log->println(_timeout);
}

PresenceDetection::~PresenceDetection()
{
    _bleScanner->unsubscribe(this);
    _bleScanner = nullptr;

    delete _csv;
    _csv = nullptr;
}

void PresenceDetection::initialize()
{
    _bleScanner->subscribe(this);
}

char* PresenceDetection::generateCsv()
{
    if(!enabled()) return nullptr;
    memset(_csv, 0, _bufferSize);

    _csvIndex = 0;
    long ts = millis();
    {
        std::lock_guard<std::mutex> lock(mtx);

        for (auto it: _devices)
        {
            if (ts - _timeout < it.second->timestamp)
            {
                buildCsv(it.second);
            }

            // Prevent csv buffer overflow
            if (_csvIndex > _bufferSize - (sizeof(it.second->name) + sizeof(it.second->address) + 10))
            {
                break;
            }
        }
    }

    if(_csvIndex == 0)
    {
        strcpy(_csv, ";;");
        return _csv;
    }

    _csv[_csvIndex-1] = 0x00;
    return _csv;
}


void PresenceDetection::buildCsv(const std::shared_ptr<PdDevice>& device)
{
    for(int i = 0; i < 17; i++)
    {
        _csv[_csvIndex] = device->address[i];
        ++_csvIndex;
    }
    _csv[_csvIndex] = ';';
    ++_csvIndex;

    int i=0;
    while(device->name[i] != 0x00 && i < sizeof(device->name))
    {
        _csv[_csvIndex] = device->name[i];
        ++_csvIndex;
        ++i;
    }

    _csv[_csvIndex] = ';';
    ++_csvIndex;

    if(device->hasRssi)
    {
        char rssiStr[20] = {0};
        itoa(device->rssi, rssiStr, 10);

        int i=0;
        while(rssiStr[i] != 0x00 && i < 20)
        {
            _csv[_csvIndex] = rssiStr[i];
            ++_csvIndex;
            ++i;
        }
    }

    _csv[_csvIndex] = '\n';
    _csvIndex++;
}

void PresenceDetection::onResult(NimBLEAdvertisedDevice *device)
{
    std::string addressStr = device->getAddress().toString();
    char addrArrComp[13] = {0};

//    Log->println(addressStr.c_str());

    addrArrComp[0] = addressStr.at(0);
    addrArrComp[1] = addressStr.at(1);
    addrArrComp[2] = addressStr.at(3);
    addrArrComp[3] = addressStr.at(4);
    addrArrComp[4] = addressStr.at(6);
    addrArrComp[5] = addressStr.at(7);
    addrArrComp[6] = addressStr.at(9);
    addrArrComp[7] = addressStr.at(10);
    addrArrComp[8] = addressStr.at(12);
    addrArrComp[9] = addressStr.at(13);
    addrArrComp[10] = addressStr.at(15);
    addrArrComp[11] = addressStr.at(16);

    long long addr = strtoll(addrArrComp, nullptr, 16);

    bool found;
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = _devices.find(addr);
        found = (it != _devices.end());

        if(found)
        {
            it->second->timestamp = millis();
            if(device->haveRSSI())
            {
                it->second->hasRssi = true;
                it->second->rssi = device->getRSSI();
            }
        }
    }

    if(!found)
    {
        std::shared_ptr<PdDevice> pdDevice = std::make_shared<PdDevice>();

        int i=0;
        size_t len = addressStr.length();
        while(i < len)
        {
            pdDevice->address[i] = addressStr.at(i);
            ++i;
        }

        if(device->haveRSSI())
        {
            pdDevice->hasRssi = true;
            pdDevice->rssi = device->getRSSI();
        }

        std::string nameStr = "-";
        if(device->haveName())
        {
            std::string nameStr = device->getName();

            i=0;
            len = nameStr.length();
            while(i < len && i < sizeof(pdDevice->name)-1)
            {
                pdDevice->name[i] = nameStr.at(i);
                ++i;
            }

            pdDevice->timestamp = millis();

            {
                std::lock_guard<std::mutex> lock(mtx);
                _devices[addr] = pdDevice;
            }
        }
        else if (device->haveManufacturerData())
        {
            std::string strManufacturerData = device->getManufacturerData();

            uint8_t cManufacturerData[100];
            strManufacturerData.copy((char *)cManufacturerData,  std::min(strManufacturerData.length(), sizeof(cManufacturerData)), 0);

            if (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C && cManufacturerData[1] == 0x00)
            {
                BLEBeacon oBeacon = BLEBeacon();
                oBeacon.setData(strManufacturerData);

//                if(ENDIAN_CHANGE_U16(oBeacon.getMinor()) == 40004)
//                {
                    pdDevice->timestamp = millis();
                    strcpy(pdDevice->name, oBeacon.getProximityUUID().toString().c_str());
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        _devices[addr] = pdDevice;
                    }
//                }
            }
        }
    }

//    if(device->haveName())
//    {
//        Log->print(" | ");
//        Log->print(device->getName().c_str());
//        if(device->haveRSSI())
//        {
//            Log->print(" | ");
//            Log->print(device->getRSSI());
//        }
//    }
//    Log->println();

}

bool PresenceDetection::enabled()
{
    return _timeout > 0;
}
