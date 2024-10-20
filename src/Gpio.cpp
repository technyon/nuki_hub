#include <esp32-hal.h>
#include "Gpio.h"
#include "Config.h"
#include "Arduino.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "networkDevices/LAN8720Definitions.h"
#include "networkDevices/DM9051Definitions.h"
#include "networkDevices/W5500Definitions.h"

Gpio* Gpio::_inst = nullptr;

Gpio::Gpio(Preferences* preferences)
    : _preferences(preferences)
{
    _inst = this;
    loadPinConfiguration();

    if(_preferences->getBool(preference_gpio_locking_enabled, false))
    {
        migrateObsoleteSetting();
    }

    _inst->init();
}


bool Gpio::isTriggered(const PinEntry& entry)
{
//    Log->println(" ------------ ");

    const int threshold = 3;

    uint8_t state = digitalRead(entry.pin);
    uint8_t lastState = (_triggerState[entry.pin] & 0x80) >> 7;

    uint8_t pinState = _triggerState[entry.pin] & 0x7f;
    pinState = pinState << 1 | state;
    _triggerState[entry.pin] = (pinState & 0x7f) | lastState << 7;
//    Log->print("Trigger state: ");
//    Log->println(_triggerState[entry.pin], 2);

    pinState = pinState & 0x07;
//    Log->print("Val: ");
//    Log->println(pinState);


    if(pinState != 0x00 && pinState != 0x07)
    {
        return false;
    }
//    Log->print("Last State: ");
//    Log->println(lastState);
//    Log->print("State: ");
//    Log->println(state);

    if(state != lastState)
    {
//        Log->print("State changed: ");
//        Log->println(state);
        _triggerState[entry.pin] = (pinState & 0x7f) | state << 7;
    }
    else
    {
        return false;
    }

    if(entry.role == PinRole::GeneralInputPullDown || entry.role == PinRole::GeneralInputPullUp)
    {
        return true;
    }

    return state == LOW;
}

void Gpio::onTimer()
{
    for(const auto& entry : _inst->_pinConfiguration)
    {
        switch(entry.role)
        {
        case PinRole::InputLock:
        case PinRole::InputUnlock:
        case PinRole::InputUnlatch:
        case PinRole::InputLockNgo:
        case PinRole::InputLockNgoUnlatch:
        case PinRole::InputElectricStrikeActuation:
        case PinRole::InputActivateRTO:
        case PinRole::InputActivateCM:
        case PinRole::InputDeactivateRtoCm:
        case PinRole::InputDeactivateRTO:
        case PinRole::InputDeactivateCM:
        case PinRole::GeneralInputPullDown:
        case PinRole::GeneralInputPullUp:
            if(isTriggered(entry))
            {
                _inst->notify(getGpioAction(entry.role), entry.pin);
            }
            break;
        case PinRole::OutputHighLocked:
        case PinRole::OutputHighUnlocked:
        case PinRole::OutputHighMotorBlocked:
        case PinRole::OutputHighRtoActive:
        case PinRole::OutputHighCmActive:
        case PinRole::OutputHighRtoOrCmActive:
        case PinRole::GeneralOutput:
        case PinRole::Ethernet:
        // ignore. This case should not occur since pins are configured as output
        default:
            break;
        }
    }
}

void Gpio::isrOnTimer()
{
    _inst->onTimer();
}

void Gpio::init()
{
    _inst->_triggerState.reserve(_inst->availablePins().size());
    for(int i=0; i<_inst->availablePins().size(); i++)
    {
        _inst->_triggerState.push_back(0);
    }

    bool hasInputPin = false;

    for(const auto& entry : _inst->_pinConfiguration)
    {
        const auto it = std::find(_inst->availablePins().begin(), _inst->availablePins().end(), entry.pin);

        if(it == _inst->availablePins().end())
        {
            continue;
        }

        switch(entry.role)
        {
        case PinRole::InputLock:
        case PinRole::InputUnlock:
        case PinRole::InputUnlatch:
        case PinRole::InputLockNgo:
        case PinRole::InputLockNgoUnlatch:
        case PinRole::InputElectricStrikeActuation:
        case PinRole::InputActivateRTO:
        case PinRole::InputActivateCM:
        case PinRole::InputDeactivateRtoCm:
        case PinRole::InputDeactivateRTO:
        case PinRole::InputDeactivateCM:
        case PinRole::GeneralInputPullUp:
            pinMode(entry.pin, INPUT_PULLUP);
            hasInputPin = true;
            break;
        case PinRole::GeneralInputPullDown:
            pinMode(entry.pin, INPUT_PULLDOWN);
            hasInputPin = true;
            break;
        case PinRole::OutputHighLocked:
        case PinRole::OutputHighUnlocked:
        case PinRole::OutputHighMotorBlocked:
        case PinRole::OutputHighRtoActive:
        case PinRole::OutputHighCmActive:
        case PinRole::OutputHighRtoOrCmActive:
        case PinRole::GeneralOutput:
            pinMode(entry.pin, OUTPUT);
            break;
        case PinRole::Ethernet:
        default:
            break;
        }
    }

    if(hasInputPin)
    {
        _inst->timer = timerBegin(1000000);
        timerAttachInterrupt(_inst->timer, isrOnTimer);
        timerAlarm(_inst->timer, 100000, true, 0);
    }
}

const std::vector<uint8_t>& Gpio::availablePins() const
{
    return _availablePins;
}

void Gpio::loadPinConfiguration()
{
    Log->println("Load GPIO configuration");
    size_t storedLength = _preferences->getBytesLength(preference_gpio_configuration);
    if(storedLength == 0)
    {
        return;
    }

    uint8_t serialized[storedLength];
    memset(serialized, 0, sizeof(serialized));

    size_t size = _preferences->getBytes(preference_gpio_configuration, serialized, sizeof(serialized));

    if(size == 0)
    {
        return;
    }

    size_t numEntries = size / 2;

    _pinConfiguration.clear();
    _pinConfiguration.reserve(numEntries);

    std::vector<int> disabledPins = getDisabledPins();

    for(int i=0; i < numEntries; i++)
    {
        PinEntry entry;
        entry.pin = serialized[i * 2];
        Log->print(F("Pin "));
        Log->println(entry.pin);

        if(std::find(disabledPins.begin(), disabledPins.end(), entry.pin) == disabledPins.end())
        {
            if(entry.role == PinRole::Ethernet)
            {
                entry.role = PinRole::Disabled;
            }
            entry.role = (PinRole) serialized[(i * 2 + 1)];
            Log->println("Not found in Ethernet disabled pins");
            Log->print(F("Role: "));
            Log->println(getRoleDescription(entry.role));
        }
        else
        {
            entry.role = PinRole::Ethernet;
            Log->println("Found in Ethernet disabled pins");
            Log->print(F("Role: "));
            Log->println(getRoleDescription(entry.role));
        }
        if(entry.role != PinRole::Disabled)
        {
            _pinConfiguration.push_back(entry);
        }
    }
}

const  std::vector<int> Gpio::getDisabledPins() const
{
    std::vector<int> disabledPins;

    switch(_preferences->getInt(preference_network_hardware, 0))
    {
    case 2:
        disabledPins.push_back(ETH_PHY_CS_GENERIC_W5500);
        disabledPins.push_back(ETH_PHY_IRQ_GENERIC_W5500);
        disabledPins.push_back(ETH_PHY_RST_GENERIC_W5500);
        disabledPins.push_back(ETH_PHY_SPI_SCK_GENERIC_W5500);
        disabledPins.push_back(ETH_PHY_SPI_MISO_GENERIC_W5500);
        disabledPins.push_back(ETH_PHY_SPI_MOSI_GENERIC_W5500);
        break;
    case 3:
        disabledPins.push_back(ETH_PHY_CS_M5_W5500);
        disabledPins.push_back(ETH_PHY_IRQ_M5_W5500);
        disabledPins.push_back(ETH_PHY_RST_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_SCK_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_MISO_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_MOSI_M5_W5500);
        break;
    case 10:
        disabledPins.push_back(ETH_PHY_CS_M5_W5500_S3);
        disabledPins.push_back(ETH_PHY_IRQ_M5_W5500);
        disabledPins.push_back(ETH_PHY_RST_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_SCK_M5_W5500_S3);
        disabledPins.push_back(ETH_PHY_SPI_MISO_M5_W5500_S3);
        disabledPins.push_back(ETH_PHY_SPI_MOSI_M5_W5500_S3);
        break;
    case 9:
        disabledPins.push_back(ETH_PHY_CS_ETH01EVO);
        disabledPins.push_back(ETH_PHY_IRQ_ETH01EVO);
        disabledPins.push_back(ETH_PHY_RST_ETH01EVO);
        disabledPins.push_back(ETH_PHY_SPI_SCK_ETH01EVO);
        disabledPins.push_back(ETH_PHY_SPI_MISO_ETH01EVO);
        disabledPins.push_back(ETH_PHY_SPI_MOSI_ETH01EVO);
        break;
    case 6:
        disabledPins.push_back(ETH_PHY_CS_M5_W5500);
        disabledPins.push_back(ETH_PHY_IRQ_M5_W5500);
        disabledPins.push_back(ETH_PHY_RST_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_SCK_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_MISO_M5_W5500);
        disabledPins.push_back(ETH_PHY_SPI_MOSI_M5_W5500);
        break;
    case 11:
        disabledPins.push_back(_preferences->getInt(preference_network_custom_cs, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_irq, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_rst, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_sck, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_miso, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_mosi, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_pwr, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_mdc, -1));
        disabledPins.push_back(_preferences->getInt(preference_network_custom_mdio, -1));
        break;
#if defined(CONFIG_IDF_TARGET_ESP32)
    case 4:
        disabledPins.push_back(12);
        disabledPins.push_back(ETH_RESET_PIN_LAN8720);
        disabledPins.push_back(ETH_PHY_MDC_LAN8720);
        disabledPins.push_back(ETH_PHY_MDIO_LAN8720);
        break;
    case 5:
        disabledPins.push_back(16);
        disabledPins.push_back(ETH_RESET_PIN_LAN8720);
        disabledPins.push_back(ETH_PHY_MDC_LAN8720);
        disabledPins.push_back(ETH_PHY_MDIO_LAN8720);
        break;
    case 8:
        disabledPins.push_back(5);
        disabledPins.push_back(ETH_RESET_PIN_LAN8720);
        disabledPins.push_back(ETH_PHY_MDC_LAN8720);
        disabledPins.push_back(ETH_PHY_MDIO_LAN8720);
        break;
    case 7:
        disabledPins.push_back(-1);
        disabledPins.push_back(ETH_RESET_PIN_LAN8720);
        disabledPins.push_back(ETH_PHY_MDC_LAN8720);
        disabledPins.push_back(ETH_PHY_MDIO_LAN8720);
        break;
#endif
    default:
        break;
    }

    Log->print(F("GPIO Ethernet disabled pins:"));
    for_each_n(disabledPins.begin(), disabledPins.size(),
               [](int x)
    {
        Log->print(" ");
        Log->print(x);
    });
    Log->println();
    return disabledPins;
}

void Gpio::savePinConfiguration(const std::vector<PinEntry> &pinConfiguration)
{
    Log->println("Save GPIO configuration");
    int8_t serialized[std::max(pinConfiguration.size() * 2, _preferences->getBytesLength(preference_gpio_configuration))];
    memset(serialized, 0, sizeof(serialized));

    std::vector<int> disabledPins = getDisabledPins();

    int len = pinConfiguration.size();
    for(int i=0; i < len; i++)
    {
        const auto& entry = pinConfiguration[i];
        Log->print(F("Pin "));
        Log->println(entry.pin);

        if(std::find(disabledPins.begin(), disabledPins.end(), entry.pin) != disabledPins.end())
        {
            serialized[i * 2] = entry.pin;
            serialized[i * 2 + 1] = (int8_t)PinRole::Ethernet;
            Log->println("Found in Ethernet disabled pins");
            Log->print(F("Role: "));
            Log->println(getRoleDescription(PinRole::Ethernet));

        }
        else
        {
            if(entry.role != PinRole::Disabled && entry.role != PinRole::Ethernet)
            {
                serialized[i * 2] = entry.pin;
                serialized[i * 2 + 1] = (int8_t) entry.role;
                Log->println("Not found in Ethernet disabled pins");
                Log->print(F("Role: "));
                Log->println(getRoleDescription(entry.role));
            }
        }
    }

    _preferences->putBytes(preference_gpio_configuration, serialized, sizeof(serialized));
}

const std::vector<PinEntry> &Gpio::pinConfiguration() const
{
    return _pinConfiguration;
}

const PinRole Gpio::getPinRole(const int &pin) const
{
    for(const auto& pinEntry : _pinConfiguration)
    {
        if(pinEntry.pin == pin)
        {
            return pinEntry.role;
        }
    }
    return PinRole::Disabled;
}

String Gpio::getRoleDescription(const PinRole& role) const
{
    switch(role)
    {
    case PinRole::Disabled:
        return "Disabled";
    case PinRole::InputLock:
        return "Input: Lock";
    case PinRole::InputUnlock:
        return "Input: Unlock";
    case PinRole::InputUnlatch:
        return "Input: Unlatch";
    case PinRole::InputLockNgo:
        return "Input: Lock n Go";
    case PinRole::InputLockNgoUnlatch:
        return "Input: Lock n Go and unlatch";
    case PinRole::InputElectricStrikeActuation:
        return "Input: Electric strike actuation";
    case PinRole::InputActivateRTO:
        return "Input: Activate RTO";
    case PinRole::InputActivateCM:
        return "Input: Activate CM";
    case PinRole::InputDeactivateRtoCm:
        return "Input: Deactivate RTO/CM";
    case PinRole::InputDeactivateRTO:
        return "Input: Deactivate RTO";
    case PinRole::InputDeactivateCM:
        return "Input: Deactivate CM";
    case PinRole::OutputHighLocked:
        return "Output: High when locked";
    case PinRole::OutputHighUnlocked:
        return "Output: High when unlocked";
    case PinRole::OutputHighMotorBlocked:
        return "Output: High when motor blocked";
    case PinRole::OutputHighRtoActive:
        return "Output: High when RTO active";
    case PinRole::OutputHighCmActive:
        return "Output: High when CM active";
    case PinRole::OutputHighRtoOrCmActive:
        return "Output: High when RTO or CM active";
    case PinRole::GeneralOutput:
        return "General output";
    case PinRole::GeneralInputPullDown:
        return "General input (Pull-down)";
    case PinRole::GeneralInputPullUp:
        return "General input (Pull-up)";
    case PinRole::Ethernet:
        return "Ethernet";
    default:
        return "Unknown";
    }
}


GpioAction Gpio::getGpioAction(const PinRole &role) const
{
    switch(role)
    {
    case PinRole::Disabled:
        return GpioAction::None;
    case PinRole::InputLock:
        return GpioAction::Lock;
    case PinRole::InputUnlock:
        return GpioAction::Unlock;
    case PinRole::InputUnlatch:
        return GpioAction::Unlatch;
    case PinRole::InputLockNgo:
        return GpioAction::LockNgo;
    case PinRole::InputLockNgoUnlatch:
        return GpioAction::LockNgoUnlatch;
    case PinRole::InputElectricStrikeActuation:
        return GpioAction::ElectricStrikeActuation;
    case PinRole::InputActivateRTO:
        return GpioAction::ActivateRTO;
    case PinRole::InputActivateCM:
        return GpioAction::ActivateCM;
    case PinRole::InputDeactivateRtoCm:
        return GpioAction::DeactivateRtoCm;
    case PinRole::InputDeactivateRTO:
        return GpioAction::DeactivateRTO;
    case PinRole::InputDeactivateCM:
        return GpioAction::DeactivateCM;

    case PinRole::GeneralInputPullDown:
    case PinRole::GeneralInputPullUp:
        return GpioAction::GeneralInput;

    case PinRole::GeneralOutput:
    case PinRole::Ethernet:
    case PinRole::OutputHighLocked:
    case PinRole::OutputHighUnlocked:
    case PinRole::OutputHighMotorBlocked:
    case PinRole::OutputHighRtoActive:
    case PinRole::OutputHighCmActive:
    case PinRole::OutputHighRtoOrCmActive:
    default:
        return GpioAction::None;
    }
}


void Gpio::getConfigurationText(String& text, const std::vector<PinEntry>& pinConfiguration, const String& linebreak) const
{
    for(const auto& entry : pinConfiguration)
    {
        if(entry.role != PinRole::Disabled)
        {
            text.concat("GPIO ");
            text.concat(entry.pin);
            if(entry.pin < 10)
            {
                text.concat(' ');
            }
            text.concat(": ");
            text.concat(getRoleDescription(entry.role));
            text.concat(linebreak);
        }
    }
}

const std::vector<PinRole>& Gpio::getAllRoles() const
{
    return _allRoles;
}

void Gpio::notify(const GpioAction &action, const int& pin)
{
    for(auto& callback : _callbacks)
    {
        callback(action, pin);
    }
}

void Gpio::addCallback(std::function<void(const GpioAction&, const int&)> callback)
{
    _callbacks.push_back(callback);
}

void Gpio::setPinOutput(const uint8_t& pin, const uint8_t& state)
{
    digitalWrite(pin, state);
}

void Gpio::migrateObsoleteSetting()
{
    _pinConfiguration.clear();

    PinEntry entry1;
    entry1.pin = 27;
    entry1.role = PinRole::InputUnlatch;

    PinEntry entry2;
    entry2.pin = 32;
    entry2.role = PinRole::InputLock;

    PinEntry entry3;
    entry3.pin = 33;
    entry3.role = PinRole::InputUnlock;

    _pinConfiguration.push_back(entry1);
    _pinConfiguration.push_back(entry2);
    _pinConfiguration.push_back(entry3);

    savePinConfiguration(_pinConfiguration);

    _preferences->remove(preference_gpio_locking_enabled);
    Log->println("Migrated gpio control setting");
    delay(200);
    restartEsp(RestartReason::GpioConfigurationUpdated);
}


