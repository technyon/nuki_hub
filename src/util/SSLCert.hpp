/*
MIT License

Copyright (c) 2017 Frank Hessel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SRC_SSLCERT_HPP_
#define SRC_SSLCERT_HPP_

#include <Arduino.h>

#include <string>
#include <mbedtls/rsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/oid.h>

#define HTTPS_SERVER_ERROR_KEYGEN 0x0F
#define HTTPS_SERVER_ERROR_KEYGEN_RNG 0x02
#define HTTPS_SERVER_ERROR_KEYGEN_SETUP_PK 0x03
#define HTTPS_SERVER_ERROR_KEYGEN_GEN_PK 0x04
#define HTTPS_SERVER_ERROR_KEY_WRITE_PK 0x05
#define HTTPS_SERVER_ERROR_KEY_OUT_OF_MEM 0x06
#define HTTPS_SERVER_ERROR_CERTGEN 0x1F
#define HTTPS_SERVER_ERROR_CERTGEN_RNG 0x12
#define HTTPS_SERVER_ERROR_CERTGEN_READKEY 0x13
#define HTTPS_SERVER_ERROR_CERTGEN_WRITE 0x15
#define HTTPS_SERVER_ERROR_CERTGEN_OUT_OF_MEM 0x16
#define HTTPS_SERVER_ERROR_CERTGEN_NAME 0x17
#define HTTPS_SERVER_ERROR_CERTGEN_SERIAL 0x18
#define HTTPS_SERVER_ERROR_CERTGEN_VALIDITY 0x19
#define HTTPS_SERVER_ERROR_CERTGEN_CN 0x1a

class SSLCert {
public:
  SSLCert(
    uint16_t certLength = 0,
    uint16_t pkLength = 0,
    String keyPEM = "",
    String certPEM = ""
  );
  virtual ~SSLCert();
  uint16_t getCertLength();
  uint16_t getPKLength();
  String getCertPEM();
  String getKeyPEM();  
  void setPK(String _keyPEM);
  void setCert(String _certPEM);
  void clear();

private:
  uint16_t _certLength;
  uint16_t _pkLength;
  String _keyPEM;
  String _certPEM;
};

enum SSLKeySize {
  KEYSIZE_1024 = 1024,
  KEYSIZE_2048 = 2048,
  KEYSIZE_4096 = 4096
};

int createSelfSignedCert(SSLCert &certCtx, SSLKeySize keySize, std::string dn, std::string validFrom = "20190101000000", std::string validUntil = "20300101000000");

#endif /* SRC_SSLCERT_HPP_ */
