#include "OnvifSecurity.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/buffer.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "OpenSslRaII.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {

    constexpr std::string_view kBase64Alphabet { "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                 "abcdefghijklmnopqrstuvwxyz"
                                                 "0123456789+/" };

    constexpr std::int64_t kMinSafeDays { 1 };
    constexpr std::int64_t kSecondsPerDay { 86400 };
    constexpr std::int64_t kMaxSafeDays { 36500 }; // 100 years max

    [[nodiscard]] std::int64_t safeValiditySeconds(int daysValid) noexcept
    {
        const auto days = std::clamp(static_cast<std::int64_t>(daysValid), kMinSafeDays, kMaxSafeDays);
        return days * kSecondsPerDay;
    }

    [[nodiscard]] bool setRandomSerialNumber(X509* cert) noexcept
    {
        if (cert == nullptr) {
            return false;
        }
        const Crypto::BignumPtr bn { BN_new() };
        if (!bn) {
            return false;
        }
        if (BN_rand(bn.get(), 64, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) != 1) {
            return false;
        }
        ASN1_INTEGER* serial = X509_get_serialNumber(cert);
        if (serial == nullptr) {
            return false;
        }
        return (BN_to_ASN1_INTEGER(bn.get(), serial) != nullptr);
    }

    [[nodiscard]] std::string asn1TimeToIso(const ASN1_TIME* tm)
    {
        if (tm == nullptr) {
            return { };
        }
        struct tm timeInfo { };
        if (ASN1_TIME_to_tm(tm, &timeInfo) != 1) {
            return { };
        }
        std::array<char, 32> buf { };
        const int written
            = std::snprintf(buf.data(), buf.size(), "%04d-%02d-%02dT%02d:%02d:%02dZ", timeInfo.tm_year + 1900,
                timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
        if (written <= 0 || static_cast<std::size_t>(written) >= buf.size()) {
            return { };
        }
        return std::string(buf.data(), static_cast<std::size_t>(written));
    }

} // namespace

std::vector<std::uint8_t> OnvifSecurity::generateNonce()
{
    constexpr std::size_t kNonceLength { 16U };
    std::vector<std::uint8_t> nonce(kNonceLength, std::uint8_t { 0U });
    if (RAND_bytes(nonce.data(), static_cast<int>(kNonceLength)) != 1) {
        return { };
    }
    return nonce;
}

std::string OnvifSecurity::generateIsoTimestamp(std::chrono::seconds offset)
{
    const auto now = std::chrono::system_clock::now() + offset;
    const std::time_t timeT = std::chrono::system_clock::to_time_t(now);

    std::tm utcTm { };
#if defined(_WIN32)
    gmtime_s(&utcTm, &timeT);
#else
    gmtime_r(&timeT, &utcTm);
#endif

    std::ostringstream ss { };
    ss << std::put_time(&utcTm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string OnvifSecurity::base64Encode(const std::vector<std::uint8_t>& data)
{
    std::string result { };
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i { 0 };
    while (i < data.size()) {
        const std::uint32_t octetA = (i < data.size()) ? static_cast<std::uint32_t>(data[i++]) : 0U;
        const std::uint32_t octetB = (i < data.size()) ? static_cast<std::uint32_t>(data[i++]) : 0U;
        const std::uint32_t octetC = (i < data.size()) ? static_cast<std::uint32_t>(data[i++]) : 0U;

        const std::uint32_t triple = (octetA << 16) + (octetB << 8) + octetC;

        result.push_back(kBase64Alphabet.at(static_cast<std::size_t>((triple >> 18U) & 0x3FU)));
        result.push_back(kBase64Alphabet.at(static_cast<std::size_t>((triple >> 12U) & 0x3FU)));
        result.push_back(kBase64Alphabet.at(static_cast<std::size_t>((triple >> 6U) & 0x3FU)));
        result.push_back(kBase64Alphabet.at(static_cast<std::size_t>(triple & 0x3FU)));
    }

    const std::size_t mod = data.size() % 3U;
    if (mod == 1U) {
        result[result.size() - 1U] = '=';
        result[result.size() - 2U] = '=';
    } else if (mod == 2U) {
        result[result.size() - 1U] = '=';
    }

    return result;
}

std::string OnvifSecurity::base64Encode(const std::string& text)
{
    std::vector<std::uint8_t> data { };
    data.reserve(text.size());
    for (const char ch : text) {
        data.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(ch)));
    }
    return base64Encode(data);
}

std::string OnvifSecurity::computePasswordDigest(
    const std::vector<std::uint8_t>& rawNonce, const std::string& createdUtc, const std::string& password)
{
    const Crypto::EvpMdCtxPtr ctx { EVP_MD_CTX_new() };
    if (!ctx) {
        return { };
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha1(), nullptr) != 1) {
        return { };
    }

    if (!rawNonce.empty() && (EVP_DigestUpdate(ctx.get(), rawNonce.data(), rawNonce.size()) != 1)) {
        return { };
    }
    if (!createdUtc.empty() && (EVP_DigestUpdate(ctx.get(), createdUtc.data(), createdUtc.size()) != 1)) {
        return { };
    }
    if (!password.empty() && (EVP_DigestUpdate(ctx.get(), password.data(), password.size()) != 1)) {
        return { };
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> md { };
    unsigned int mdLen { 0U };
    if (EVP_DigestFinal_ex(ctx.get(), md.data(), &mdLen) != 1) {
        return { };
    }

    const std::vector<std::uint8_t> digestBytes(md.begin(), md.begin() + mdLen);
    return base64Encode(digestBytes);
}

UsernameTokenData OnvifSecurity::createTokenData(const SecurityCredentials& credentials)
{
    UsernameTokenData data { };
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
        return { };
    }

    const UsernameTokenData token = createTokenData(credentials);

    std::ostringstream ss { };
    ss << "<wsse:Security "
          "xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
       << "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">\n"
       << "  <wsse:UsernameToken>\n"
       << "    <wsse:Username>" << Xml::escapeXml(token.username) << "</wsse:Username>\n"
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
        table[static_cast<unsigned char>(kBase64Alphabet[static_cast<std::size_t>(i)])] = i;
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

namespace {

    /// @brief Maps a Base64 alphabet character to its 6-bit value.
    /// @return Value in [0, 63], or -1 if the character is not in the alphabet.
    [[nodiscard]] int base64Value(char ch) noexcept
    {
        if ((ch >= 'A') && (ch <= 'Z')) {
            return ch - 'A';
        }
        if ((ch >= 'a') && (ch <= 'z')) {
            return (ch - 'a') + 26;
        }
        if ((ch >= '0') && (ch <= '9')) {
            return (ch - '0') + 52;
        }
        if (ch == '+') {
            return 62;
        }
        if (ch == '/') {
            return 63;
        }
        return -1;
    }

    /// @brief Parses a fixed-width run of decimal digits.
    [[nodiscard]] bool parseFixed(std::string_view s, std::size_t pos, std::size_t count, std::int64_t& out) noexcept
    {
        if ((pos > s.size()) || (count > (s.size() - pos))) {
            return false;
        }
        std::int64_t value { 0 };
        for (std::size_t i { pos }; i < (pos + count); ++i) {
            const char ch { s[i] };
            if ((ch < '0') || (ch > '9')) {
                return false;
            }
            value = (value * 10) + static_cast<std::int64_t>(ch - '0');
        }
        out = value;
        return true;
    }

    /// @brief Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's algorithm).
    [[nodiscard]] std::int64_t daysFromCivil(std::int64_t y, std::int64_t m, std::int64_t d) noexcept
    {
        const std::int64_t yy { (m <= 2) ? (y - 1) : y };
        const std::int64_t era { ((yy >= 0) ? yy : (yy - 399)) / 400 };
        const std::int64_t yoe { yy - (era * 400) };
        const std::int64_t mp { (m > 2) ? (m - 3) : (m + 9) };
        const std::int64_t doy { (((153 * mp) + 2) / 5) + (d - 1) };
        const std::int64_t doe { (yoe * 365) + (yoe / 4) - (yoe / 100) + doy };
        return (era * 146097) + doe - 719468;
    }

    /// @brief Number of days in a month of a given year.
    [[nodiscard]] std::int64_t daysInMonth(std::int64_t y, std::int64_t m) noexcept
    {
        constexpr std::array<std::int64_t, 12U> kDays { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        const bool leap { ((y % 4) == 0) && (((y % 100) != 0) || ((y % 400) == 0)) };
        if ((m == 2) && leap) {
            return 29;
        }
        return kDays.at(static_cast<std::size_t>(m - 1));
    }

} // namespace

std::optional<std::vector<std::uint8_t>> OnvifSecurity::base64DecodeStrict(std::string_view text)
{
    if ((text.size() % 4U) != 0U) {
        return std::nullopt;
    }
    std::size_t pad { 0U };
    if (!text.empty() && (text.back() == '=')) {
        pad = ((text.size() >= 2U) && (text[text.size() - 2U] == '=')) ? 2U : 1U;
    }

    std::vector<std::uint8_t> out { };
    out.reserve((text.size() / 4U) * 3U);
    for (std::size_t i { 0U }; i < text.size(); i += 4U) {
        const bool lastQuad { (i + 4U) == text.size() };
        std::uint32_t acc { 0U };
        for (std::size_t j { 0U }; j < 4U; ++j) {
            const char ch { text[i + j] };
            std::uint32_t v { 0U };
            if (ch == '=') {
                if (!lastQuad || (j < (4U - pad))) {
                    return std::nullopt;
                }
            } else {
                const int iv { base64Value(ch) };
                if (iv < 0) {
                    return std::nullopt;
                }
                v = static_cast<std::uint32_t>(iv);
            }
            acc = (acc << 6U) | v;
        }
        out.push_back(static_cast<std::uint8_t>((acc >> 16U) & 0xFFU));
        if (!lastQuad || (pad < 2U)) {
            out.push_back(static_cast<std::uint8_t>((acc >> 8U) & 0xFFU));
        }
        if (!lastQuad || (pad < 1U)) {
            out.push_back(static_cast<std::uint8_t>(acc & 0xFFU));
        }
    }
    return out;
}

std::optional<std::chrono::system_clock::time_point> OnvifSecurity::parseIsoUtc(std::string_view text)
{
    constexpr std::size_t kBaseLength { 19U }; // YYYY-MM-DDThh:mm:ss
    if ((text.size() < kBaseLength) || (text.size() > 64U)) {
        return std::nullopt;
    }
    std::int64_t year { 0 };
    std::int64_t month { 0 };
    std::int64_t day { 0 };
    std::int64_t hour { 0 };
    std::int64_t minute { 0 };
    std::int64_t second { 0 };
    const bool fieldsOk { parseFixed(text, 0U, 4U, year) && (text[4] == '-') && parseFixed(text, 5U, 2U, month)
        && (text[7] == '-') && parseFixed(text, 8U, 2U, day) && ((text[10] == 'T') || (text[10] == 't'))
        && parseFixed(text, 11U, 2U, hour) && (text[13] == ':') && parseFixed(text, 14U, 2U, minute)
        && (text[16] == ':') && parseFixed(text, 17U, 2U, second) };
    if (!fieldsOk || (year < 1) || (month < 1) || (month > 12) || (day < 1) || (day > daysInMonth(year, month))
        || (hour > 23) || (minute > 59) || (second > 59)) {
        return std::nullopt;
    }

    std::size_t pos { kBaseLength };
    if ((pos < text.size()) && (text[pos] == '.')) {
        ++pos;
        const std::size_t fracStart { pos };
        while ((pos < text.size()) && (text[pos] >= '0') && (text[pos] <= '9')) {
            ++pos;
        }
        if (pos == fracStart) {
            return std::nullopt;
        }
    }

    std::int64_t offsetSeconds { 0 };
    if (pos < text.size()) {
        const char zone { text[pos] };
        if ((zone == 'Z') || (zone == 'z')) {
            ++pos;
        } else if ((zone == '+') || (zone == '-')) {
            std::int64_t oh { 0 };
            std::int64_t om { 0 };
            if (!parseFixed(text, pos + 1U, 2U, oh) || ((pos + 3U) >= text.size()) || (text[pos + 3U] != ':')
                || !parseFixed(text, pos + 4U, 2U, om) || (oh > 14) || (om > 59)) {
                return std::nullopt;
            }
            offsetSeconds = ((oh * 3600) + (om * 60)) * ((zone == '-') ? -1 : 1);
            pos += 6U;
        } else {
            return std::nullopt;
        }
    }
    if (pos != text.size()) {
        return std::nullopt;
    }

    const std::int64_t epochSeconds { (daysFromCivil(year, month, day) * 86400) + (hour * 3600) + (minute * 60) + second
        - offsetSeconds };
    return std::chrono::system_clock::from_time_t(0) + std::chrono::seconds { epochSeconds };
}

OnvifCertificate OnvifSecurity::generateSelfSignedCertificate(
    const std::string& certificateId, const std::string& subject, int daysValid)
{
    OnvifCertificate cert { };
    cert.certificateId = certificateId;
    cert.info.certificateId = certificateId;
    cert.info.subject = subject.empty() ? ("CN=PelcoD-" + certificateId) : subject;
    cert.info.issuer = cert.info.subject;
    cert.info.keyAlgorithm = "RSA";
    cert.info.isDefault = true;

    const std::int64_t validSecs = safeValiditySeconds(daysValid);
    cert.info.validNotBefore = generateIsoTimestamp(std::chrono::seconds(0));
    cert.info.validNotAfter = generateIsoTimestamp(std::chrono::seconds(validSecs));

    const Crypto::EvpPkeyCtxPtr pctx { EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr) };
    if (!pctx) {
        return cert;
    }
    if ((EVP_PKEY_keygen_init(pctx.get()) <= 0) || (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx.get(), 2048) <= 0)) {
        return cert;
    }
    EVP_PKEY* rawPkey = nullptr;
    if ((EVP_PKEY_keygen(pctx.get(), &rawPkey) <= 0) || (rawPkey == nullptr)) {
        return cert;
    }
    const Crypto::EvpPkeyPtr pkey { rawPkey };

    const Crypto::X509Ptr x509 { X509_new() };
    if (!x509) {
        return cert;
    }
    if (X509_set_version(x509.get(), 2L) != 1) {
        return cert;
    }
    if (!setRandomSerialNumber(x509.get())) {
        (void)ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1L);
    }
    if (X509_gmtime_adj(X509_get_notBefore(x509.get()), 0L) == nullptr) {
        return cert;
    }
    if (X509_gmtime_adj(X509_get_notAfter(x509.get()), static_cast<long>(validSecs)) == nullptr) {
        return cert;
    }
    if (X509_set_pubkey(x509.get(), pkey.get()) != 1) {
        return cert;
    }

    X509_NAME* name = X509_get_subject_name(x509.get());
    if (name == nullptr) {
        return cert;
    }
    if (X509_NAME_add_entry_by_txt(
            name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(cert.info.subject.c_str()), -1, -1, 0)
        != 1) {
        return cert;
    }
    if (X509_set_issuer_name(x509.get(), name) != 1) {
        return cert;
    }

    if (X509_sign(x509.get(), pkey.get(), EVP_sha256()) <= 0) {
        return cert;
    }

    unsigned char* der = nullptr;
    const int derLen = i2d_X509(x509.get(), &der);
    if (derLen > 0 && der != nullptr) {
        const Crypto::OpenSslMemPtr derGuard { der };
        std::vector<std::uint8_t> derBytes(der, der + derLen);
        cert.x509DerBase64 = base64Encode(derBytes);
    }

    return cert;
}

Pkcs10Request OnvifSecurity::generatePkcs10Csr(const std::string& certificateId, const std::string& subject)
{
    Pkcs10Request req { };
    req.certificateId = certificateId;
    req.subject = subject.empty() ? ("CN=PelcoD-" + certificateId) : subject;

    const Crypto::EvpPkeyCtxPtr pctx { EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr) };
    if (!pctx) {
        return req;
    }
    if ((EVP_PKEY_keygen_init(pctx.get()) <= 0) || (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx.get(), 2048) <= 0)) {
        return req;
    }
    EVP_PKEY* rawPkey = nullptr;
    if ((EVP_PKEY_keygen(pctx.get(), &rawPkey) <= 0) || (rawPkey == nullptr)) {
        return req;
    }
    const Crypto::EvpPkeyPtr pkey { rawPkey };

    const Crypto::X509ReqPtr x509Req { X509_REQ_new() };
    if (!x509Req) {
        return req;
    }
    if (X509_REQ_set_version(x509Req.get(), 0L) != 1) {
        return req;
    }
    X509_NAME* name = X509_REQ_get_subject_name(x509Req.get());
    if (name == nullptr) {
        return req;
    }
    if (X509_NAME_add_entry_by_txt(
            name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(req.subject.c_str()), -1, -1, 0)
        != 1) {
        return req;
    }
    if (X509_REQ_set_pubkey(x509Req.get(), pkey.get()) != 1) {
        return req;
    }

    if (X509_REQ_sign(x509Req.get(), pkey.get(), EVP_sha256()) <= 0) {
        return req;
    }

    const Crypto::BioPtr bio { BIO_new(BIO_s_mem()) };
    if (bio) {
        if (PEM_write_bio_X509_REQ(bio.get(), x509Req.get()) == 1) {
            BUF_MEM* bptr = nullptr;
            BIO_get_mem_ptr(bio.get(), &bptr);
            if ((bptr != nullptr) && (bptr->data != nullptr) && (bptr->length > 0U)) {
                req.csrBase64 = std::string(bptr->data, bptr->length);
            }
        }
    }

    return req;
}

CertificateInformation OnvifSecurity::parseCertificateInfo(
    const std::string& certificateId, const std::string& x509DerBase64)
{
    CertificateInformation info { };
    info.certificateId = certificateId;
    info.keyAlgorithm = "RSA";
    info.validNotBefore = generateIsoTimestamp(std::chrono::seconds(0));
    info.validNotAfter = generateIsoTimestamp(std::chrono::seconds(365 * 86400));
    info.subject = "CN=" + certificateId;
    info.issuer = info.subject;

    const auto derBytes = base64Decode(x509DerBase64);
    if (!derBytes.empty()) {
        const unsigned char* p = derBytes.data();
        const Crypto::X509Ptr x509 { d2i_X509(nullptr, &p, static_cast<long>(derBytes.size())) };
        if (x509) {
            std::array<char, 256> subjBuf { };
            if (X509_NAME_oneline(X509_get_subject_name(x509.get()), subjBuf.data(), static_cast<int>(subjBuf.size()))
                != nullptr) {
                info.subject = subjBuf.data();
            }
            std::array<char, 256> issuerBuf { };
            if (X509_NAME_oneline(
                    X509_get_issuer_name(x509.get()), issuerBuf.data(), static_cast<int>(issuerBuf.size()))
                != nullptr) {
                info.issuer = issuerBuf.data();
            }
            const std::string notBeforeStr = asn1TimeToIso(X509_get0_notBefore(x509.get()));
            if (!notBeforeStr.empty()) {
                info.validNotBefore = notBeforeStr;
            }
            const std::string notAfterStr = asn1TimeToIso(X509_get0_notAfter(x509.get()));
            if (!notAfterStr.empty()) {
                info.validNotAfter = notAfterStr;
            }
        }
    }
    return info;
}

} // namespace Onvif
