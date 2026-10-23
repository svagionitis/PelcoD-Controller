/// @file TestOnvifCryptoRaII.cpp
/// @brief Unit tests for OpenSSL RAII wrappers, X.509 v3 certificates, and cryptographic safety.

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "OnvifCrypto.h"
#include "OnvifSecurity.h"
#include "OnvifTypes.h"
#include "OpenSslRaII.h"

namespace {

TEST(TestOnvifCryptoRaII, DeletersNullptrSafety)
{
    // Deleters must safely accept nullptr without throwing or faulting
    EXPECT_NO_THROW({
        Onvif::Crypto::EvpPkeyCtxDeleter { }(nullptr);
        Onvif::Crypto::EvpPkeyDeleter { }(nullptr);
        Onvif::Crypto::EvpCipherCtxDeleter { }(nullptr);
        Onvif::Crypto::EvpMdCtxDeleter { }(nullptr);
        Onvif::Crypto::X509Deleter { }(nullptr);
        Onvif::Crypto::X509ReqDeleter { }(nullptr);
        Onvif::Crypto::BioDeleter { }(nullptr);
        Onvif::Crypto::BnCtxDeleter { }(nullptr);
        Onvif::Crypto::BignumDeleter { }(nullptr);
        Onvif::Crypto::Asn1IntegerDeleter { }(nullptr);
        Onvif::Crypto::OpenSslMemDeleter { }(nullptr);
    });
}

TEST(TestOnvifCryptoRaII, RaIiSmartPointersLifecycle)
{
    // Allocate and let unique_ptr manage lifecycle
    {
        const Onvif::Crypto::EvpMdCtxPtr mdCtx { EVP_MD_CTX_new() };
        ASSERT_NE(mdCtx, nullptr);
    }
    {
        const Onvif::Crypto::BioPtr bio { BIO_new(BIO_s_mem()) };
        ASSERT_NE(bio, nullptr);
    }
    {
        const Onvif::Crypto::BignumPtr bn { BN_new() };
        ASSERT_NE(bn, nullptr);
    }
    {
        const Onvif::Crypto::BnCtxPtr bnCtx { BN_CTX_new() };
        ASSERT_NE(bnCtx, nullptr);
    }
    {
        const Onvif::Crypto::X509Ptr x509 { X509_new() };
        ASSERT_NE(x509, nullptr);
    }
    {
        const Onvif::Crypto::X509ReqPtr req { X509_REQ_new() };
        ASSERT_NE(req, nullptr);
    }
}

TEST(TestOnvifCryptoRaII, GenerateNonceAndDigest)
{
    const std::vector<std::uint8_t> nonce = Onvif::OnvifSecurity::generateNonce();
    EXPECT_EQ(nonce.size(), 16U);

    const std::string createdUtc = "2026-10-23T12:00:00Z";
    const std::string password = "SecretPassword123!";
    const std::string digest = Onvif::OnvifSecurity::computePasswordDigest(nonce, createdUtc, password);
    EXPECT_FALSE(digest.empty());
}

TEST(TestOnvifCryptoRaII, GenerateSelfSignedCertificateV3)
{
    const std::string certId = "Cert_Test_1";
    const std::string subject = "CN=TestCamera,O=PelcoD";
    const int daysValid = 365;

    const Onvif::OnvifCertificate cert
        = Onvif::OnvifSecurity::generateSelfSignedCertificate(certId, subject, daysValid);

    EXPECT_EQ(cert.certificateId, certId);
    EXPECT_EQ(cert.info.certificateId, certId);
    EXPECT_FALSE(cert.x509DerBase64.empty());
    EXPECT_EQ(cert.info.keyAlgorithm, "RSA");
    EXPECT_TRUE(cert.info.isDefault);

    // Decode and verify X.509 v3 (version index 2)
    const auto derBytes = Onvif::OnvifSecurity::base64Decode(cert.x509DerBase64);
    ASSERT_FALSE(derBytes.empty());

    const unsigned char* p = derBytes.data();
    const Onvif::Crypto::X509Ptr x509 { d2i_X509(nullptr, &p, static_cast<long>(derBytes.size())) };
    ASSERT_NE(x509, nullptr);

    // X.509 v3 is version index 2
    EXPECT_EQ(X509_get_version(x509.get()), 2L);

    // Serial number should be positive and non-null (64-bit CSPRNG, portable across LLP64/LP64)
    const ASN1_INTEGER* serial = X509_get_serialNumber(x509.get());
    ASSERT_NE(serial, nullptr);
    const Onvif::Crypto::BignumPtr bn { ASN1_INTEGER_to_BN(serial, nullptr) };
    ASSERT_NE(bn, nullptr);
    EXPECT_FALSE(BN_is_zero(bn.get()));
    EXPECT_FALSE(BN_is_negative(bn.get()));
    EXPECT_GT(BN_num_bits(bn.get()), 0);
}

TEST(TestOnvifCryptoRaII, DistinctRandomSerialNumbers)
{
    // Generating two certificates must produce distinct serial numbers (RFC 5280)
    const Onvif::OnvifCertificate cert1
        = Onvif::OnvifSecurity::generateSelfSignedCertificate("Cert_A", "CN=CameraA", 30);
    const Onvif::OnvifCertificate cert2
        = Onvif::OnvifSecurity::generateSelfSignedCertificate("Cert_B", "CN=CameraB", 30);

    const auto der1 = Onvif::OnvifSecurity::base64Decode(cert1.x509DerBase64);
    const auto der2 = Onvif::OnvifSecurity::base64Decode(cert2.x509DerBase64);
    ASSERT_FALSE(der1.empty());
    ASSERT_FALSE(der2.empty());

    const unsigned char* p1 = der1.data();
    const Onvif::Crypto::X509Ptr x1 { d2i_X509(nullptr, &p1, static_cast<long>(der1.size())) };
    const unsigned char* p2 = der2.data();
    const Onvif::Crypto::X509Ptr x2 { d2i_X509(nullptr, &p2, static_cast<long>(der2.size())) };

    ASSERT_NE(x1, nullptr);
    ASSERT_NE(x2, nullptr);

    const ASN1_INTEGER* s1 = X509_get_serialNumber(x1.get());
    const ASN1_INTEGER* s2 = X509_get_serialNumber(x2.get());
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);

    EXPECT_NE(ASN1_INTEGER_cmp(s1, s2), 0);
}

TEST(TestOnvifCryptoRaII, LargeValidityPeriodDoesNotOverflow)
{
    // Test large validity duration (e.g. 50,000 days > INT32 overflow threshold 24,855)
    constexpr int kLargeDays = 50000;
    const Onvif::OnvifCertificate cert
        = Onvif::OnvifSecurity::generateSelfSignedCertificate("Cert_OverflowTest", "CN=SafeTime", kLargeDays);

    EXPECT_FALSE(cert.x509DerBase64.empty());
    EXPECT_FALSE(cert.info.validNotAfter.empty());

    const auto der = Onvif::OnvifSecurity::base64Decode(cert.x509DerBase64);
    ASSERT_FALSE(der.empty());

    const unsigned char* p = der.data();
    const Onvif::Crypto::X509Ptr x509 { d2i_X509(nullptr, &p, static_cast<long>(der.size())) };
    ASSERT_NE(x509, nullptr);

    // Verify it parses cleanly
    const Onvif::CertificateInformation parsed
        = Onvif::OnvifSecurity::parseCertificateInfo("Cert_OverflowTest", cert.x509DerBase64);
    EXPECT_FALSE(parsed.validNotAfter.empty());
}

TEST(TestOnvifCryptoRaII, ParseCertificateInfoExtractsRealMetadata)
{
    const std::string certId = "Cert_Parse_Test";
    const std::string subject = "CN=RealMetadataCamera,O=PelcoD";
    const Onvif::OnvifCertificate cert = Onvif::OnvifSecurity::generateSelfSignedCertificate(certId, subject, 180);

    ASSERT_FALSE(cert.x509DerBase64.empty());

    const Onvif::CertificateInformation info = Onvif::OnvifSecurity::parseCertificateInfo(certId, cert.x509DerBase64);

    EXPECT_EQ(info.certificateId, certId);
    EXPECT_NE(info.subject.find("RealMetadataCamera"), std::string::npos);
    EXPECT_NE(info.issuer.find("RealMetadataCamera"), std::string::npos);
    EXPECT_FALSE(info.validNotBefore.empty());
    EXPECT_FALSE(info.validNotAfter.empty());
}

TEST(TestOnvifCryptoRaII, ParseCertificateInfoGracefulFallbackOnInvalidDer)
{
    const Onvif::CertificateInformation info
        = Onvif::OnvifSecurity::parseCertificateInfo("Cert_Corrupt", "NotValidBase64DER==");

    EXPECT_EQ(info.certificateId, "Cert_Corrupt");
    EXPECT_FALSE(info.subject.empty());
}

TEST(TestOnvifCryptoRaII, GeneratePkcs10CsrValidPem)
{
    const std::string certId = "Csr_Test_1";
    const std::string subject = "CN=CsrCamera,O=PelcoD";

    const Onvif::Pkcs10Request csr = Onvif::OnvifSecurity::generatePkcs10Csr(certId, subject);

    EXPECT_EQ(csr.certificateId, certId);
    EXPECT_FALSE(csr.csrBase64.empty());
    EXPECT_NE(csr.csrBase64.find("-----BEGIN CERTIFICATE REQUEST-----"), std::string::npos);
    EXPECT_NE(csr.csrBase64.find("-----END CERTIFICATE REQUEST-----"), std::string::npos);
}

TEST(TestOnvifCryptoRaII, CryptoHexDigestAndHmac)
{
    const auto digestOpt = Onvif::Crypto::hexDigest(EVP_sha256(), "Hello OpenSSL RAII");
    ASSERT_TRUE(digestOpt.has_value());
    EXPECT_EQ(digestOpt->size(), 64U); // SHA-256 is 64 hex chars

    const std::vector<std::uint8_t> key = { 'k', 'e', 'y' };
    const std::vector<std::uint8_t> data = { 'd', 'a', 't', 'a' };
    const auto hmac = Onvif::Crypto::hmacSha256(key, data);
    EXPECT_EQ(hmac.size(), 32U); // HMAC-SHA256 is 32 bytes

    EXPECT_TRUE(Onvif::Crypto::constTimeEqual("constant_time_test", "constant_time_test"));
    EXPECT_FALSE(Onvif::Crypto::constTimeEqual("constant_time_test", "constant_time_diff"));
}

} // namespace
