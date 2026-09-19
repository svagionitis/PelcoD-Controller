#include "OnvifSecurity.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <array>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PelcoD::Onvif {

namespace {

    constexpr char kBase64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

} // namespace

std::vector<std::uint8_t> OnvifSecurity::generateNonce()
{
    constexpr size_t kNonceLength { 16 };
    std::vector<std::uint8_t> nonce(kNonceLength, 0);
    RAND_bytes(nonce.data(), static_cast<int>(kNonceLength));
    return nonce;
}

std::string OnvifSecurity::generateIsoTimestamp(std::chrono::seconds offset)
{
    const auto now = std::chrono::system_clock::now() + offset;
    const std::time_t timeT = std::chrono::system_clock::to_time_t(now);

    std::tm utcTm {};
#if defined(_WIN32)
    gmtime_s(&utcTm, &timeT);
#else
    gmtime_r(&timeT, &utcTm);
#endif

    std::ostringstream ss {};
    ss << std::put_time(&utcTm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string OnvifSecurity::base64Encode(const std::vector<std::uint8_t>& data)
{
    std::string result {};
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i { 0 };
    while (i < data.size()) {
        const std::uint32_t octetA = (i < data.size()) ? static_cast<std::uint8_t>(data[i++]) : 0;
        const std::uint32_t octetB = (i < data.size()) ? static_cast<std::uint8_t>(data[i++]) : 0;
        const std::uint32_t octetC = (i < data.size()) ? static_cast<std::uint8_t>(data[i++]) : 0;

        const std::uint32_t triple = (octetA << 16) + (octetB << 8) + octetC;

        result.push_back(kBase64Table[(triple >> 18) & 0x3F]);
        result.push_back(kBase64Table[(triple >> 12) & 0x3F]);
        result.push_back(kBase64Table[(triple >> 6) & 0x3F]);
        result.push_back(kBase64Table[triple & 0x3F]);
    }

    const size_t mod = data.size() % 3;
    if (mod == 1) {
        result[result.size() - 1] = '=';
        result[result.size() - 2] = '=';
    } else if (mod == 2) {
        result[result.size() - 1] = '=';
    }

    return result;
}

std::string OnvifSecurity::base64Encode(const std::string& text)
{
    const std::vector<std::uint8_t> data(text.begin(), text.end());
    return base64Encode(data);
}

std::string OnvifSecurity::computePasswordDigest(
    const std::vector<std::uint8_t>& rawNonce, const std::string& createdUtc, const std::string& password)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return {};
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    if (!rawNonce.empty()) {
        EVP_DigestUpdate(ctx, rawNonce.data(), rawNonce.size());
    }
    if (!createdUtc.empty()) {
        EVP_DigestUpdate(ctx, createdUtc.data(), createdUtc.size());
    }
    if (!password.empty()) {
        EVP_DigestUpdate(ctx, password.data(), password.size());
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> md {};
    unsigned int mdLen { 0 };
    EVP_DigestFinal_ex(ctx, md.data(), &mdLen);
    EVP_MD_CTX_free(ctx);

    const std::vector<std::uint8_t> digestBytes(md.begin(), md.begin() + mdLen);
    return base64Encode(digestBytes);
}

UsernameTokenData OnvifSecurity::createTokenData(const SecurityCredentials& credentials)
{
    UsernameTokenData data {};
    data.username = credentials.username;
    if (data.username.empty()) {
        return data;
    }

    const std::vector<std::uint8_t> rawNonce = generateNonce();
    data.nonceBase64 = base64Encode(rawNonce);
    data.createdUtc = generateIsoTimestamp(credentials.clockOffset);
    data.passwordDigest = computePasswordDigest(rawNonce, data.createdUtc, credentials.password);
    return data;
}

std::string OnvifSecurity::buildSoapSecurityHeader(const SecurityCredentials& credentials)
{
    if (credentials.username.empty()) {
        return {};
    }

    const UsernameTokenData token = createTokenData(credentials);

    std::ostringstream ss {};
    ss << "<wsse:Security "
          "xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
       << "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">\n"
       << "  <wsse:UsernameToken>\n"
       << "    <wsse:Username>" << token.username << "</wsse:Username>\n"
       << "    <wsse:Password "
          "Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">"
       << token.passwordDigest << "</wsse:Password>\n"
       << "    <wsse:Nonce "
          "EncodingType=\"http://docs.oasis-open.org/wss/2004/01/"
          "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">"
       << token.nonceBase64 << "</wsse:Nonce>\n"
       << "    <wsu:Created>" << token.createdUtc << "</wsu:Created>\n"
       << "  </wsse:UsernameToken>\n"
       << "</wsse:Security>";

    return ss.str();
}

std::vector<std::uint8_t> OnvifSecurity::base64Decode(const std::string& base64Text)
{
    std::vector<std::uint8_t> result;
    result.reserve((base64Text.size() * 3) / 4);
    std::vector<int> table(256, -1);
    for (int i = 0; i < 64; ++i) {
        table[static_cast<unsigned char>(kBase64Table[i])] = i;
    }
    int val = 0;
    int valb = -8;
    for (char ch : base64Text) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '\r' || c == '\n' || c == ' ') {
            continue;
        }
        if (c == '=') {
            break;
        }
        if (table[c] == -1) {
            continue;
        }
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<std::uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

OnvifCertificate OnvifSecurity::generateSelfSignedCertificate(
    const std::string& certificateId, const std::string& subject, int daysValid)
{
    OnvifCertificate cert {};
    cert.certificateId = certificateId;
    cert.info.certificateId = certificateId;
    cert.info.subject = subject.empty() ? ("CN=PelcoD-" + certificateId) : subject;
    cert.info.issuer = cert.info.subject;
    cert.info.keyAlgorithm = "RSA";
    cert.info.isDefault = true;
    cert.info.validNotBefore = generateIsoTimestamp(std::chrono::seconds(0));
    cert.info.validNotAfter = generateIsoTimestamp(std::chrono::seconds(daysValid * 86400));

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!pctx) {
        return cert;
    }
    if (EVP_PKEY_keygen_init(pctx) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return cert;
    }
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0 || !pkey) {
        EVP_PKEY_CTX_free(pctx);
        return cert;
    }
    EVP_PKEY_CTX_free(pctx);

    X509* x509 = X509_new();
    if (!x509) {
        EVP_PKEY_free(pkey);
        return cert;
    }
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), static_cast<long>(daysValid * 86400));
    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(cert.info.subject.c_str()), -1, -1, 0);
    X509_set_issuer_name(x509, name);

    if (X509_sign(x509, pkey, EVP_sha256()) > 0) {
        unsigned char* der = nullptr;
        const int derLen = i2d_X509(x509, &der);
        if (derLen > 0 && der != nullptr) {
            std::vector<std::uint8_t> derBytes(der, der + derLen);
            cert.x509DerBase64 = base64Encode(derBytes);
            OPENSSL_free(der);
        }
    }

    X509_free(x509);
    EVP_PKEY_free(pkey);
    return cert;
}

Pkcs10Request OnvifSecurity::generatePkcs10Csr(const std::string& certificateId, const std::string& subject)
{
    Pkcs10Request req {};
    req.certificateId = certificateId;
    req.subject = subject.empty() ? ("CN=PelcoD-" + certificateId) : subject;

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!pctx) {
        return req;
    }
    if (EVP_PKEY_keygen_init(pctx) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return req;
    }
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0 || !pkey) {
        EVP_PKEY_CTX_free(pctx);
        return req;
    }
    EVP_PKEY_CTX_free(pctx);

    X509_REQ* x509Req = X509_REQ_new();
    if (!x509Req) {
        EVP_PKEY_free(pkey);
        return req;
    }
    X509_REQ_set_version(x509Req, 0);
    X509_NAME* name = X509_REQ_get_subject_name(x509Req);
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(req.subject.c_str()), -1, -1, 0);
    X509_REQ_set_pubkey(x509Req, pkey);

    if (X509_REQ_sign(x509Req, pkey, EVP_sha256()) > 0) {
        BIO* bio = BIO_new(BIO_s_mem());
        if (bio) {
            PEM_write_bio_X509_REQ(bio, x509Req);
            BUF_MEM* bptr = nullptr;
            BIO_get_mem_ptr(bio, &bptr);
            if (bptr && bptr->data && bptr->length > 0) {
                req.csrBase64 = std::string(bptr->data, bptr->length);
            }
            BIO_free(bio);
        }
    }

    X509_REQ_free(x509Req);
    EVP_PKEY_free(pkey);
    return req;
}

CertificateInformation OnvifSecurity::parseCertificateInfo(
    const std::string& certificateId, const std::string& x509DerBase64)
{
    CertificateInformation info {};
    info.certificateId = certificateId;
    info.keyAlgorithm = "RSA";
    info.validNotBefore = generateIsoTimestamp(std::chrono::seconds(0));
    info.validNotAfter = generateIsoTimestamp(std::chrono::seconds(365 * 86400));
    info.subject = "CN=" + certificateId;
    info.issuer = info.subject;

    const auto derBytes = base64Decode(x509DerBase64);
    if (!derBytes.empty()) {
        const unsigned char* p = derBytes.data();
        X509* x509 = d2i_X509(nullptr, &p, static_cast<long>(derBytes.size()));
        if (x509) {
            char subjBuf[256] {};
            X509_NAME_oneline(X509_get_subject_name(x509), subjBuf, sizeof(subjBuf));
            info.subject = subjBuf;
            char issuerBuf[256] {};
            X509_NAME_oneline(X509_get_issuer_name(x509), issuerBuf, sizeof(issuerBuf));
            info.issuer = issuerBuf;
            X509_free(x509);
        }
    }
    return info;
}

} // namespace PelcoD::Onvif
