/// @file CredentialVault.cpp
/// @brief Implementation of authenticated AES-256-GCM credential persistence.

#include "CredentialVault.h"

#include "OnvifSecurity.h"
#include "XmlUtils.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <pugixml.hpp>

#include <array>
#include <fstream>
#include <memory>
#include <sstream>

namespace Onvif {

namespace {

    constexpr std::size_t kKeyBytes { 32U };
    constexpr std::size_t kIvBytes { 12U };
    constexpr std::size_t kTagBytes { 16U };
    constexpr int kPbkdf2Rounds { 100000 };

    struct CipherCtxDeleter {
        void operator()(EVP_CIPHER_CTX* ctx) const noexcept
        {
            if (ctx != nullptr) {
                EVP_CIPHER_CTX_free(ctx);
            }
        }
    };

    using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, CipherCtxDeleter>;

    [[nodiscard]] std::string serializeUsers(const std::vector<OnvifUser>& users)
    {
        std::ostringstream ss {};
        ss << "<users>";
        for (const auto& u : users) {
            ss << "<user name=\"" << Xml::escapeXmlAttr(u.username) << "\" level=\"" << userLevelToString(u.level)
               << "\" pass=\"" << Xml::escapeXmlAttr(u.password) << "\"/>";
        }
        ss << "</users>";
        return ss.str();
    }

    [[nodiscard]] std::vector<OnvifUser> deserializeUsers(std::string_view xml)
    {
        std::vector<OnvifUser> out {};
        pugi::xml_document doc {};
        if (!doc.load_buffer(xml.data(), xml.size())) {
            return out;
        }
        for (const auto node : doc.child("users").children("user")) {
            const std::string name { node.attribute("name").as_string() };
            const std::string levelStr { node.attribute("level").as_string() };
            const std::string pass { node.attribute("pass").as_string() };
            const OnvifUserLevel level { userLevelFromString(levelStr) };
            if (!name.empty() && level != OnvifUserLevel::Anonymous) {
                out.push_back(OnvifUser { name, pass, level });
            }
        }
        return out;
    }

} // namespace

std::string CredentialVault::deriveKey(std::string_view secret, std::string_view salt)
{
    if (secret.empty() || salt.empty()) {
        return {};
    }
    std::string key {};
    key.resize(kKeyBytes);
    const int rc { PKCS5_PBKDF2_HMAC(secret.data(), static_cast<int>(secret.size()),
        reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()), kPbkdf2Rounds, EVP_sha256(),
        static_cast<int>(kKeyBytes), reinterpret_cast<unsigned char*>(key.data())) };
    if (rc != 1) {
        return {};
    }
    return key;
}

std::string CredentialVault::encrypt(const std::vector<OnvifUser>& users, std::string_view key)
{
    if (key.size() != kKeyBytes) {
        return {};
    }

    std::string plain { serializeUsers(users) };
    if (plain.empty()) {
        return {};
    }

    std::array<unsigned char, kIvBytes> iv {};
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }

    CipherCtxPtr ctx { EVP_CIPHER_CTX_new() };
    if (!ctx) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kIvBytes), nullptr) != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.data()), iv.data())
        != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }

    std::vector<unsigned char> cipher {};
    cipher.resize(plain.size());
    int outLen { 0 };
    if (EVP_EncryptUpdate(ctx.get(), cipher.data(), &outLen, reinterpret_cast<const unsigned char*>(plain.data()),
            static_cast<int>(plain.size()))
        != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }

    int finalLen { 0 };
    if (EVP_EncryptFinal_ex(ctx.get(), cipher.data() + outLen, &finalLen) != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }
    cipher.resize(static_cast<std::size_t>(outLen + finalLen));
    OPENSSL_cleanse(plain.data(), plain.size());

    std::array<unsigned char, kTagBytes> tag {};
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagBytes), tag.data()) != 1) {
        return {};
    }

    // Envelope = IV(12) + Tag(16) + Ciphertext
    std::vector<std::uint8_t> envelope {};
    envelope.reserve(kIvBytes + kTagBytes + cipher.size());
    envelope.insert(envelope.end(), iv.begin(), iv.end());
    envelope.insert(envelope.end(), tag.begin(), tag.end());
    envelope.insert(envelope.end(), cipher.begin(), cipher.end());

    return OnvifSecurity::base64Encode(envelope);
}

std::optional<std::vector<OnvifUser>> CredentialVault::decrypt(std::string_view envelope, std::string_view key)
{
    if (key.size() != kKeyBytes || envelope.empty()) {
        return std::nullopt;
    }

    const auto rawOpt { OnvifSecurity::base64DecodeStrict(envelope) };
    if (!rawOpt.has_value()) {
        return std::nullopt;
    }
    const auto& raw { *rawOpt };
    if (raw.size() < (kIvBytes + kTagBytes)) {
        return std::nullopt;
    }

    const unsigned char* iv { raw.data() };
    const unsigned char* tag { raw.data() + kIvBytes };
    const unsigned char* cipher { raw.data() + kIvBytes + kTagBytes };
    const std::size_t cipherLen { raw.size() - kIvBytes - kTagBytes };

    CipherCtxPtr ctx { EVP_CIPHER_CTX_new() };
    if (!ctx) {
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kIvBytes), nullptr) != 1) {
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.data()), iv) != 1) {
        return std::nullopt;
    }

    std::string plain {};
    plain.resize(cipherLen);
    int outLen { 0 };
    if (EVP_DecryptUpdate(
            ctx.get(), reinterpret_cast<unsigned char*>(plain.data()), &outLen, cipher, static_cast<int>(cipherLen))
        != 1) {
        return std::nullopt;
    }

    // Set expected tag before DecryptFinal
    if (EVP_CIPHER_CTX_ctrl(
            ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagBytes), const_cast<unsigned char*>(tag))
        != 1) {
        return std::nullopt;
    }

    int finalLen { 0 };
    if (EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(plain.data()) + outLen, &finalLen) <= 0) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return std::nullopt;
    }
    plain.resize(static_cast<std::size_t>(outLen + finalLen));

    std::vector<OnvifUser> users { deserializeUsers(plain) };
    OPENSSL_cleanse(plain.data(), plain.size());
    return users;
}

bool CredentialVault::saveToFile(const std::string& path, const std::vector<OnvifUser>& users, std::string_view key)
{
    const std::string envelope { encrypt(users, key) };
    if (envelope.empty()) {
        return false;
    }
    std::ofstream ofs { path, std::ios::binary | std::ios::trunc };
    if (!ofs.is_open()) {
        return false;
    }
    ofs << envelope;
    return ofs.good();
}

std::optional<std::vector<OnvifUser>> CredentialVault::loadFromFile(const std::string& path, std::string_view key)
{
    std::ifstream ifs { path, std::ios::binary };
    if (!ifs.is_open()) {
        return std::nullopt;
    }
    std::string envelope { (std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>() };
    return decrypt(envelope, key);
}

} // namespace Onvif
