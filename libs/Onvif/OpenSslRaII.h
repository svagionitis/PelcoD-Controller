#pragma once

/// @file OpenSslRaII.h
/// @brief Modern C++ RAII smart pointer wrappers and deleters for OpenSSL opaque types.

#include <cstddef>
#include <cstdint>

#include <memory>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace Onvif::Crypto {

/// @brief Deleter functor releasing an EVP_PKEY_CTX handle.
struct EvpPkeyCtxDeleter {
    void operator()(EVP_PKEY_CTX* p) const noexcept
    {
        if (p != nullptr) {
            EVP_PKEY_CTX_free(p);
        }
    }
};

/// @brief Deleter functor releasing an EVP_PKEY handle.
struct EvpPkeyDeleter {
    void operator()(EVP_PKEY* p) const noexcept
    {
        if (p != nullptr) {
            EVP_PKEY_free(p);
        }
    }
};

/// @brief Deleter functor releasing an EVP_CIPHER_CTX handle.
struct EvpCipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX* p) const noexcept
    {
        if (p != nullptr) {
            EVP_CIPHER_CTX_free(p);
        }
    }
};

/// @brief Deleter functor releasing an EVP_MD_CTX handle.
struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX* p) const noexcept
    {
        if (p != nullptr) {
            EVP_MD_CTX_free(p);
        }
    }
};

/// @brief Deleter functor releasing an X509 certificate handle.
struct X509Deleter {
    void operator()(X509* p) const noexcept
    {
        if (p != nullptr) {
            X509_free(p);
        }
    }
};

/// @brief Deleter functor releasing an X509_REQ certificate request handle.
struct X509ReqDeleter {
    void operator()(X509_REQ* p) const noexcept
    {
        if (p != nullptr) {
            X509_REQ_free(p);
        }
    }
};

/// @brief Deleter functor releasing a BIO stream handle and its chain.
struct BioDeleter {
    void operator()(BIO* p) const noexcept
    {
        if (p != nullptr) {
            BIO_free_all(p);
        }
    }
};

/// @brief Deleter functor releasing a BN_CTX bignum context.
struct BnCtxDeleter {
    void operator()(BN_CTX* p) const noexcept
    {
        if (p != nullptr) {
            BN_CTX_free(p);
        }
    }
};

/// @brief Deleter functor releasing a BIGNUM instance.
struct BignumDeleter {
    void operator()(BIGNUM* p) const noexcept
    {
        if (p != nullptr) {
            BN_free(p);
        }
    }
};

/// @brief Deleter functor releasing an ASN1_INTEGER instance.
struct Asn1IntegerDeleter {
    void operator()(ASN1_INTEGER* p) const noexcept
    {
        if (p != nullptr) {
            ASN1_INTEGER_free(p);
        }
    }
};

/// @brief Deleter functor releasing OpenSSL allocated memory buffers (OPENSSL_malloc/i2d).
struct OpenSslMemDeleter {
    void operator()(void* p) const noexcept
    {
        if (p != nullptr) {
            OPENSSL_free(p);
        }
    }
};

/// @brief RAII unique pointer managing an EVP_PKEY_CTX handle.
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;

/// @brief RAII unique pointer managing an EVP_PKEY handle.
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

/// @brief RAII unique pointer managing an EVP_CIPHER_CTX handle.
using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;

/// @brief RAII unique pointer managing an EVP_MD_CTX handle.
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;

/// @brief RAII unique pointer managing an X509 certificate handle.
using X509Ptr = std::unique_ptr<X509, X509Deleter>;

/// @brief RAII unique pointer managing an X509_REQ certificate request handle.
using X509ReqPtr = std::unique_ptr<X509_REQ, X509ReqDeleter>;

/// @brief RAII unique pointer managing a BIO stream handle.
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

/// @brief RAII unique pointer managing a BN_CTX context.
using BnCtxPtr = std::unique_ptr<BN_CTX, BnCtxDeleter>;

/// @brief RAII unique pointer managing a BIGNUM instance.
using BignumPtr = std::unique_ptr<BIGNUM, BignumDeleter>;

/// @brief RAII unique pointer managing an ASN1_INTEGER instance.
using Asn1IntegerPtr = std::unique_ptr<ASN1_INTEGER, Asn1IntegerDeleter>;

/// @brief RAII unique pointer managing an OpenSSL allocated memory buffer.
using OpenSslMemPtr = std::unique_ptr<unsigned char, OpenSslMemDeleter>;

} // namespace Onvif::Crypto
