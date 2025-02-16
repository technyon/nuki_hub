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

/**
  * \brief Certificate and private key that can be passed to the HTTPSServer.
  * 
  * **Converting PEM to DER Files**
  * 
  * Certificate:
  * ```bash
  * openssl x509 -inform PEM -outform DER -in myCert.crt -out cert.der
  * ```
  * 
  * Private Key:
  * ```bash
  * openssl rsa -inform PEM -outform DER -in myCert.key -out key.der
  * ```
  * 
  * **Converting DER File to C Header**
  * 
  * ```bash
  * echo "#ifndef KEY_H_" > ./key.h
  * echo "#define KEY_H_" >> ./key.h
  * xxd -i key.der >> ./key.h
  * echo "#endif" >> ./key.h
  * ```
  */
class SSLCert {
public:
  /**
   * \brief Creates a new SSLCert.
   * 
   * The certificate and key data may be NULL (default values) if the certificate is meant
   * to be passed to createSelfSignedCert().
   * 
   * Otherwise, the data must reside in a memory location that is not deleted until the server
   * using the certificate is stopped.
   * 
   * \param[in] certData The certificate data to use (DER format)
   * \param[in] certLength The length of the certificate data
   * \param[in] pkData The private key data to use (DER format)
   * \param[in] pkLength The length of the private key
   */
  SSLCert(
    unsigned char * certData = NULL,
    uint16_t certLength = 0,
    unsigned char * pkData = NULL,
    uint16_t pkLength = 0
  );
  virtual ~SSLCert();

  /**
   * \brief Returns the length of the certificate in byte
   */
  uint16_t getCertLength();

  /**
   * \brief Returns the length of the private key in byte
   */
  uint16_t getPKLength();

  /**
   * \brief Returns the certificate data
   */
  unsigned char * getCertData();

  /**
   * \brief Returns the private key data
   */
  unsigned char * getPKData();

  /**
   * \brief Sets the private key in DER format
   * 
   * The data has to reside in a place in memory that is not deleted as long as the
   * server is running.
   * 
   * See SSLCert() for some information on how to generate DER data.
   * 
   * \param[in] _pkData The data of the private key
   * \param[in] length The length of the private key
   */
  void setPK(unsigned char * _pkData, uint16_t length);

  /**
   * \brief Sets the certificate data in DER format
   * 
   * The data has to reside in a place in memory that is not deleted as long as the
   * server is running.
   * 
   * See SSLCert for some information on how to generate DER data.
   * 
   * \param[in] _certData The data of the certificate
   * \param[in] length The length of the certificate
   */
  void setCert(unsigned char * _certData, uint16_t length);

  /**
   * \brief Clears the key buffers and deletes them.
   */
  void clear();

private:
  uint16_t _certLength;
  unsigned char * _certData;
  uint16_t _pkLength;
  unsigned char * _pkData;

};

/**
 * \brief Defines the key size for key generation
 * 
 * Not available if the `HTTPS_DISABLE_SELFSIGNING` compiler flag is set
 */
enum SSLKeySize {
  /** \brief RSA key with 1024 bit */
  KEYSIZE_1024 = 1024,
  /** \brief RSA key with 2048 bit */
  KEYSIZE_2048 = 2048,
  /** \brief RSA key with 4096 bit */
  KEYSIZE_4096 = 4096
};

/**
 * \brief Creates a self-signed certificate on the ESP32
 * 
 * This function creates a new self-signed certificate for the given hostname on the heap.
 * Make sure to clear() it before you delete it.
 * 
 * The distinguished name (dn) parameter has to follow the x509 specifications. An example
 * would be:
 *   CN=myesp.local,O=acme,C=US
 * 
 * The strings validFrom and validUntil have to be formatted like this:
 * "20190101000000", "20300101000000"
 * 
 * This will take some time, so you should probably write the certificate data to non-volatile
 * storage when you are done.
 * 
 * Setting the `HTTPS_DISABLE_SELFSIGNING` compiler flag will remove this function from the library
 */
int createSelfSignedCert(SSLCert &certCtx, SSLKeySize keySize, std::string dn, std::string validFrom = "20190101000000", std::string validUntil = "20300101000000");

#endif /* SRC_SSLCERT_HPP_ */
