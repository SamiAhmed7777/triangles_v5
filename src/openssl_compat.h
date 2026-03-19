#ifndef TRIANGLES_OPENSSL_COMPAT_H
#define TRIANGLES_OPENSSL_COMPAT_H

#include <openssl/opensslv.h>
#include <openssl/crypto.h>

static inline const char* TrianglesOpenSSLVersionString()
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    return OpenSSL_version(OPENSSL_VERSION);
#else
    return SSLeay_version(SSLEAY_VERSION);
#endif
}

#endif // TRIANGLES_OPENSSL_COMPAT_H
