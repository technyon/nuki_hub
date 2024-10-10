//
// A simple server implementation showing how to:
//  * serve static messages
//  * read GET and POST parameters
//  * handle missing pages / 404s
//

#include <Arduino.h>
#include <MongooseCore.h>
#include <MongooseHttpServer.h>
#include <Update.h>

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

const char* server_index = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
"</form>"
"<div id='prg'>progress: 0%</div>"
"<script>"
  "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    "$.ajax({"
      "url: '/update',"
      "type: 'POST',"
      "data: data,"
      "contentType: false,"
      "processData:false,"
      "xhr: function() {"
        "var xhr = new window.XMLHttpRequest();"
        "xhr.upload.addEventListener('progress', function(evt) {"
          "if (evt.lengthComputable) {"
            "var per = evt.loaded / evt.total;"
            "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
          "}"
        "}, false);"
        "return xhr;"
      "},"
      "success:function(d, s) {"
      "console.log('success!')" 
    "},"
    "error: function (a, b, c) {"
    "}"
  "});"
 "});"
"</script>";

#include <Arduino.h>

bool updateCompleted = false;

static void updateError(MongooseHttpServerRequest *request)
{
  MongooseHttpServerResponseStream *resp = request->beginResponseStream();
  resp->setCode(500);
  resp->setContentType("text/plain");
  resp->printf("Error: %d", Update.getError());
  request->send(resp);

  // Anoyingly this uses Stream rather than Print...
  Update.printError(Serial);
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
#if defined(ADMIN_USER) && defined(ADMIN_PASS) && defined(ADMIN_REALM)
    if(false == request->authenticate(ADMIN_USER, ADMIN_PASS)) {
      request->requestAuthentication(ADMIN_REALM);
      return;
    }
#endif

    request->send(200, "text/html", server_index);
  });

  server.on("/update$", HTTP_POST)->
    onRequest([](MongooseHttpServerRequest *request) {
#if defined(ADMIN_USER) && defined(ADMIN_PASS) && defined(ADMIN_REALM)
      if(false == request->authenticate(ADMIN_USER, ADMIN_PASS)) {
        request->requestAuthentication(ADMIN_REALM);
        return;
      }
#endif
      updateCompleted = false;
    })->
    onUpload([](MongooseHttpServerRequest *request, int ev, MongooseString filename, uint64_t index, uint8_t *data, size_t len)
    {
      if(MG_EV_HTTP_PART_BEGIN == ev) {
        Serial.printf("Update Start: %s\n", filename.c_str());

        if (!Update.begin()) { //start with max available size
          updateError(request);
        }
      }

      if(!Update.hasError())
      {
        Serial.printf("Update Writing %llu\n", index);
        if(Update.write(data, len) != len) {
          updateError(request);
        }
      }

      if(MG_EV_HTTP_PART_END == ev) {
        Serial.println("Data finished");
        if(Update.end(true)) {
          Serial.printf("Update Success: %lluB\n", index+len);
          request->send(200, "text/plain", "OK");
          updateCompleted = true;
        } else {
          updateError(request);
        }
      }

      return len;
    })->
    onClose([](MongooseHttpServerRequest *request) 
    {
      if(updateCompleted) {
        ESP.restart();
      }
    });
}

void loop()
{
  Mongoose.poll(1000);
}
