# Arduino libary for Base32 (rfc4648) decoding.


## Traditional C interface

### base32decode : decode a \0 terminated base32 encoded string.
  
    int base32decode(
       const char * encoded, 
       unsigned char * decodedBytes, 
       size_t maxbuf
    );

#### inputs: 

    encoded        \0 terminated char buffer with encoded string
    decodedBytes   outputbuffer (or NULL)
    maxbuff        size of the output buffer

#### outputs:

 returns the decoded byte array in decodedBytes and the length. Or if
 decodedBytes==NULL, will just return the length needed; regardless of
 the value of maxbuff.
 
 If the size of maxbuff allows it - a terminating \0 is added (but not
 including in the length returned) - as very often the decoded data
 itself is actually again a string.


## C++/INO/Arduino# tring interface

### base32decodeToString - decode a String into a decoded String

    int base32decodeToString(String encoded, String &decoded);

#### inputs:
    encoded   String with the encoded base32 value
    &decoded  returned string (if any)

#### outputs:  
Will return the length of the decoded string or a negative
 value on error. 


# Example

Typical use:

    String in = "IFZGI5LJNZXSAUTVNRSXU===";
    String out;

    int r = base32decodeToString(in, out);
    if (r < 0) {
      Serial.println("Could not decode the string");
      return;
    };

    Serial.print("Decoded: ");
    Serial.println(out);
