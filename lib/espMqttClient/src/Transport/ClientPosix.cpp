/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#include "ClientPosix.h"

#if defined(__linux__)

namespace espMqttClientInternals {

ClientPosix::ClientPosix()
: _sockfd(-1)
, _host() {
  // empty
}

ClientPosix::~ClientPosix() {
  ClientPosix::stop();
}

bool ClientPosix::connect(IPAddress ip, uint16_t port) {
  if (connected()) stop();

  _sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (_sockfd < 0) {
    emc_log_e("Error %d: \"%s\" opening socket", errno, strerror(errno));
  }

  int flag = 1;
  if (setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0) {
    emc_log_e("Error %d: \"%s\" disabling nagle", errno, strerror(errno));
  }

  memset(&_host, 0, sizeof(_host));
  _host.sin_family = AF_INET;
  _host.sin_addr.s_addr = htonl(uint32_t(ip));
  _host.sin_port = ::htons(port);

  int ret = ::connect(_sockfd, reinterpret_cast<sockaddr*>(&_host), sizeof(_host));

  if (ret < 0) {
    emc_log_e("Error connecting: %d - (%d) %s", ret, errno, strerror(errno));
    return false;
  }

  emc_log_i("Socket connected");
  return true;
}

bool ClientPosix::connect(const char* hostname, uint16_t port) {
  IPAddress ipAddress = _hostToIP(hostname);
  if (ipAddress == IPAddress(0)) {
    emc_log_e("No such host '%s'", hostname);
    return false;
  }
  return connect(ipAddress, port);
}

size_t ClientPosix::write(const uint8_t* buf, size_t size) {
  return ::send(_sockfd, buf, size, 0);
}

int ClientPosix::read(uint8_t* buf, size_t size) {
  int ret = ::recv(_sockfd, buf, size, MSG_DONTWAIT);
  /*
  if (ret < 0) {
    emc_log_e("Error reading: %s", strerror(errno));
  }
  */
  return ret;
}

void ClientPosix::stop() {
  if (_sockfd >= 0) {
    ::close(_sockfd);
    _sockfd = -1;
  }
}

bool ClientPosix::connected() {
  return _sockfd >= 0;
}

bool ClientPosix::disconnected() {
  return _sockfd < 0;
}

IPAddress ClientPosix::_hostToIP(const char* hostname) {
  IPAddress returnIP(0);
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_in *h;
  int rv;

// Set up request addrinfo struct
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  emc_log_i("Looking for '%s'", hostname);

// ask for host data
  if ((rv = getaddrinfo(hostname, NULL, &hints, &servinfo)) != 0) {
    emc_log_e("getaddrinfo: %s", gai_strerror(rv));
    return returnIP;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    h = (struct sockaddr_in *)p->ai_addr;
    returnIP = ::htonl(h->sin_addr.s_addr);
    if (returnIP != IPAddress(0)) break;
  }
  // Release allocated memory
  freeaddrinfo(servinfo);

  if (returnIP != IPAddress(0)) {
    emc_log_i("Host '%s' = %u", hostname, (uint32_t)returnIP);
  } else {
    emc_log_e("No IP for '%s' found", hostname);
  }
  return returnIP;
}

}  // namespace espMqttClientInternals

#endif
