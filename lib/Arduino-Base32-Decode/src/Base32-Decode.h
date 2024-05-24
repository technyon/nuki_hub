#ifndef BASE32_DECODE_H
#define BASE32_DECODE_H

/* base32decode - decode a \0 terminated base32 encoded string.
 *  
 * encoded        \0 terminated char buffer with encoded string
 * decodedBytes   outputbuffer (or NULL)
 * maxbuff        size of the output buffer
 *
 * returns the decoded byte array in decodedBytes and the length. Or if
 * decodedBytes==NULL, will just return the length needed; regardless of
 * the value of maxbuff.
 * 
 * If the size of maxbuff allows it - a terminating \0 is added (but not
 * including in the length returned) - as very often the decoded data
 * itself is actually again a string.
 */
extern int base32decode(const char * encoded, unsigned char * decodedBytes, size_t maxbuf);

/* base32decodeToString - decode a String into a decoded String
 *  
 *  encoded   String with the encoded base32 value
 *  &decoded  returned string (if any)
 *  
 * Will return the length of the decoded string or a negative
 * value on error. 
 */

extern int base32decodeToString(String encoded, String &decoded);
#endif
