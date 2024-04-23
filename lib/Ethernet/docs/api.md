# Ethernet Library

## Ethernet Class

### `Ethernet.begin()`

#### Description

Initializes the Ethernet library and network settings.

With version 1.0, the library supports DHCP. Using Ethernet.begin(mac) with the proper network setup, the Ethernet shield will automatically obtain an IP address. This increases the sketch size significantly. To make sure the DHCP lease is properly renewed when needed, be sure to call Ethernet.maintain() regularly.


#### Syntax

```
Ethernet.begin(mac);
Ethernet.begin(mac, ip);
Ethernet.begin(mac, ip, dns);
Ethernet.begin(mac, ip, dns, gateway);
Ethernet.begin(mac, ip, dns, gateway, subnet);
```

#### Parameters
- mac: the MAC (Media access control) address for the device (array of 6 bytes). this is the Ethernet hardware address of your shield. Newer Arduino Ethernet Shields include a sticker with the device's MAC address. For older shields, choose your own.
- ip: the IP address of the device (array of 4 bytes)
- dns: the IP address of the DNS server (array of 4 bytes). optional: defaults to the device IP address with the last octet set to 1
- gateway: the IP address of the network gateway (array of 4 bytes). optional: defaults to the device IP address with the last octet set to 1
- subnet: the subnet mask of the network (array of 4 bytes). optional: defaults to 255.255.255.0

#### Returns
- The DHCP version of this function, Ethernet.begin(mac), returns an int: 1 on a successful DHCP connection, 0 on failure. 
- The other versions don't return anything.

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

// the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
byte ip[] = { 10, 0, 0, 177 };    

void setup()
{
  Ethernet.begin(mac, ip);
}

void loop () {}
```

### `Ethernet.dnsServerIP()`

#### Description

Returns the DNS server IP address for the device.


#### Syntax

```
Ethernet.dnsServerIP()

```

#### Parameters
none

#### Returns
- the DNS server IP address for the device (IPAddress).

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Ethernet.begin(mac, ip);

  Serial.print("The DNS server IP address is: ");
  Serial.println(Ethernet.dnsServerIP());
}

void loop () {}
```

### `Ethernet.gatewayIP()`

#### Description

Returns the gateway IP address for the device.


#### Syntax

```
Ethernet.gatewayIP()

```

#### Parameters
none

#### Returns
- the gateway IP address for the device (IPAddress).

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Ethernet.begin(mac, ip);

  Serial.print("The gateway IP address is: ");
  Serial.println(Ethernet.gatewayIP());
}

void loop () {}
```

### `Ethernet.hardwareStatus()`

#### Description

Ethernet.hardwareStatus() tells you which WIZnet Ethernet controller chip was detected during Ethernet.begin(), if any. This can be used for troubleshooting. If no Ethernet controller was detected then there is likely a hardware problem.


#### Syntax

```
Ethernet.hardwareStatus()

```

#### Parameters
none

#### Returns
- which WIZnet Ethernet controller chip was detected during Ethernet.begin() (EthernetHardwareStatus):

```
EthernetNoHardware
EthernetW5100
EthernetW5200
EthernetW5500
```

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Ethernet.begin(mac, ip);

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.");
  }
  else if (Ethernet.hardwareStatus() == EthernetW5100) {
    Serial.println("W5100 Ethernet controller detected.");
  }
  else if (Ethernet.hardwareStatus() == EthernetW5200) {
    Serial.println("W5200 Ethernet controller detected.");
  }
  else if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500 Ethernet controller detected.");
  }
}

void loop () {}
```

### `Ethernet.init()`

#### Description

Used to configure the CS (chip select) pin for the Ethernet controller chip. The Ethernet library has a default CS pin, which is usually correct, but with some non-standard Ethernet hardware you might need to use a different CS pin.


#### Syntax

```
Ethernet.init(sspin)

```

#### Parameters
- sspin: the pin number to use for CS (byte)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Ethernet.init(53);  // use pin 53 for Ethernet CS
  Ethernet.begin(mac, ip);
}

void loop () {}
```

### `Ethernet.linkStatus()`

#### Description

Tells you whether the link is active. LinkOFF could indicate the Ethernet cable is unplugged or defective. This feature is only available when using the W5200 and W5500 Ethernet controller chips.


#### Syntax

```
Ethernet.linkStatus()

```

#### Parameters
none

#### Returns
- the link status (EthernetLinkStatus):

- Unknown

- LinkON

- LinkOFF

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
}

void loop () {
  if (Ethernet.linkStatus() == Unknown) {
    Serial.println("Link status unknown. Link status detection is only available with W5200 and W5500.");
  }
  else if (Ethernet.linkStatus() == LinkON) {
    Serial.println("Link status: On");
  }
  else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Link status: Off");
  }
}
```

### `Ethernet.localIP()`

#### Description

Obtains the IP address of the Ethernet shield. Useful when the address is auto assigned through DHCP.


#### Syntax

```
Ethernet.localIP();

```

#### Parameters
none

#### Returns
- the IP address

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {  
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

void setup() {
  // start the serial library:
  Serial.begin(9600);
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  // print your local IP address:
  Serial.println(Ethernet.localIP());

}

void loop() {

}
```

### `Ethernet.MACAddress()`

#### Description

Fills the supplied buffer with the MAC address of the device.


#### Syntax

```
Ethernet.MACAddress(mac_address)

```

#### Parameters
- mac_address: buffer to receive the MAC address (array of 6 bytes)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Ethernet.begin(mac, ip);

  byte macBuffer[6];  // create a buffer to hold the MAC address
  Ethernet.MACAddress(macBuffer); // fill the buffer
  Serial.print("The MAC address is: ");
  for (byte octet = 0; octet < 6; octet++) {
    Serial.print(macBuffer[octet], HEX);
    if (octet < 5) {
      Serial.print('-');
    }
  }
}

void loop () {}
 
```

### `Ethernet.maintain()`

#### Description

Allows for the renewal of DHCP leases. When assigned an IP address via DHCP, ethernet devices are given a lease on the address for an amount of time. With Ethernet.maintain(), it is possible to request a renewal from the DHCP server. Depending on the server's configuration, you may receive the same address, a new one, or none at all.

You can call this function as often as you want, it will only re-request a DHCP lease when needed (returning 0 in all other cases). The easiest way is to just call it on every loop() invocation, but less often is also fine. Not calling this function (or calling it significantly less then once per second) will prevent the lease to be renewed when the DHCP protocol requires this, continuing to use the expired lease instead (which will not directly break connectivity, but if the DHCP server leases the same address to someone else, things will likely break).

Ethernet.maintain() was added to Arduino 1.0.1.


#### Syntax

```
Ethernet.maintain();

```

#### Parameters
none

#### Returns

byte:

- 0: nothing happened

- 1: renew failed

- 2: renew success

- 3: rebind fail

- 4: rebind success

### `Ethernet.setDnsServerIP()`

#### Description

Set the IP address of the DNS server. Not for use with DHCP.


#### Syntax

```
Ethernet.setDnsServerIP(dns_server)

```

#### Parameters
- dns_server: the IP address of the DNS server (IPAddress)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);
IPAddress myDns(192, 168, 1, 1);

void setup() {
  Ethernet.begin(mac, ip, myDns);
  IPAddress newDns(192, 168, 1, 1);
  Ethernet.setDnsServerIP(newDns);  // change the DNS server IP address
}

void loop () {}
```

### `Ethernet.setGatewayIP()`

#### Description

Set the IP address of the network gateway. Not for use with DHCP.


#### Syntax

```
Ethernet.setGatewayIP(gateway)

```

#### Parameters
- gateway: the IP address of the network gateway (IPAddress)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);

void setup() {
  Ethernet.begin(mac, ip, myDns, gateway);
  IPAddress newGateway(192, 168, 100, 1);
  Ethernet.setGatewayIP(newGateway);  // change the gateway IP address
}

void loop () {}
```

### `Ethernet.setLocalIP()`

#### Description

Set the IP address of the device. Not for use with DHCP.


#### Syntax

```
Ethernet.setLocalIP(local_ip)

```

#### Parameters
- local_ip: the IP address to use (IPAddress)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Ethernet.begin(mac, ip);
  IPAddress newIp(10, 0, 0, 178);
  Ethernet.setLocalIP(newIp);  // change the IP address
}

void loop () {}
```

### `Ethernet.setMACAddress()`

#### Description

Set the MAC address. Not for use with DHCP.


#### Syntax

```
Ethernet.setMACAddress(mac)

```

#### Parameters
- mac: the MAC address to use (array of 6 bytes)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Ethernet.begin(mac, ip);
  byte newMac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
  Ethernet.setMACAddress(newMac);  // change the MAC address
}

void loop () {}
```

### `Ethernet.setRetransmissionCount()`

#### Description

Set the number of transmission attempts the Ethernet controller will make before giving up. The initial value is 8. 8 transmission attempts times the 200 ms default timeout equals a blocking delay of 1600 ms during a communications failure. You might prefer to set a lower number to make your program more responsive in the event something goes wrong with communications. Despite the name, this sets the total number of transmission attempts (not the number of retries after the first attempt fails) so the minimum value you would ever want to set is 1.


#### Syntax

```
Ethernet.setRetransmissionCount(number)

```

#### Parameters
- number: number of transmission attempts the Ethernet controller should make before giving up (byte)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Ethernet.begin(mac, ip);
  Ethernet.setRetransmissionCount(1);  // configure the Ethernet controller to only attempt one transmission before giving up
}

void loop () {}
```

### `Ethernet.setRetransmissionTimeout()`

#### Description

Set the Ethernet controller's timeout. The initial value is 200 ms. A 200 ms timeout times the default of 8 attempts equals a blocking delay of 1600 ms during a communications failure. You might prefer to set a shorter timeout to make your program more responsive in the event something goes wrong with communications. You will need to do some experimentation to determine an appropriate value for your specific application.


#### Syntax

```
Ethernet.setRetransmissionTimeout(milliseconds)

```

#### Parameters
- milliseconds: the timeout duration (uint16_t)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Ethernet.begin(mac, ip);
  Ethernet.setRetransmissionTimeout(50);  // set the Ethernet controller's timeout to 50 ms
}

void loop () {}
```

### `Ethernet.setSubnetMask()`

#### Description

Set the subnet mask of the network. Not for use with DHCP.


#### Syntax

```
Ethernet.setSubnetMask(subnet)

```

#### Parameters
- subnet: the subnet mask of the network (IPAddress)

#### Returns
Nothing

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

void setup() {
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  IPAddress newSubnet(255, 255, 255, 0);
  Ethernet.setSubnetMask(newSubnet);  // change the subnet mask
}

void loop () {}
```

### `Ethernet.subnetMask()`

#### Description

Returns the subnet mask of the device.


#### Syntax

```
Ethernet.subnetMask()

```

#### Parameters
none

#### Returns
- the subnet mask of the device (IPAddress)

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Ethernet.begin(mac, ip);

  Serial.print("The subnet mask is: ");
  Serial.println(Ethernet.subnetMask());
}

void loop () {}
```

## IPAddress Class

### `IPAddress()`

#### Description

Defines an IP address. It can be used to declare both local and remote addresses.


#### Syntax

```
IPAddress(address);

```

#### Parameters
- address: a comma delimited list representing the address (4 bytes, ex. 192, 168, 1, 1)

#### Returns
None

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

// network configuration. dns server, gateway and subnet are optional.

 // the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  

// the dns server ip
IPAddress dnServer(192, 168, 0, 1);
// the router's gateway address:
IPAddress gateway(192, 168, 0, 1);
// the subnet:
IPAddress subnet(255, 255, 255, 0);

//the IP address is dependent on your network
IPAddress ip(192, 168, 0, 2);

void setup() {
  Serial.begin(9600);

  // initialize the ethernet device
  Ethernet.begin(mac, ip, dnServer, gateway, subnet);
  //print out the IP address
  Serial.print("IP = ");
  Serial.println(Ethernet.localIP());
}

void loop() {
}
```

## Server Class

### `Server`

#### Description
Server is the base class for all Ethernet server based calls. It is not called directly, but invoked whenever you use a function that relies on it.

### `EthernetServer()`

#### Description

Create a server that listens for incoming connections on the specified port.


#### Syntax

```
Server(port);

```

#### Parameters
- port: the port to listen on (int)

#### Returns
None

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

// network configuration.  gateway and subnet are optional.

 // the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
byte ip[] = { 10, 0, 0, 177 };    
// the router's gateway address:
byte gateway[] = { 10, 0, 0, 1 };
// the subnet:
byte subnet[] = { 255, 255, 0, 0 };

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup()
{
  // initialize the ethernet device
  Ethernet.begin(mac, ip, gateway, subnet);

  // start listening for clients
  server.begin();
}

void loop()
{
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client == true) {
    // read bytes from the incoming client and write them back
    // to any clients connected to the server:
    server.write(client.read());
  }
}
```

### `server.begin()`

#### Description

Tells the server to begin listening for incoming connections.


#### Syntax

```
server.begin()

```

#### Parameters
None

#### Returns
None

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

// the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
byte ip[] = { 10, 0, 0, 177 };    
// the router's gateway address:
byte gateway[] = { 10, 0, 0, 1 };
// the subnet:
byte subnet[] = { 255, 255, 0, 0 };

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup()
{
  // initialize the ethernet device
  Ethernet.begin(mac, ip, gateway, subnet);

  // start listening for clients
  server.begin();
}

void loop()
{
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client == true) {
    // read bytes from the incoming client and write them back
    // to any clients connected to the server:
    server.write(client.read());
  }
}
```

### `server.accept()`

#### Description

The traditional server.available() function would only tell you of a new client after it sent data, which makes some protocols like FTP impossible to properly implement.

The intention is programs will use either available() or accept(), but not both. With available(), the client connection continues to be managed by EthernetServer. You don’t need to keep a client object, since calling available() will give you whatever client has sent data. Simple servers can be written with very little code using available().

With accept(), EthernetServer gives you the client only once, regardless of whether it has sent any data. You must keep track of the connected clients. This requires more code, but you gain more control.


#### Syntax

```
server.accept()

```

#### Parameters
none

#### Returns
- a Client object. If no client has data available for reading, this object will evaluate to false in an if-statement. (EthernetClient).

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 69, 104);

// telnet defaults to port 23
EthernetServer server(23);

EthernetClient clients[8];

void setup() {
  Ethernet.begin(mac, ip);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // start listening for clients
  server.begin();
}

void loop() {
  // check for any new client connecting, and say hello (before any incoming data)
  EthernetClient newClient = server.accept();
  if (newClient) {
    for (byte i = 0; i < 8; i++) {
      if (!clients[i]) {
        newClient.print("Hello, client number: ");
        newClient.println(i);
        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        clients[i] = newClient;
        break;
      }
    }
  }

  // check for incoming data from all clients
  for (byte i = 0; i < 8; i++) {
    while (clients[i] && clients[i].available() > 0) {
      // read incoming data from the client
      Serial.write(clients[i].read());
    }
  }

  // stop any clients which disconnect
  for (byte i = 0; i < 8; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
    }
  }
}
```

### `server.available()`

#### Description

Gets a client that is connected to the server and has data available for reading. The connection persists when the returned client object goes out of scope; you can close it by calling client.stop().


#### Syntax

```
server.available()

```

#### Parameters
None

#### Returns
- a Client object; if no Client has data available for reading, this object will evaluate to false in an if-statement (see the example below)

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

// the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
byte ip[] = { 10, 0, 0, 177 };    
// the router's gateway address:
byte gateway[] = { 10, 0, 0, 1 };
// the subnet:
byte subnet[] = { 255, 255, 0, 0 };


// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup()
{
  // initialize the ethernet device
  Ethernet.begin(mac, ip, gateway, subnet);

  // start listening for clients
  server.begin();
}

void loop()
{
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client) {
    // read bytes from the incoming client and write them back
    // to any clients connected to the server:
    server.write(client.read());
  }
}
```

### `if(server)`

#### Description
Indicates whether the server is listening for new clients. You can use this to detect whether server.begin() was successful. It can also tell you when no more sockets are available to listen for more clients, because the maximum number have connected.


#### Syntax

```
if(server)

```

#### Parameters
none

#### Returns
- whether the server is listening for new clients (bool).

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // initialize the Ethernet device
  Ethernet.begin(mac, ip);

  // start listening for clients
  server.begin();
}

void loop() {
  if (server) {
    Serial.println("Server is listening");
  }
  else {
    Serial.println("Server is not listening");
  }
}
```

### `server.write()`

#### Description

Write data to all the clients connected to a server. This data is sent as a byte or series of bytes.


#### Syntax

```
server.write(val)
server.write(buf, len)

```

#### Parameters
- val: a value to send as a single byte (byte or char)

- buf: an array to send as a series of bytes (byte or char)

- len: the length of the buffer

#### Returns
- byte
- write() returns the number of bytes written. It is not necessary to read this.

#### Example

```
#include <SPI.h>
#include <Ethernet.h>

// network configuration.  gateway and subnet are optional.

 // the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
byte ip[] = { 10, 0, 0, 177 };    
// the router's gateway address:
byte gateway[] = { 10, 0, 0, 1 };
// the subnet:
byte subnet[] = { 255, 255, 0, 0 };

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup()
{
  // initialize the ethernet device
  Ethernet.begin(mac, ip, gateway, subnet);

  // start listening for clients
  server.begin();
}

void loop()
{
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client == true) {
    // read bytes from the incoming client and write them back
    // to any clients connected to the server:
    server.write(client.read());
  }
}
```

### `server.print()`

#### Description

Print data to all the clients connected to a server. Prints numbers as a sequence of digits, each an ASCII character (e.g. the number 123 is sent as the three characters '1', '2', '3').


#### Syntax

```
server.print(data)
server.print(data, BASE)

```

#### Parameters
- data: the data to print (char, byte, int, long, or string)

- BASE (optional): the base in which to print numbers: BIN for binary (base 2), DEC for decimal (base 10), OCT for octal (base 8), HEX for hexadecimal (base 16).

#### Returns
- byte
- print() will return the number of bytes written, though reading that number is optional


### `server.println()`

#### Description

Print data, followed by a newline, to all the clients connected to a server. Prints numbers as a sequence of digits, each an ASCII character (e.g. the number 123 is sent as the three characters '1', '2', '3').


#### Syntax

```
server.println()
server.println(data)
server.println(data, BASE)

```

#### Parameters
- data (optional): the data to print (char, byte, int, long, or string)

- BASE (optional): the base in which to print numbers: BIN for binary (base 2), DEC for decimal (base 10), OCT for octal (base 8), HEX for hexadecimal (base 16).

#### Returns
- byte

- println() will return the number of bytes written, though reading that number is optional

## Client Class

### `Client`

#### Description
Client is the base class for all Ethernet client based calls. It is not called directly, but invoked whenever you use a function that relies on it.

### `EthernetClient()`

#### Description

Creates a client which can connect to a specified internet IP address and port (defined in the client.connect() function).


#### Syntax

```
EthernetClient()

```

#### Parameters
None

#### Example

```
#include <Ethernet.h>
#include <SPI.h>


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 10, 0, 0, 177 };
byte server[] = { 64, 233, 187, 99 }; // Google

EthernetClient client;

void setup()
{
  Ethernet.begin(mac, ip);
  Serial.begin(9600);

  delay(1000);

  Serial.println("connecting...");

  if (client.connect(server, 80)) {
    Serial.println("connected");
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
  } else {
    Serial.println("connection failed");
  }
}

void loop()
{
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
      ;
  }
}
```

### `if (EthernetClient)`

#### Description
Indicates if the specified Ethernet client is ready.


#### Syntax

```
if (client)

```

#### Parameters
none

#### Returns
- boolean : returns true if the specified client is available.

#### Example

```
#include <Ethernet.h>
#include <SPI.h>


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 10, 0, 0, 177 };
byte server[] = { 64, 233, 187, 99 }; // Google

EthernetClient client;

void setup()
{
  Ethernet.begin(mac, ip);
  Serial.begin(9600);

  delay(1000);

  Serial.println("connecting...");
  while(!client){
    ; // wait until there is a client connected to proceed
  }
  if (client.connect(server, 80)) {
    Serial.println("connected");
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
  } else {
    Serial.println("connection failed");
  }
}

void loop()
{
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
      ;
  }
}
 
```

### `client.connected()`

#### Description

Whether or not the client is connected. Note that a client is considered connected if the connection has been closed but there is still unread data.


#### Syntax

```
client.connected()

```

#### Parameters
none

#### Returns
- Returns true if the client is connected, false if not.

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 10, 0, 0, 177 };
byte server[] = { 64, 233, 187, 99 }; // Google

EthernetClient client;

void setup()
{
  Ethernet.begin(mac, ip);
  Serial.begin(9600);
  client.connect(server, 80);
  delay(1000);

  Serial.println("connecting...");

  if (client.connected()) {
    Serial.println("connected");
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
  } else {
    Serial.println("connection failed");
  }
}

void loop()
{
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
      ;
  }
}
```

### `client.connect()`

#### Description

Connects to a specified IP address and port. The return value indicates success or failure. Also supports DNS lookups when using a domain name.


#### Syntax

```
client.connect()
client.connect(ip, port)
client.connect(URL, port)

```

#### Parameters
- ip: the IP address that the client will connect to (array of 4 bytes)

- URL: the domain name the client will connect to (string, ex.:"arduino.cc")

- port: the port that the client will connect to (int)

#### Returns
- Returns an int (1,-1,-2,-3,-4) indicating connection status :

- SUCCESS 1
- TIMED_OUT -1
- INVALID_SERVER -2
- TRUNCATED -3
- INVALID_RESPONSE -4
#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 10, 0, 0, 177 };
byte server[] = { 64, 233, 187, 99 }; // Google

EthernetClient client;

void setup()
{
  Ethernet.begin(mac, ip);
  Serial.begin(9600);

  delay(1000);

  Serial.println("connecting...");

  if (client.connect(server, 80)) {
    Serial.println("connected");
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
  } else {
    Serial.println("connection failed");
  }
}

void loop()
{
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
      ;
  }
}
```

### `client.localPort()`

#### Description

Returns the local port number the client is connected to.


#### Syntax

```
client.localPort

```

#### Parameters
none

#### Returns
- the local port number the client is connected to (uint16_t).

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // initialize the Ethernet device
  Ethernet.begin(mac, ip);

  // start listening for clients
  server.begin();
}

void loop() {
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client) {
    Serial.print("Client is connected on port: ");
    Serial.println(client.localPort());
    client.stop();
  }
}
```

### `client.remoteIP()`

#### Description

Returns the IP address of the client.


#### Syntax

```
client.remoteIP()

```

#### Parameters
none

#### Returns
- the client's IP address (IPAddress).

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // initialize the Ethernet device
  Ethernet.begin(mac, ip);

  // start listening for clients
  server.begin();
}

void loop() {
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client) {
    Serial.print("Remote IP address: ");
    Serial.println(client.remoteIP());
    client.stop();
  }
}
```

### `client.remotePort()`

#### Description

Returns the port of the host that sent the current incoming packet.


#### Syntax

```
client.remotePort()

```

#### Parameters
none

#### Returns
- the port of the host that sent the current incoming packet (uint16_t).

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // initialize the Ethernet device
  Ethernet.begin(mac, ip);

  // start listening for clients
  server.begin();
}

void loop() {
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client) {
    Serial.print("Remote port: ");
    Serial.println(client.remotePort());
    client.stop();
  }
}
```

### `client.setConnectionTimeout()`

#### Description

Set the timeout for client.connect() and client.stop(). The initial value is 1000 ms. You might prefer to set a lower timeout value to make your program more responsive in the event something goes wrong.


#### Syntax

```
client.setConnectionTimeout(milliseconds)

```

#### Parameters
- milliseconds: the timeout duration for client.connect() and client.stop() (uint16_t)

#### Returns
Nothing

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 0, 0, 177);

// telnet defaults to port 23
EthernetServer server = EthernetServer(23);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // initialize the Ethernet device
  Ethernet.begin(mac, ip);

  // start listening for clients
  server.begin();
}

void loop() {
  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  if (client) {
    client.setConnectionTimeout(100);  // set the timeout duration for client.connect() and client.stop()
  }
}
```

### `client.write()`

#### Description

Write data to the server the client is connected to. This data is sent as a byte or series of bytes.


#### Syntax

```
client.write(val)
client.write(buf, len)

```

#### Parameters
- val: a value to send as a single byte (byte or char)

- buf: an array to send as a series of bytes (byte or char)

- len: the length of the buffer

#### Returns
byte:
- write() returns the number of bytes written. It is not necessary to read this value.

### `print()`

#### Description

Print data to the server that a client is connected to. Prints numbers as a sequence of digits, each an ASCII character (e.g. the number 123 is sent as the three characters '1', '2', '3').


#### Syntax

```
client.print(data)
client.print(data, BASE)

```

#### Parameters
- data: the data to print (char, byte, int, long, or string)

- BASE (optional): the base in which to print numbers: DEC for decimal (base 10), OCT for octal (base 8), HEX for hexadecimal (base 16).
#### Returns
- byte: returns the number of bytes written, though reading that number is optional

### `client.println()`

#### Description

Print data, followed by a carriage return and newline, to the server a client is connected to. Prints numbers as a sequence of digits, each an ASCII character (e.g. the number 123 is sent as the three characters '1', '2', '3').


#### Syntax

```
client.println()
client.println(data)
client.print(data, BASE)

```

#### Parameters
- data (optional): the data to print (char, byte, int, long, or string)

- BASE (optional): the base in which to print numbers: DEC for decimal (base 10), OCT for octal (base 8), HEX for hexadecimal (base 16).

#### Returns
- byte: return the number of bytes written, though reading that number is optional


### `client.available()`

#### Description

Returns the number of bytes available for reading (that is, the amount of data that has been written to the client by the server it is connected to).

available() inherits from the Stream utility class.


#### Syntax

```
client.available()

```

#### Parameters
none

#### Returns
- The number of bytes available.

#### Example

```
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 10, 0, 0, 177 };
byte server[] = { 64, 233, 187, 99 }; // Google

EthernetClient client;

void setup()
{
  Ethernet.begin(mac, ip);
  Serial.begin(9600);

  delay(1000);

  Serial.println("connecting...");

  if (client.connect(server, 80)) {
    Serial.println("connected");
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
  } else {
    Serial.println("connection failed");
  }
}

void loop()
{
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
      ;
  }
}
```

### `client.read()`

#### Description

Read the next byte received from the server the client is connected to (after the last call to read()).

read() inherits from the Stream utility class.


#### Syntax

```
client.read()

```

#### Parameters
none

#### Returns
- The next byte (or character), or -1 if none is available.

### `client.flush()`
Waits until all outgoing characters in buffer have been sent.

flush() inherits from the Stream utility class.


#### Syntax

```
client.flush()

```

#### Parameters
none

#### Returns
none

### `client.stop()`

#### Description

Disconnect from the server.


#### Syntax

```
client.stop()

```

#### Parameters
none

#### Returns
none

## EthernetUDP Class

### `EthernetUDP.begin()`

#### Description
Initializes the ethernet UDP library and network settings.


#### Syntax

```
EthernetUDP.begin(localPort);
```

#### Parameters
- localPort: the local port to listen on (int)

#### Returns
- 1 if successful, 0 if there are no sockets available to use.

#### Example

```

#include <SPI.h>        

#include <Ethernet.h>

#include <EthernetUdp.h>



// Enter a MAC address and IP address for your controller below.

// The IP address will be dependent on your local network:

byte mac[] = {  

  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

IPAddress ip(192, 168, 1, 177);



unsigned int localPort = 8888;      // local port to listen on



// An EthernetUDP instance to let us send and receive packets over UDP

EthernetUDP Udp;



void setup() {

  // start the Ethernet and UDP:

  Ethernet.begin(mac,ip);

  Udp.begin(localPort);



}



void loop() {
}

  
```

### `EthernetUDP.read()`

#### Description
Reads UDP data from the specified buffer. If no arguments are given, it will return the next character in the buffer.

This function can only be successfully called after UDP.parsePacket().


#### Syntax

```
EthernetUDP.read();
EthernetUDP.read(packetBuffer, MaxSize);
```

#### Parameters
- packetBuffer: buffer to hold incoming packets (char)
- MaxSize: maximum size of the buffer (int)

#### Returns
- char : returns the characters in the buffer

#### Example

```
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);

}

void loop() {

  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBuffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer);
}
}
```

### `EthernetUDP.write()`

#### Description
Writes UDP data to the remote connection. Must be wrapped between beginPacket() and endPacket(). beginPacket() initializes the packet of data, it is not sent until endPacket() is called.


#### Syntax

```
EthernetUDP.write(message);
EthernetUDP.write(buffer, size);

```

#### Parameters

- message: the outgoing message (char)

- buffer: an array to send as a series of bytes (byte or char)

- size: the length of the buffer

#### Returns
- byte : returns the number of characters sent. This does not have to be read

#### Example

```
 

  
#include <SPI.h>        

#include <Ethernet.h>

#include <EthernetUdp.h>



// Enter a MAC address and IP address for your controller below.

// The IP address will be dependent on your local network:

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

IPAddress ip(192, 168, 1, 177);



unsigned int localPort = 8888;      // local port to listen on



// An EthernetUDP instance to let us send and receive packets over UDP

EthernetUDP Udp;



void setup() {

  // start the Ethernet and UDP:

  Ethernet.begin(mac,ip);

  Udp.begin(localPort);

}



void loop() {

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());

    Udp.write("hello");

    Udp.endPacket();

}

  
```

### `EthernetUDP.beginPacket()`

#### Description
Starts a connection to write UDP data to the remote connection


#### Syntax

```
EthernetUDP.beginPacket(remoteIP, remotePort);
```

#### Parameters
- remoteIP: the IP address of the remote connection (4 bytes)
- remotePort: the port of the remote connection (int)
#### Returns
- Returns an int: 1 if successful, 0 if there was a problem resolving the hostname or port.

#### Example

```
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);

}

void loop() {

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write("hello");
    Udp.endPacket();

}
```

### `EthernetUDP.endPacket()`

#### Description
Called after writing UDP data to the remote connection.


#### Syntax

```
EthernetUDP.endPacket();
```

#### Parameters
None

#### Returns
- Returns an int: 1 if the packet was sent successfully, 0 if there was an error

#### Example

```
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);

}

void loop() {

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write("hello");
    Udp.endPacket();

}
```

### `EthernetUDP.parsePacket()`

#### Description
Checks for the presence of a UDP packet, and reports the size. parsePacket() must be called before reading the buffer with UDP.read().


#### Syntax

```
EthernetUDP.parsePacket();
```

#### Parameters
None

#### Returns
- int: the size of a received UDP packet

#### Example

```

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);

  Serial.begin(9600);
}

void loop() {
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
  }
  delay(10);
}
```

### `EthernetUDP.available()`

#### Description

Get the number of bytes (characters) available for reading from the buffer. This is data that's already arrived.

This function can only be successfully called after UDP.parsePacket().

available() inherits from the Stream utility class.


#### Syntax

```
EthernetUDP.available()

```

#### Parameters
None

#### Returns
- the number of bytes available to read

#### Example

```
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);

}

void loop() {

  int packetSize = Udp.parsePacket();
  if(Udp.available())
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBuffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer);
 }
}
```

## UDP class

### `UDP.stop()`

#### Description

Disconnect from the server. Release any resource being used during the UDP session.


#### Syntax

```
UDP.stop()

```

#### Parameters
none

#### Returns
none

### `UDP.remoteIP()`

#### Description
Gets the IP address of the remote connection.

This function must be called after UDP.parsePacket().


#### Syntax

```
UDP.remoteIP();
```

#### Parameters
None

#### Returns
- 4 bytes : the IP address of the remote connection

#### Example

```

#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);
}

void loop() {

  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From IP : ");

    IPAddress remote = Udp.remoteIP();
    //print out the remote connection's IP address
    Serial.print(remote);

    Serial.print(" on port : ");
    //print out the remote connection's port
    Serial.println(Udp.remotePort());
  }

}

 
```

### `UDP.remotePort()`

#### Description
Gets the port of the remote UDP connection.

This function must be called after UDP.parsePacket().


#### Syntax

```
UDP.remotePort();
```

#### Parameters
None

#### Returns
- int : the port of the UDP connection to a remote host

#### Example

```
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

void setup() {
  // start the Ethernet and UDP:
  Ethernet.begin(mac,ip);
  Udp.begin(localPort);

}

void loop() {

  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBuffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer);
 }
}
```