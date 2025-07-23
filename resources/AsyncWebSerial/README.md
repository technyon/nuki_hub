# AsyncWebSerial

AsyncWebSerial: Simplify ESP32 debugging and logging with seamless browser-based serial communication.

## Features

- **Browser-Based Serial Access**: Log and debug ESP32 microcontrollers directly from a web browser using the Web Serial API.
- **Real-Time Communication**: Stream logs and data in real time, eliminating the need for traditional serial monitors.
- **Asynchronous Operation**: Leverages asynchronous processing for smooth and efficient communication.
- **Cross-Platform Compatibility**: Works on any browser that supports the Web Serial API, no additional software required.
- **Customizable Integration**: Easily integrates into ESP32 projects, enabling tailored debugging and logging workflows.
- **User-Friendly Interface**: Provides an intuitive way to monitor and interact with the ESP32 during development.

## Dependencies

- [AsyncTCP (mathieucarbou fork)](https://github.com/mathieucarbou/AsyncTCP)
- [ESPAsyncWebServer (mathieucarbou fork)](https://github.com/mathieucarbou/ESPAsyncWebServer)

## Build web interface

Install frontend dependencies:

```bash
yarn && yarn build
```

## Implementation

### Add AsyncWebSerial to your platformIO project platformio.ini

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    circuitcode/AsyncWebSerial
```

### Include the library in your code

```cpp
#include <AsyncWebSerial.h>
```

### Initialize the library

```cpp
AsyncWebServer server(80);
AsyncWebSerial webSerial;
```

### Use the library

```cpp
void setup() {
 webSerial.begin(&server);
 server.begin();
}

void loop() {
 webSerial.loop();
}
```

### Send data to the AsyncWebSerial

```cpp
webSerial.println("Hello, World!");
webSerial.printf("Hello, %s!", "World");
```

### Receive data from the AsyncWebSerial

You can use the `onMessage` method to receive data from the AsyncWebSerial. The method accepts a callback function that will be called when data is received. The callback function can accept both `const char *` and `String` data types.

```cpp
webSerial.onMessage([](const char *data, size_t len) {
  Serial.write(data, len);
});

webSerial.onMessage([](const String &msg) {
  Serial.println(msg);
});
```

### Connect to the device serial page

Navigate to `http://<device-ip>/webserial` to access the serial page.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

