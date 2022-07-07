/*
 Pager Server

 A simple server that echoes any incoming messages to all
 connected clients. Connect two or more telnet sessions
 to see how server.available() and server.print() works.

 created in September 2020 for the Ethernet library
 by Juraj Andrassy https://github.com/jandrassy

*/
#include <Ethernet.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);

EthernetServer server(2323);

void setup() {

  Serial.begin(9600);
  while (!Serial);

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // try to configure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }

  server.begin();

  IPAddress ip = Ethernet.localIP();
  Serial.println();
  Serial.print("To access the server, connect with Telnet client to ");
  Serial.print(ip);
  Serial.println(" 2323");
}

void loop() {

  EthernetClient client = server.available(); // returns first client which has data to read or a 'false' client
  if (client) { // client is true only if it is connected and has data to read
    String s = client.readStringUntil('\n'); // read the message incoming from one of the clients
    s.trim(); // trim eventual \r
    Serial.println(s); // print the message to Serial Monitor
    client.print("echo: "); // this is only for the sending client
    server.println(s); // send the message to all connected clients
#ifndef ARDUINO_ARCH_SAM
    server.flush(); // flush the buffers
#endif /* !defined(ARDUINO_ARCH_SAM) */
  }
}
