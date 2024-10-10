//
// A simple server implementation showing how to:
//  * serve static messages
//  * read GET and POST parameters
//  * handle missing pages / 404s
//

#include <Arduino.h>
#include <MongooseCore.h>
#include <MongooseHttpServer.h>

#ifdef ESP32
#include <WiFi.h>
#define START_ESP_WIFI
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#define START_ESP_WIFI
#else
#error Platform not supported
#endif

MongooseHttpServer server;

const char *ssid = "wifi";
const char *password = "password";

const char *server_pem = 
"-----BEGIN CERTIFICATE-----\r\n"
"MIIDDjCCAfagAwIBAgIBBDANBgkqhkiG9w0BAQsFADA/MRkwFwYDVQQDDBB0ZXN0\r\n"
"LmNlc2FudGEuY29tMRAwDgYDVQQKDAdDZXNhbnRhMRAwDgYDVQQLDAd0ZXN0aW5n\r\n"
"MB4XDTE2MTExMzEzMTgwMVoXDTI2MDgxMzEzMTgwMVowFDESMBAGA1UEAwwJbG9j\r\n"
"YWxob3N0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAro8CW1X0xaGm\r\n"
"GkDaMxKbXWA5Lw+seA61tioGrSIQzuqLYeJoFnwVgF0jB5PTj+3EiGMBcA/mh73V\r\n"
"AthTFmJBxj+agIp7/cvUBpgfLClmSYL2fZi6Fodz+f9mcry3XRw7O6vlamtWfTX8\r\n"
"TAmMSR6PXVBHLgjs5pDOFFmrNAsM5sLYU1/1MFvE2Z9InTI5G437IE1WchRSbpYd\r\n"
"HchC39XzpDGoInZB1a3OhcHm+xUtLpMJ0G0oE5VFEynZreZoEIY4JxspQ7LPsay9\r\n"
"fx3Tlk09gEMQgVCeCNiQwUxZdtLau2x61LNcdZCKN7FbFLJszv1U2uguELsTmi7E\r\n"
"6pHrTziosQIDAQABo0AwPjAJBgNVHRMEAjAAMAsGA1UdDwQEAwIDqDATBgNVHSUE\r\n"
"DDAKBggrBgEFBQcDATAPBgNVHREECDAGhwR/AAABMA0GCSqGSIb3DQEBCwUAA4IB\r\n"
"AQBUw0hbTcT6crzODO4QAXU7z4Xxn0LkxbXEsoThG1QCVgMc4Bhpx8gyz5CLyHYz\r\n"
"AiJOBFEeV0XEqoGTNMMFelR3Q5Tg9y1TYO3qwwAWxe6/brVzpts6NiG1uEMBnBFg\r\n"
"oN1x3I9x4NpOxU5MU1dlIxvKs5HQCoNJ8D0SqOX9BV/pZqwEgiCbuWDWQAlxkFpn\r\n"
"iLonlkVI5hTuybCSBsa9FEI9M6JJn9LZmlH90FYHeS4t6P8eOJCeekHL0jUG4Iae\r\n"
"DMP12h8Sd0yxIKmmZ+Q/p/D/BkuHf5Idv3hgyLkZ4mNznjK49wHaYM+BgBoL3Zeg\r\n"
"gJ2sWjUlokrbHswSBLLbUJIF\r\n"
"-----END CERTIFICATE-----\r\n";

const char *server_key = 
"-----BEGIN PRIVATE KEY-----\r\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCujwJbVfTFoaYa\r\n"
"QNozEptdYDkvD6x4DrW2KgatIhDO6oth4mgWfBWAXSMHk9OP7cSIYwFwD+aHvdUC\r\n"
"2FMWYkHGP5qAinv9y9QGmB8sKWZJgvZ9mLoWh3P5/2ZyvLddHDs7q+Vqa1Z9NfxM\r\n"
"CYxJHo9dUEcuCOzmkM4UWas0CwzmwthTX/UwW8TZn0idMjkbjfsgTVZyFFJulh0d\r\n"
"yELf1fOkMagidkHVrc6Fweb7FS0ukwnQbSgTlUUTKdmt5mgQhjgnGylDss+xrL1/\r\n"
"HdOWTT2AQxCBUJ4I2JDBTFl20tq7bHrUs1x1kIo3sVsUsmzO/VTa6C4QuxOaLsTq\r\n"
"ketPOKixAgMBAAECggEAI+uNwpnHirue4Jwjyoqzqd1ZJxQEm5f7UIcJZKsz5kBh\r\n"
"ej0KykWybv27bZ2/1UhKPv6QlyzOdXRc1v8I6fxCKLeB5Z2Zsjo1YT4AfCfwwoPO\r\n"
"kT3SXTx2YyVpQYcP/HsIvVi8FtALtixbxJHaall9iugwHYr8pN17arihAE6d0wZC\r\n"
"JXtXRjUWwjKzXP8FoH4KhyadhHbDwIbbJe3cyLfdvp54Gr0YHha0JcOxYgDYNya4\r\n"
"OKxlCluI+hPF31iNzOmFLQVrdYynyPcR6vY5XOiANKE2iNbqCzRb54CvW9WMqObX\r\n"
"RD9t3DMOxGsbVNIwyzZndWy13HoQMGnrHfnGak9ueQKBgQDiVtOqYfLnUnTxvJ/b\r\n"
"qlQZr2ZmsYPZztxlP+DSqZGPD+WtGSo9+rozWfzjTv3KGIDLvf+GFVmjVHwlLQfd\r\n"
"u7eTemWHFc4HK68wruzPO/FdyVpQ4w9v3Usg+ll4a/PDEId0fDMjAr6kk4LC6t8y\r\n"
"9fJR0HjOz57jVnlrDt3v50G8BwKBgQDFbw+jRiUxXnBbDyXZLi+I4iGBGdC+CbaJ\r\n"
"CmsM6/TsOFc+GRsPwQF1gCGqdaURw76noIVKZJOSc8I+yiwU6izyh/xaju5JiWQd\r\n"
"kwbU1j4DE6GnxmT3ARmB7VvCxjaEZEAtICWs1QTKRz7PcTV8yr7Ng1A3VIy+NSpo\r\n"
"LFMMmk83hwKBgQDVCEwpLg/mUeHoNVVw95w4oLKNLb+gHeerFLiTDy8FrDzM88ai\r\n"
"l37yHly7xflxYia3nZkHpsi7xiUjCINC3BApKyasQoWskh1OgRY653yCfaYYQ96f\r\n"
"t3WjEH9trI2+p6wWo1+uMEMnu/9zXoW9/WeaQdGzNg+igh29+jxCNTPVuQKBgGV4\r\n"
"CN9vI5pV4QTLqjYOSJvfLDz/mYqxz0BrPE1tz3jAFAZ0PLZCCY/sBGFpCScyJQBd\r\n"
"vWNYgYeZOtGuci1llSgov4eDQfBFTlDsyWwFl+VY55IkoqtXw1ZFOQ3HdSlhpKIM\r\n"
"jZBgApA7QYq3sjeqs5lHzahCKftvs5XKgfxOKjxtAoGBALdnYe6xkDvGLvI51Yr+\r\n"
"Dy0TNcB5W84SxUKvM7DVEomy1QPB57ZpyQaoBq7adOz0pWJXfp7qo4950ZOhBGH1\r\n"
"hKbZ6c4ggwVJy2j49EgMok5NGCKvPAtabbR6H8Mz8DW9aXURxhWJvij+Qw1fWK4b\r\n"
"7G/qUI9iE5iUU7MkIcLIbTf/\r\n"
"-----END PRIVATE KEY-----\r\n";

const char *index_page = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"utf-8\" />\n"
"  <title>WebSocket Test</title>\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
"  <style type=\"text/css\">\n"
"    body {\n"
"      background-color: #789; margin: 0;\n"
"      padding: 0; font: 14px Helvetica, Arial, sans-serif;\n"
"    }\n"
"    div.content {\n"
"      width: 800px; margin: 2em auto; padding: 20px 50px;\n"
"      background-color: #fff; border-radius: 1em;\n"
"    }\n"
"    #messages {\n"
"      border: 2px solid #fec; border-radius: 1em;\n"
"      height: 10em; overflow: scroll; padding: 0.5em 1em;\n"
"    }\n"
"    a:link, a:visited { color: #69c; text-decoration: none; }\n"
"    @media (max-width: 700px) {\n"
"      body { background-color: #fff; }\n"
"      div.content {\n"
"        width: auto; margin: 0 auto; border-radius: 0;\n"
"        padding: 1em;\n"
"      }\n"
"    }\n"
"</style>\n"
"\n"
"<script language=\"javascript\" type=\"text/javascript\">\n"
"\n"
"  var rooms = [];\n"
"  var ws = new WebSocket(\'ws://\' + location.host + \'/ws\');\n"
"\n"
"  if (!window.console) { window.console = { log: function() {} } };\n"
"\n"
"  ws.onopen = function(ev)  { console.log(ev); };\n"
"  ws.onerror = function(ev) { console.log(ev); };\n"
"  ws.onclose = function(ev) { console.log(ev); };\n"
"  ws.onmessage = function(ev) {\n"
"    console.log(ev);\n"
"    var div = document.createElement(\'div\');\n"
"    div.innerHTML = ev.data;\n"
"    document.getElementById(\'messages\').appendChild(div);\n"
"\n"
"  };\n"
"\n"
"  window.onload = function() {\n"
"    document.getElementById(\'send_button\').onclick = function(ev) {\n"
"      var msg = document.getElementById(\'send_input\').value;\n"
"      document.getElementById(\'send_input\').value = \'\';\n"
"      ws.send(msg);\n"
"    };\n"
"    document.getElementById(\'send_input\').onkeypress = function(ev) {\n"
"      if (ev.keyCode == 13 || ev.which == 13) {\n"
"        document.getElementById(\'send_button\').click();\n"
"      }\n"
"    };\n"
"  };\n"
"</script>\n"
"</head>\n"
"<body>\n"
"  <div class=\"content\">\n"
"    <h1>Websocket PubSub Demonstration</h1>\n"
"\n"
"    <p>\n"
"      This page demonstrates how Mongoose could be used to implement\n"
"      <a href=\"http://en.wikipedia.org/wiki/Publish%E2%80%93subscribe_pattern\">\n"
"       publish–subscribe pattern</a>. Open this page in several browser\n"
"       windows. Each window initiates persistent\n"
"       <a href=\"http://en.wikipedia.org/wiki/WebSocket\">WebSocket</a>\n"
"      connection with the server, making each browser window a websocket client.\n"
"      Send messages, and see messages sent by other clients.\n"
"    </p>\n"
"\n"
"    <div id=\"messages\">\n"
"    </div>\n"
"\n"
"    <p>\n"
"      <input type=\"text\" id=\"send_input\" />\n"
"      <button id=\"send_button\">Send Message</button>\n"
"    </p>\n"
"  </div>\n"
"</body>\n"
"</html>\n";

#include <Arduino.h>

void broadcast(MongooseHttpWebSocketConnection *from, MongooseString msg)
{
  char buf[500];
  char addr[32];
  mg_sock_addr_to_str(from->getRemoteAddress(), addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) msg.length(), msg.c_str());
  printf("%s\n", buf);
  server.sendAll(from, buf);
}

void setup()
{
  Serial.begin(115200);

#ifdef START_ESP_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
#ifdef ESP32
  Serial.println(WiFi.getHostname());
#elif defined(ESP8266)
  Serial.println(WiFi.hostname());
#endif
#endif

  Mongoose.begin();

#ifdef SIMPLE_SERVER_SECURE
  if(false == server.begin(443, server_pem, server_key)) {
    Serial.print("Failed to start server");
    return;
  }
#else
  server.begin(80);
#endif

  server.on("/$", HTTP_GET, [](MongooseHttpServerRequest *request) {
    request->send(200, "text/html", index_page);
  });

  // Test the stream response class
  server.on("/ws$")->
    onConnect([](MongooseHttpWebSocketConnection *connection) {
      broadcast(connection, MongooseString("++ joined"));
    })->
    onClose([](MongooseHttpServerRequest *c) {
      MongooseHttpWebSocketConnection *connection = static_cast<MongooseHttpWebSocketConnection *>(c);
      broadcast(connection, MongooseString("++ left"));
    })->
    onFrame([](MongooseHttpWebSocketConnection *connection, int flags, uint8_t *data, size_t len) {
      broadcast(connection, MongooseString((const char *)data, len));
    });
}

void loop()
{
  Mongoose.poll(1000);
  Serial.printf("Free memory %u\n", ESP.getFreeHeap());
}
