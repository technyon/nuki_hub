// Copyright (c) 2015 Cesanta Software Limited
// All rights reserved

#include <Arduino.h>

#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error Platform not supported
#endif

#include "mongoose.h"

const char* ssid     = "my-ssid";
const char* password = "my-password";

static const char *s_http_port = "80";
//static struct mg_serve_http_opts s_http_server_opts;

static void ev_handler(struct mg_connection *nc, int ev, void *p, void *d) {
  static const char *reply_fmt =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "Hello %s\n";

  switch (ev) {
    case MG_EV_ACCEPT: {
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      Serial.printf("Connection %p from %s\n", nc, addr);
      break;
    }
    case MG_EV_HTTP_REQUEST: {
      char addr[32];
      struct http_message *hm = (struct http_message *) p;
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      Serial.printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
             hm->method.p, (int) hm->uri.len, hm->uri.p);
      mg_printf(nc, reply_fmt, addr);
      nc->flags |= MG_F_SEND_AND_CLOSE;
      break;
    }
    case MG_EV_CLOSE: {
      Serial.printf("Connection %p closed\n", nc);
      break;
    }
  }
}

struct mg_mgr mgr;
struct mg_connection *nc;

void setup()
{
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  mg_mgr_init(&mgr, NULL);
  Serial.printf("Starting web server on port %s\n", s_http_port);
  nc = mg_bind(&mgr, s_http_port, ev_handler, NULL);
  if (nc == NULL) {
    Serial.printf("Failed to create listener\n");
    return;
  }

  // Set up HTTP server parameters
  mg_set_protocol_http_websocket(nc);
//  s_http_server_opts.document_root = ".";  // Serve current directory
//  s_http_server_opts.enable_directory_listing = "yes";
}

static uint32_t count = 0;
void loop() 
{
  mg_mgr_poll(&mgr, 1000);
  //Serial.println(count++);
}
