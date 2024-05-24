
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <Base32-Decode.h>

void example1() {
  String in, out;
  in = "IFZGI5LJNZXSAUTVNRSXU===";

  int r = base32decodeToString(in, out);
  if (r < 0) {
    Serial.println("Could not decode string");
    return;
  }
  Serial.print("Decoded: ");
  Serial.println(out);
}


void example2() {
  const char * in = "IFZGI5LJNZXSAUTVNRSXU===";
  size_t maxout = strlen(in); // we know that the encoded string is as long, or shorter than the decoded string.
  char out[maxout];

  int r = base32decode(in, (unsigned char*) out, maxout);
  if (r < 0) {
    Serial.println("Could not decode string");
    return;
  }
  Serial.print("Decoded: ");
  Serial.println(out);
}


void example3() {
  const char * in = "IFZGI5LJNZXSAUTVNRSXU===";

  // figure out the lenght we're going to get
  //
  int maxout = base32decode(in, NULL, 0);

  // keep room for an terminating \0
  maxout += 1;

  // declare just enough memory
  char out[maxout];

  int r = base32decode(in, (unsigned char*) out, maxout);
  if (r < 0) {
    Serial.println("Could not decode string");
    return;
  }
  Serial.print("Decoded: ");
  Serial.println(out);
}


// RFC 4648 test vectors - https://www.rfc-editor.org/rfc/rfc4648 section 10

void runalltests() {
  typedef struct testvector_t {
    char *out;
    char *in;
  } testvector_t;
  testvector_t testvectors[] = {
    // normal with padding
    { (char *) "",       (char *)""},
    { (char *) "f",      (char *)"MY======"},
    { (char *) "fo",     (char *)"MZXQ===="},
    { (char *) "foo",    (char *)"MZXW6==="},
    { (char *) "foob",   (char *)"MZXW6YQ="},
    { (char *) "fooba",  (char *)"MZXW6YTB"},
    { (char *) "foobar", (char *)"MZXW6YTBOI======"},
    // careless without the normal padding (but happens a lot)
    { (char *) "f",      (char *)"MY"},
    { (char *) "fo",     (char *) "MZXQ"},
    { (char *) "foo",    (char *)"MZXW6"},
    { (char *) "foob",   (char *)"MZXW6YQ"},
    { (char *) "fooba",  (char *)"MZXW6YTB"},
    { (char *)"foobar",  (char *)"MZXW6YTBOI"},
    // wrong case.
    { (char *) "f",      (char *)"my"},
    { (char *) "fo",     (char *)"mzxq"},
    { (char *) "foo",    (char *)"mzxw6"},
    { (char *) "foob",   (char *)"mzxw6yq"},
    { (char *) "fooba",  (char *)"mzxw6ytb"},
    { (char *)"foobar",  (char *)"mzxw6ytboi"},
    // acidental crufft (not in the RFC)
    { (char *)"",  (char *)" "},
    { (char *)"",  (char *)"   "},
    { (char *)"foobar",  (char *)" mzx w6 yt b o i"},
    { (char *)"foobar",  (char *)" m   zx w6 yt b o i"},
    { (char *)"foobar",  (char *)"mzx\tw6ytboi"},
    { (char *)"foobar",  (char *)"mzxw6\nytboi"},
    { (char *)"foobar",  (char *)"mzxw6  ytb oi "}
  };
  for (int i = 0; i < sizeof(testvectors) / sizeof(testvector_t); i++) {
    unsigned char buff[1024];
    int ret = base32decode(testvectors[i].in, buff, sizeof(buff));
    Serial.printf("test %d: %s -> '%s' == '%s' (size %d)\n", i + 1, testvectors[i].in, buff, testvectors[i].out, ret);
    
    assert(ret == strlen(testvectors[i].out));
    assert(strcmp((char *)buff, testvectors[i].out) == 0);
    
    printf("test: %d ok\n\n", i + 1);
  }
  Serial.println("==\nAll test passed\n\n");
}

void setup() {
  Serial.begin(119200);
  while (!Serial) delay(10);
  Serial.println("\n\n" __FILE__ " started");

  // runalltests();
  example1();
  example2();
  example3();
}

void loop() {
  delay(10000);
}

