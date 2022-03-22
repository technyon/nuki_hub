#include "Nuki.h"
#include <FreeRTOS.h>

Nuki::Nuki(const std::string& name, uint32_t id)
: _nukiBle(name, id)
{

}

void Nuki::initialize()
{
    _nukiBle.initialize();
}

void Nuki::update()
{
    if (!_paired) {
        if (_nukiBle.pairNuki()) {
            Serial.println("Nuki paired");
            _paired = true;

            _nukiBle.updateKeyTurnerState();
            // nukiBle.requestConfig(false);
            // nukiBle.requestConfig(true);
            // nukiBle.requestBatteryReport();
            _nukiBle.requestKeyPadCodes(0, 2);
            // nukiBle.requestLogEntries(0, 10, 0, true);

            //execute action
            // nukiBle.lockAction(LockAction::lock, 0, 0);
            // addKeypadEntry();
        }
    }

    vTaskDelay( 1000 / portTICK_PERIOD_MS);

    _nukiBle.updateKeyTurnerState();
}
