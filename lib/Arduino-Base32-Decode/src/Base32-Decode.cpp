#include <Arduino.h>
#include <limits.h>
#include <string.h>

#include "Base32-Decode.h"

// Code and table taken from https://github.com/ekscrypto/Base32/
// under the Unlincense https://unlicense.org> and also by
// Dave Poirier on 12-06-14 who released this as "Public Domain"
//

int base32decode(const char * encoded, unsigned char * decoded, size_t maxbuf) {
#define __ 255
  static char decodingTable[256] = {
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0x00 - 0x0F
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0x10 - 0x1F
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0x20 - 0x2F
    __, __, 26, 27, 28, 29, 30, 31, __, __, __, __, __,  0, __, __, // 0x30 - 0x3F
    __,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 0x40 - 0x4F
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, __, __, __, __, __, // 0x50 - 0x5F
    __,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 0x60 - 0x6F
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, __, __, __, __, __, // 0x70 - 0x7F
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0x80 - 0x8F
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0x90 - 0x9F
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0xA0 - 0xAF
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0xB0 - 0xBF
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0xC0 - 0xCF
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0xD0 - 0xDF
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0xE0 - 0xEF
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 0xF0 - 0xFF
  };

  size_t encodedLength = strlen(encoded);

  // strip any trailing padding.
  while (encodedLength && encoded[encodedLength - 1] == '=') encodedLength--;

  int blocks = (encodedLength + 7) >> 3;
  int expectedDataLength = blocks * 5;

  if (decoded == NULL)
    return expectedDataLength + 1; // for terminating 0

  if (maxbuf <= expectedDataLength)
    return -1;

  unsigned char encodedByte1, encodedByte2, encodedByte3, encodedByte4;
  unsigned char encodedByte5, encodedByte6, encodedByte7, encodedByte8;

  unsigned int encodedToProcess = encodedLength;
  unsigned int  encodedBaseIndex = 0;
  unsigned int  decodedBaseIndex = 0;

  unsigned char block[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  unsigned int  blockIndex = 0;
  unsigned char c;

  while ( encodedToProcess-- >= 1 ) {
    c = encoded[encodedBaseIndex++];
    if ( c == '=' ) break; // padding...

    c = decodingTable[c];
    if ( c == __ ) continue; // skip anything we do not know.

    block[blockIndex++] = c;
    if ( blockIndex == 8 ) {
      encodedByte1 = block[0];
      encodedByte2 = block[1];
      encodedByte3 = block[2];
      encodedByte4 = block[3];
      encodedByte5 = block[4];
      encodedByte6 = block[5];
      encodedByte7 = block[6];
      encodedByte8 = block[7];
      decoded[decodedBaseIndex + 0] = ((encodedByte1 << 3) & 0xF8) | ((encodedByte2 >> 2) & 0x07);
      decoded[decodedBaseIndex + 1] = ((encodedByte2 << 6) & 0xC0) | ((encodedByte3 << 1) & 0x3E) | ((encodedByte4 >> 4) & 0x01);
      decoded[decodedBaseIndex + 2] = ((encodedByte4 << 4) & 0xF0) | ((encodedByte5 >> 1) & 0x0F);
      decoded[decodedBaseIndex + 3] = ((encodedByte5 << 7) & 0x80) | ((encodedByte6 << 2) & 0x7C) | ((encodedByte7 >> 3) & 0x03);
      decoded[decodedBaseIndex + 4] = ((encodedByte7 << 5) & 0xE0) | (encodedByte8 & 0x1F);
      decodedBaseIndex += 5;
      blockIndex = 0;
    }
  }
  encodedByte7 = 0;
  encodedByte6 = 0;
  encodedByte5 = 0;
  encodedByte4 = 0;
  encodedByte3 = 0;
  encodedByte2 = 0;

  if (blockIndex > 6)
    encodedByte7 = block[6];
  if (blockIndex > 5)
    encodedByte6 = block[5];
  if (blockIndex > 4)
    encodedByte5 = block[4];
  if (blockIndex > 3)
    encodedByte4 = block[3];
  if (blockIndex > 2)
    encodedByte3 = block[2];
  if (blockIndex > 1)
    encodedByte2 = block[1];
  if (blockIndex > 0) {
    encodedByte1 = block[0];
    decoded[decodedBaseIndex + 0] = ((encodedByte1 << 3) & 0xF8) | ((encodedByte2 >> 2) & 0x07);
    decoded[decodedBaseIndex + 1] = ((encodedByte2 << 6) & 0xC0) | ((encodedByte3 << 1) & 0x3E) | ((encodedByte4 >> 4) & 0x01);
    decoded[decodedBaseIndex + 2] = ((encodedByte4 << 4) & 0xF0) | ((encodedByte5 >> 1) & 0x0F);
    decoded[decodedBaseIndex + 3] = ((encodedByte5 << 7) & 0x80) | ((encodedByte6 << 2) & 0x7C) | ((encodedByte7 >> 3) & 0x03);
    decoded[decodedBaseIndex + 4] = ((encodedByte7 << 5) & 0xE0);
  };

  static int paddingAdjustment[8] = {0, 1, 1, 1, 2, 3, 3, 4};
  decodedBaseIndex += paddingAdjustment[blockIndex];

  // ensure null terminated if there is space.
  if (decodedBaseIndex < maxbuf)
    decoded[decodedBaseIndex] = 0;
  return decodedBaseIndex;
}

int base32decodeToString(String encoded, String &decoded) {
  size_t maxlen = encoded.length() * 5 / 8 + 1;
  char * buff = new char[maxlen];
  int ret = base32decode(encoded.c_str(), (unsigned char*) buff, maxlen);
  if (ret >= 0)
    decoded = String(buff);
  return ret;
}

#ifdef TEST_BASE32
#include <assert.h>
#include <stdio.h>

int main(int a, char **b) {
  typedef struct testvector_t {
    char *out;
    char *in;
  } testvector_t;
  // RFC 4648 test vectors - https://www.rfc-editor.org/rfc/rfc4648 section 10
  testvector_t testvectors[] = {
    // normal with padding
    {"",       ""},
    {"f",      "MY======"},
    { "fo",     "MZXQ===="},
    {"foo",    "MZXW6==="},
    {"foob",   "MZXW6YQ="},
    {"fooba",  "MZXW6YTB"},
    {"foobar", "MZXW6YTBOI======"},
    // careless without
    {"f",      "MY"},
    {"fo",     "MZXQ"},
    {"foo",    "MZXW6"},
    {"foob",   "MZXW6YQ"},
    {"fooba",  "MZXW6YTB"},
    { "foobar", "MZXW6YTBOI"},
    // wrong case.
    {"f",      "my"},
    {"fo",     "mzxq"},
    {"foo",    "mzxw6"},
    {"foob",   "mzxw6yq"},
    {"fooba",  "mzxw6ytb"},
    { "foobar", "mzxw6ytboi"}
  };
  for (int i = 0; i < sizeof(testvectors) / sizeof(testvector_t); i++) {
    char buff[1024];
    int ret = base32decode(testvectors[i].in, buff, sizeof(buff));
    printf("test %d: %s -> '%s' == '%s' (size %d)\n", i + 1, testvectors[i].in, buff, testvectors[i].out, ret);
    assert(ret == strlen(testvectors[i].out));
    assert(strcmp(buff, testvectors[i].out) == 0);
    printf("test %d ok\n", i + 1);
  }
  return 0;
}
#endif
