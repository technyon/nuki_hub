/* Copyright (c) Dirk-Willem van Gulik, All rights reserved.
 *  
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * 
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef _TOTP_RFC6238_H
#define _TOTP_RFC6238_H

// Needed for the SHA1
//
#include <mbedtls/md.h>

// Needed for base32 decode - origin 
// https://github.com/dirkx/Arduino-Base32-Decode/releases
//
#include <Base32-Decode.h>

class TOTP {
  public:

    // Defaults from RFC 6238
    // Seed assumed in base64 format; and to be a multiple of 8 bits.
    // once decoded.
    static const time_t RFC6238_DEFAULT_interval = 30; // seconds (default)
    static const time_t RFC6238_DEFAULT_epoch = 0; // epoch relative to the unix epoch (jan 1970 is the default)
    static const int RFC6238_DEFAULT_digits = 6; // length (default is 6)

    static String * currentOTP(String seed,
                               time_t interval = RFC6238_DEFAULT_interval,
                               int digits = RFC6238_DEFAULT_digits,
                               time_t epoch = RFC6238_DEFAULT_epoch
                              )
    {
      return currentOTP(time(NULL), seed, interval, digits, epoch);
    }

    static String * currentOTP(time_t t,
		   	       String seed,
                               time_t interval = RFC6238_DEFAULT_interval,
                               int digits = RFC6238_DEFAULT_digits,
                               time_t epoch = RFC6238_DEFAULT_epoch
                              )
    {
      uint64_t v = t;
      v = (v - epoch) / interval;

      // HMAC is calculated in big-endian (network) order.
      // v = htonll(v);
      
      // Unfortunately htonll is not exposed
      uint32_t endianness = 0xdeadbeef;
      if ((*(const uint8_t *)&endianness) == 0xef) {
        v = ((v & 0x00000000ffffffff) << 32) | ((v & 0xffffffff00000000) >> 32);
        v = ((v & 0x0000ffff0000ffff) << 16) | ((v & 0xffff0000ffff0000) >> 16);
        v = ((v & 0x00ff00ff00ff00ff) <<  8) | ((v & 0xff00ff00ff00ff00) >>  8);
      };
      
      unsigned char buff[ seed.length() ];
      bzero(buff, sizeof(buff));
      int n = base32decode(seed.c_str(), buff, sizeof(buff));
      if (n < 0) {
        Serial.println("Could not decode base32 seed");
        return NULL;
      }

#ifdef RFC6238_DEBUG
      Serial.print("Key: ");
      Serial.print(seed);
      Serial.print(" --> ");
      for (int i = 0; i < n; i++) {
        Serial.printf("%02x", buff[i]);
      }
      Serial.printf(" -- bits=%d -- check this against https://cryptotools.net/otp\n",n * 8);
#endif

      unsigned char digest[20];
      if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),
                          buff, n, // key
                          (unsigned char*) &v, sizeof(v), // input
                          digest)) return NULL;

      uint8_t offst   = digest[19] & 0x0f;
      uint32_t bin_code =   (digest[offst + 0] & 0x7f) << 24
                            | (digest[offst + 1] & 0xff) << 16
                            | (digest[offst + 2] & 0xff) <<  8
                            | (digest[offst + 3] & 0xff);
      int power = pow(10, digits);

#if RFC6238_DEBUG
      // To check against https://cryptotools.net/otp
      //
      for (int i = 0; i < 20; i++) {
        if (offst == i) Serial.print("|");
        Serial.printf("%02x", digest[i]);
        if (offst == i) Serial.print("|");
      }
      Serial.println();
#endif

      // prefix with zero's - as needed & cut off to right number of digits.
      //
      char outbuff[32];
      snprintf(outbuff, sizeof(outbuff), "%06u",  bin_code % power);
      String * otp = new String(outbuff);

      return (otp);
    }
};
#endif
