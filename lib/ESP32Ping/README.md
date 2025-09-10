# ESP32Ping
Let the ESP32Ping ping a remote machine.

#Note that this is a port from https://github.com/dancol90/ESP8266Ping
With this library an ESP32Ping can ping a remote machine and know if it's reachable.
It provide some basic measurements on ping messages (avg response time).

## Usage

First, include the library in your sketch along with WiFi library:

```Arduino
#include <WiFi.h>
#include <ESP32Ping.h>
```

Next, simply call the `Ping.ping()` function

```Arduino
IPAddress ip (192, 168, 0, 1); // The remote ip to ping
bool ret = Ping.ping(ip);
```

`ret` will be true if the remote responded to pings, false if not reachable.
The library supports hostname too, just pass a string instead of the ip address:

```Arduino
bool ret = Ping.ping("www.google.com");
```

Additionally, the function accept a second integer parameter `count` that specify how many pings has to be sent:

```Arduino
bool ret = Ping.ping(ip_or_host, 10);
```

After `Ping.ping()` has been called, the average response time (in milliseconds) can be retrieved with

```Arduino
float avg_time_ms = Ping.averageTime();
```
## Fixed in 1.3
Memory leak bug ( https://github.com/marian-craciunescu/ESP32Ping/issues/4 )
## Fixed in 1.4
averageTime changed from `int` to `float`.Expect the code to still work , but you should upgrade 
## Fixed in 1.5
Fixed counters data. (Any review and testing is welcomed)
