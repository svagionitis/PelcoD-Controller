#include "OnvifSecurity.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

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

std::vector<uint8_t> OnvifSecurity::generateNonce()
{
    constexpr size_t kNonceLength { 16 };
    std::vector<uint8_t> nonce(kNonceLength, 0);
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

std::string OnvifSecurity::base64Encode(const std::vector<uint8_t>& data)
{
    std::string result {};
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i { 0 };
    while (i < data.size()) {
        const uint32_t octetA = (i < data.size()) ? static_cast<uint8_t>(data[i++]) : 0;
        const uint32_t octetB = (i < data.size()) ? static_cast<uint8_t>(data[i++]) : 0;
        const uint32_t octetC = (i < data.size()) ? static_cast<uint8_t>(data[i++]) : 0;

        const uint32_t triple = (octetA << 16) + (octetB << 8) + octetC;

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
    const std::vector<uint8_t> data(text.begin(), text.end());
    return base64Encode(data);
}

std::string OnvifSecurity::computePasswordDigest(
    const std::vector<uint8_t>& rawNonce, const std::string& createdUtc, const std::string& password)
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

    const std::vector<uint8_t> digestBytes(md.begin(), md.begin() + mdLen);
    return base64Encode(digestBytes);
}

UsernameTokenData OnvifSecurity::createTokenData(const SecurityCredentials& credentials)
{
    UsernameTokenData data {};
    data.username = credentials.username;
    if (data.username.empty()) {
        return data;
    }

    const std::vector<uint8_t> rawNonce = generateNonce();
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

} // namespace PelcoD::Onvif
