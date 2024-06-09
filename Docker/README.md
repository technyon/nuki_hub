# Build with Docker

You can build this project using Docker. Just run the following commands in the console:

## Build with PlatformIO (will build for the ESP32, ESP32-S3, ESP32-C3 and ESP32-solo1)
```console
git clone https://github.com/technyon/nuki_hub --recursive
cd nuki_hub/Docker
./build_with_docker_pio.sh
```

once the script is complete you will find the nuki_hub binaries in the `nuki_hub/release` folder.

## Build with CMake (will only build for the ESP32)
```console
git clone https://github.com/technyon/nuki_hub --recursive
cd nuki_hub/Docker
./build_with_docker_cmake.sh
```

once the script is complete you will find the nuki_hub binary in the `nuki_hub/build_cmake` folder.
