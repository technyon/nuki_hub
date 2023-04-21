#include "PresenceDetection.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "CharBuffer.h"

PresenceDetection::PresenceDetection(Preferences* preferences, BleScanner::Scanner *bleScanner, Network* network, char* buffer, size_t bufferSize)
: _preferences(preferences),
  _bleScanner(bleScanner),
  _network(network),
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
    delay(3000);

    if(_timeout < 0) return;
    memset(_csv, 0, _bufferSize);

    if(_devices.size() == 0)
    {
        strcpy(_csv, ";;");
        _network->publishPresenceDetection(_csv);
        return;
    }

    _csvIndex = 0;
    long ts = millis();
    for(auto it : _devices)
    {
        if(ts - _timeout < it.second.timestamp)
        {
            buildCsv(it.second);
        }
    }

    _csv[_csvIndex-1] = 0x00;

//    Log->print("Devices found: ");
//    Log->println(_devices.size());
    _network->publishPresenceDetection(_csv);
}


int appendInt(char * dst, int value, char prefix = 0){
    char* ptr = dst;
    if(prefix!=0){
        *ptr = prefix;        
        ptr++;
    }
    itoa(value, ptr, 10);    
    return strlen(dst);    
}

int appendFloat(char * dst, float value, unsigned char prec=1, char prefix = 0){
    char* ptr = dst;
    if(prefix!=0){
        *ptr = prefix;        
        ptr++;
    }
    dtostrf(value,2,prec,ptr);
    return strlen(dst);
}

int appendString(char * dst, const char* src, char prefix = 0){
    char* ptr = dst;
    if(prefix!=0){
        *ptr = prefix;        
        ptr++;
    }
    strcpy(ptr,src);    
    return strlen(dst);
}

void PresenceDetection::buildCsv(const PdDevice &device)
{    
    int maxexpectedsize = 27 + strlen(device.name) + device.hasEnvironmentalSensingService ?(23) :0;

    // check space
    if(_bufferSize-_csvIndex <= maxexpectedsize){
        return;
    }

    // add device address
    memcpy(_csv+_csvIndex,device.address,17);
    _csvIndex+=17;

    // add device name
    _csvIndex+=appendString(_csv+_csvIndex,device.name,';'); 

    // add rssi
    _csvIndex+=appendInt(_csv+_csvIndex,device.hasRssi?device.rssi:0,';');  


    // add optional environmental informations (temperature;humidity;voltage;batt_level)
    if(device.hasEnvironmentalSensingService){         
        _csvIndex+=appendFloat(_csv+_csvIndex,device.temperature/100.0,2,';');
        _csvIndex+=appendFloat(_csv+_csvIndex,device.humidity/100.0,2,';');
        _csvIndex+=appendFloat(_csv+_csvIndex,device.voltage/1000.0,3,';');
        _csvIndex+=appendInt(_csv+_csvIndex,device.batt_level,';');               
    }
    _csv[_csvIndex] = '\n';
    _csvIndex++;          
}

int16_t readInt16(const char * data, int offset){
    int msb = data[offset+1];
    int lsb = data[offset];
    return (msb << 8) | lsb;
}

void updateDevice(NimBLEAdvertisedDevice *device, PdDevice *pdDevice){
     if (BLEUUID((uint16_t)0x181a).equals(device->getServiceDataUUID())){
            const char * data = device->getServiceData().c_str();
            pdDevice->temperature=readInt16(data,6);  // temperature x 100°C -> temperature/100.0
            pdDevice->humidity=readInt16(data,8);  // humidity x 100%   -> humidity/100.0
            pdDevice->voltage=readInt16(data,10); // battery voltage [mv] -> voltage/1000.0
            pdDevice->batt_level=data[12];
            pdDevice->hasEnvironmentalSensingService = true;
     }else{
        pdDevice->hasEnvironmentalSensingService = false;
     }
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

    auto it = _devices.find(addr);
    if(it == _devices.end())
    {
        

        PdDevice pdDevice;
        

        int i=0
        ;
        size_t len = addressStr.length();
        while(i < len)
        {
            pdDevice.address[i] = addressStr.at(i);
            ++i;
        }

        if(device->haveRSSI())
        {
            pdDevice.hasRssi = true;
            pdDevice.rssi = device->getRSSI();
        }

        std::string nameStr = "-";
        if(device->haveName())
        {
            std::string nameStr = device->getName();

            i=0;
            len = nameStr.length();
            while(i < len && i < sizeof(pdDevice.name)-1)
            {
                pdDevice.name[i] = nameStr.at(i);
                ++i;
            
            }

            pdDevice.timestamp = millis()
            ;
            

            updateDevice(device,&pdDevice);
            _devices[addr] = pdDevice;
        }
    }
    else
    {
        it->second.timestamp = millis();
        if(device->haveRSSI())
        
        {
            it->second.hasRssi = true;
            it->second.rssi = device->getRSSI();
            updateDevice(device,&it->second)
            ;
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