# PsychicHttp - ESP IDF Example
*  Download and install [ESP IDF 4.4.7](https://github.com/espressif/esp-idf/releases/tag/v4.4.7) (or later version)
*  Clone the project: ```git clone --recursive git@github.com:hoeken/PsychicHttp.git```
*  Run build command: ```cd PsychicHttp/examples/esp-idf``` and then ```idf.py build```
*  Flash the LittleFS filesystem: ```esptool.py write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB 0x317000 build/littlefs.bin```
*  Flash the app firmware: ```idf.py flash monitor``` and visit the IP address shown in the console with a web browser.
*  Learn more about [Arduino as ESP-IDF Component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
