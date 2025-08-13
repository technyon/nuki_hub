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

#include "SSLCert.hpp"

SSLCert::SSLCert(uint16_t certLength, uint16_t pkLength, String keyPEM, String certPEM):
    _certLength(certLength),
    _pkLength(pkLength),
    _keyPEM(keyPEM),
    _certPEM(certPEM)
{

}

SSLCert::~SSLCert()
{
    // TODO Auto-generated destructor stub
}

uint16_t SSLCert::getCertLength()
{
    return _certLength;
}

uint16_t SSLCert::getPKLength()
{
    return _pkLength;
}

String SSLCert::getKeyPEM()
{
    return _keyPEM;
}

String SSLCert::getCertPEM()
{
    return _certPEM;
}

void SSLCert::setPK(String keyPEM)
{
    _keyPEM = keyPEM;
    _pkLength = keyPEM.length();
}


void SSLCert::setCert(String certPEM)
{
    _certPEM = certPEM;
    _certLength = certPEM.length();
}

void SSLCert::clear()
{
    _certLength = 0;
    _pkLength = 0;

    _keyPEM = "";
    _certPEM = "";
}

/**
 * Returns the CN value from a DN, or "" if it cannot be found
 */
static std::string get_cn(std::string dn)
{
    size_t cnStart = dn.find("CN=");
    if (cnStart == std::string::npos)
    {
        return "";
    }
    cnStart += 3;
    size_t cnStop = dn.find(",", cnStart);
    if (cnStop == std::string::npos)
    {
        cnStop = dn.length();
    }
    return dn.substr(cnStart, cnStop - cnStart);
}

/**
 * Sets the DN as subjectAltName extension in the certificate
 */
static int add_subject_alt_name(mbedtls_x509write_cert *crt, std::string &cn)
{
    size_t bufsize = cn.length() + 8; // some additional space for tags and length fields
    uint8_t buf[bufsize];
    uint8_t *p = &buf[bufsize - 1];
    uint8_t *start = buf;
    int length = 0;
    int ret; // used by MBEDTLS macro

    // The ASN structure that we will construct as parameter for write_crt_set_extension is as follows:
    // | 0x30 = Sequence | length | 0x82 = dNSName, context-specific | length | cn0 | cn1 | cn2 | cn3 | .. | cnn |
    //                       ↑    :                                      ↑     `-------------v------------------´:
    //                       |    :                                      `-------------------´                   :
    //                       |    `----------v------------------------------------------------------------------´
    //                       `---------------´
    // Let's encrypt has useful infos: https://letsencrypt.org/docs/a-warm-welcome-to-asn1-and-der/#choice-and-any-encoding
    MBEDTLS_ASN1_CHK_ADD(length,
                         mbedtls_asn1_write_raw_buffer(&p, start, (uint8_t*)cn.c_str(), cn.length()));
    MBEDTLS_ASN1_CHK_ADD(length,
                         mbedtls_asn1_write_len(&p, start, length));
    MBEDTLS_ASN1_CHK_ADD(length,
                         mbedtls_asn1_write_tag(&p, start, MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0x02)); // 0x02 = dNSName
    MBEDTLS_ASN1_CHK_ADD(length,
                         mbedtls_asn1_write_len(&p, start, length));
    MBEDTLS_ASN1_CHK_ADD(length,
                         mbedtls_asn1_write_tag(&p, start, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ));
    return mbedtls_x509write_crt_set_extension( crt,
            MBEDTLS_OID_SUBJECT_ALT_NAME, MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME),
            0, // not critical
            p, length);
}

/**
 * Function to create the key for a self-signed certificate.
 *
 * Writes private key as DER in certCtx
 *
 * Based on programs/pkey/gen_key.c
 */
static int gen_key(SSLCert &certCtx, SSLKeySize keySize)
{

    // Initialize the entropy source
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init( &entropy );

    // Initialize the RNG
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init( &ctr_drbg );
    int rngRes = mbedtls_ctr_drbg_seed(
                     &ctr_drbg, mbedtls_entropy_func, &entropy,
                     NULL, 0
                 );
    if (rngRes != 0)
    {
        mbedtls_entropy_free( &entropy );
        return HTTPS_SERVER_ERROR_KEYGEN_RNG;
    }

    // Initialize the private key
    mbedtls_pk_context key;
    mbedtls_pk_init( &key );
    int resPkSetup = mbedtls_pk_setup( &key, mbedtls_pk_info_from_type( MBEDTLS_PK_RSA ) );
    if ( resPkSetup != 0)
    {
        mbedtls_ctr_drbg_free( &ctr_drbg );
        mbedtls_entropy_free( &entropy );
        return HTTPS_SERVER_ERROR_KEYGEN_SETUP_PK;
    }

    // Actual key generation
    int resPkGen = mbedtls_rsa_gen_key(
                       mbedtls_pk_rsa( key ),
                       mbedtls_ctr_drbg_random,
                       &ctr_drbg,
                       keySize,
                       65537
                   );
    if ( resPkGen != 0)
    {
        mbedtls_pk_free( &key );
        mbedtls_ctr_drbg_free( &ctr_drbg );
        mbedtls_entropy_free( &entropy );
        return HTTPS_SERVER_ERROR_KEYGEN_GEN_PK;
    }

    // Free the entropy source and the RNG as they are no longer needed
    mbedtls_ctr_drbg_free( &ctr_drbg );
    mbedtls_entropy_free( &entropy );

    // Allocate the space on the heap, as stack size is quite limited
    unsigned char * output_buf = new unsigned char[4096];
    if (output_buf == NULL)
    {
        mbedtls_pk_free( &key );
        return HTTPS_SERVER_ERROR_KEY_OUT_OF_MEM;
    }
    memset(output_buf, 0, 4096);

    // Write the key to the temporary buffer and determine its length
    int resPkWrite = mbedtls_pk_write_key_pem( &key, output_buf, 4096 );
    if (resPkWrite < 0)
    {
        delete[] output_buf;
        mbedtls_pk_free( &key );
        return HTTPS_SERVER_ERROR_KEY_WRITE_PK;
    }

    // Clean up the temporary buffer and clear the key context
    mbedtls_pk_free( &key );

    // Set the private key in the context
    certCtx.setPK((char*)output_buf);

    delete[] output_buf;

    return 0;
}

static int parse_serial_decimal_format(unsigned char *obuf, size_t obufmax,
                                       const char *ibuf, size_t *len)
{
    unsigned long long int dec;
    unsigned int remaining_bytes = sizeof(dec);
    unsigned char *p = obuf;
    unsigned char val;
    char *end_ptr = NULL;

    errno = 0;
    dec = strtoull(ibuf, &end_ptr, 10);

    if ((errno != 0) || (end_ptr == ibuf))
    {
        return -1;
    }

    *len = 0;

    while (remaining_bytes > 0)
    {
        if (obufmax < (*len + 1))
        {
            return -1;
        }

        val = (dec >> ((remaining_bytes - 1) * 8)) & 0xFF;

        /* Skip leading zeros */
        if ((val != 0) || (*len != 0))
        {
            *p = val;
            (*len)++;
            p++;
        }

        remaining_bytes--;
    }

    return 0;
}

/**
 * Function to generate the X509 certificate data for a private key
 *
 * Writes certificate in certCtx
 *
 * Based on programs/x509/cert_write.c
 */

static int cert_write(SSLCert &certCtx, std::string dn, std::string validityFrom, std::string validityTo)
{
    int funcRes = 0;
    int stepRes = 0;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_pk_context key;
    mbedtls_x509write_cert crt;
    unsigned char * primary_buffer;
    unsigned char *certOffset;
    unsigned char * output_buffer;
    size_t certLength;
    const char *serial = "peer";
    size_t serial_len;

    // Make a C-friendly version of the distinguished name
    char dn_cstr[dn.length()+1];
    strcpy(dn_cstr, dn.c_str());

    std::string cn = get_cn(dn);
    if (cn == "")
    {
        return HTTPS_SERVER_ERROR_CERTGEN_CN;
    }

    // Initialize the entropy source
    mbedtls_entropy_init( &entropy );

    // Initialize the RNG
    mbedtls_ctr_drbg_init( &ctr_drbg );
    stepRes = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0 );
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_RNG;
        goto error_after_entropy;
    }

    mbedtls_pk_init( &key );

    stepRes = mbedtls_pk_parse_key( &key, (const unsigned char *)certCtx.getKeyPEM().c_str(), certCtx.getPKLength() + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_READKEY;
        goto error_after_key;
    }

    // Start configuring the certificate
    mbedtls_x509write_crt_init( &crt );
    // Set version and hash algorithm
    mbedtls_x509write_crt_set_version( &crt, MBEDTLS_X509_CRT_VERSION_3 );
    mbedtls_x509write_crt_set_md_alg( &crt, MBEDTLS_MD_SHA256 );

    // Set the keys (same key as we self-sign)
    mbedtls_x509write_crt_set_subject_key( &crt, &key );
    mbedtls_x509write_crt_set_issuer_key( &crt, &key );

    // Set issuer and subject (same, as we self-sign)
    stepRes = mbedtls_x509write_crt_set_subject_name( &crt, dn_cstr );
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_NAME;
        goto error_after_cert;
    }
    stepRes = mbedtls_x509write_crt_set_issuer_name( &crt, dn_cstr );
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_NAME;
        goto error_after_cert;
    }

    // Set the validity of the certificate. At the moment, it's fixed from 2019 to end of 2029.
    stepRes = mbedtls_x509write_crt_set_validity( &crt, validityFrom.c_str(), validityTo.c_str());
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_VALIDITY;
        goto error_after_cert;
    }

    // Make this a CA certificate
    stepRes = mbedtls_x509write_crt_set_basic_constraints( &crt, 1, 0 );
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_VALIDITY;
        goto error_after_cert;
    }

    stepRes = add_subject_alt_name( &crt, cn );
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_NAME;
        goto error_after_cert;
    }

    // Initialize the serial number
    stepRes = mbedtls_x509write_crt_set_serial_raw( &crt, (unsigned char *)serial, strlen(serial) );
    if (stepRes != 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_SERIAL;
        goto error_after_cert_serial;
    }

    // Create buffer to write the certificate
    primary_buffer = new unsigned char[4096];
    if (primary_buffer == NULL)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_OUT_OF_MEM;
        goto error_after_cert_serial;
    }

    // Write the actual certificate
    stepRes = mbedtls_x509write_crt_pem(&crt, primary_buffer, 4096, mbedtls_ctr_drbg_random, &ctr_drbg );
    if (stepRes < 0)
    {
        funcRes = HTTPS_SERVER_ERROR_CERTGEN_WRITE;
        goto error_after_primary_buffer;
    }

    // Configure the cert in the context
    certCtx.setCert((char*)primary_buffer);

    // Run through the cleanup process
error_after_primary_buffer:
    delete[] primary_buffer;

error_after_cert_serial:

error_after_cert:
    mbedtls_x509write_crt_free( &crt );

error_after_key:
    mbedtls_pk_free(&key);

error_after_entropy:
    mbedtls_ctr_drbg_free( &ctr_drbg );
    mbedtls_entropy_free( &entropy );
    return funcRes;
}

int createSelfSignedCert(SSLCert &certCtx, SSLKeySize keySize, std::string dn, std::string validFrom, std::string validUntil)
{

    // Add the private key
    int keyRes = gen_key(certCtx, keySize);
    if (keyRes != 0)
    {
        // Key-generation failed, return the failure code
        return keyRes;
    }

    // Add the self-signed certificate
    int certRes = cert_write(certCtx, dn, validFrom, validUntil);
    if (certRes != 0)
    {
        // Cert writing failed, reset the pk and return failure code
        certCtx.setPK("");
        return certRes;
    }

    // If all went well, return 0
    return 0;
}