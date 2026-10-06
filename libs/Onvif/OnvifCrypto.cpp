/// @file OnvifCrypto.cpp
/// @brief Implementation of the internal OpenSSL RAII helpers.

#include "OnvifCrypto.h"

#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <array>
#include <limits>
#include <memory>

namespace Onvif::Crypto {

namespace {

    /// @brief Deleter releasing an EVP_MD_CTX.
    struct MdCtxDeleter {
        void operator()(EVP_MD_CTX* ctx) const noexcept
        {
            EVP_MD_CTX_free(ctx);
        }
    };

    using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, MdCtxDeleter>;

    constexpr std::array<char, 16U> kHex { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
        'e', 'f' };

} // namespace

std::optional<std::string> hexDigest(const EVP_MD* md, std::string_view input)
{
    const MdCtxPtr ctx { EVP_MD_CTX_new() };
    if ((ctx == nullptr) || (md == nullptr)) {
        return std::nullopt;
    }
    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1) {
        return std::nullopt;
    }
    if (!input.empty() && (EVP_DigestUpdate(ctx.get(), input.data(), input.size()) != 1)) {
        return std::nullopt;
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> out {};
    unsigned int outLen { 0U };
    if (EVP_DigestFinal_ex(ctx.get(), out.data(), &outLen) != 1) {
        return std::nullopt;
    }
    std::string hex {};
    hex.reserve(static_cast<std::size_t>(outLen) * 2U);
    for (std::size_t i { 0U }; i < static_cast<std::size_t>(outLen); ++i) {
        const unsigned int byte { static_cast<unsigned int>(out.at(i)) };
        hex.push_back(kHex.at(static_cast<std::size_t>(byte >> 4U)));
        hex.push_back(kHex.at(static_cast<std::size_t>(byte & 0x0FU)));
    }
    return hex;
}

std::vector<std::uint8_t> hmacSha256(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data)
{
    if (key.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> out {};
    unsigned int outLen { 0U };
    const unsigned char* result { HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data.data(),
        data.size(), out.data(), &outLen) };
    if (result == nullptr) {
        return {};
    }
    return std::vector<std::uint8_t>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(outLen));
}

bool constTimeEqual(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) {
        return false;
    }
    if (a.empty()) {
        return true;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

bool constTimeEqual(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) noexcept
{
    if (a.size() != b.size()) {
        return false;
    }
    if (a.empty()) {
        return true;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::vector<std::uint8_t> randomBytes(std::size_t count)
{
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    std::vector<std::uint8_t> buf(count, std::uint8_t { 0U });
    if ((count > 0U) && (RAND_bytes(buf.data(), static_cast<int>(count)) != 1)) {
        return {};
    }
    return buf;
}

} // namespace Onvif::Crypto
