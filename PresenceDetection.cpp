#include "PresenceDetection.h"

PresenceDetection::PresenceDetection(BleScanner *bleScanner, Network* network)
: _bleScanner(bleScanner),
  _network(network)
{
    _csv = new char[presence_detection_buffer_size];
}

PresenceDetection::~PresenceDetection()
{
    _bleScanner->unsubscribe(this);
    _bleScanner = nullptr;

    _network = nullptr;

    delete _csv;
    _csv = nullptr;
}

void PresenceDetection::initialize()
{
    _bleScanner->subscribe(this);
}

void PresenceDetection::update()
{
    vTaskDelay( 5000 / portTICK_PERIOD_MS);

    if(_devices.size() == 0) return;

    memset(_csv, 0, presence_detection_buffer_size);
    _csvIndex = 0;
    long ts = millis();
    for(auto it : _devices)
    {
        if(ts - 20000 < it.second.timestamp)
        {
            buildCsv(it.second);
        }

        // Prevent csv buffer overflow
        if(_csvIndex > presence_detection_buffer_size - (sizeof(it.second.name) + sizeof(it.second.address) + 3))
        {
            break;
        }
    }

    _network->publishPresenceDetection(_csv);
}


void PresenceDetection::buildCsv(const PdDevice &device)
{
    for(int i = 0; i < 17; i++)
    {
        _csv[_csvIndex] = device.address[i];
        ++_csvIndex;
    }
    _csv[_csvIndex] = ';';
    ++_csvIndex;

    int i=0;
    while(device.name[i] != 0x00 && i < 30)
    {
        _csv[_csvIndex] = device.name[i];
        ++_csvIndex;
        ++i;
    }
    _csv[_csvIndex] = '\n';
    _csvIndex++;
}


void PresenceDetection::onResult(NimBLEAdvertisedDevice *device)
{
    std::string addressStr = device->getAddress().toString();
    char addrArrComp[13] = {0};

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

    auto it = _devices.find(addr);
    if(it == _devices.end())
    {
        PdDevice pdDevice;

        int i=0;
        size_t len = addressStr.length();
        while(i < len)
        {
            pdDevice.address[i] = addressStr.at(i);
            ++i;
        }

        if(device->haveName())
        {
            std::string nameStr = device->getName();

            int i=0;
            size_t len = nameStr.length();
            while(i < len && i < sizeof(pdDevice.name)-1)
            {
                pdDevice.name[i] = nameStr.at(i);
                ++i;
            }

            pdDevice.timestamp = millis();

            _devices[addr] = pdDevice;
        }
    }
    else
    {
        it->second.timestamp = millis();
    }

//    if(device->haveName())
//    {
//        Serial.print(" | ");
//        Serial.print(device->getName().c_str());
//    }
//    Serial.println();

}