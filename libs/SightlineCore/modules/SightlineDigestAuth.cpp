#include "SightlineDigestAuth.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>

namespace Sightline {

namespace {

    // RFC 1321 MD5 Transformations and Constants
    constexpr std::array<std::uint32_t, 64> kTable
        = { 0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
              0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU, 0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
              0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU, 0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
              0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
              0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU, 0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
              0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U, 0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
              0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
              0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U, 0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U };

    constexpr std::array<std::uint32_t, 64> kShifts
        = { 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 5U, 9U, 14U, 20U, 5U, 9U, 14U,
              20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U,
              16U, 23U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U };

    [[nodiscard]] constexpr std::uint32_t leftRotate(std::uint32_t value, std::uint32_t count) noexcept
    {
        return (value << count) | (value >> (32U - count));
    }

    void processMd5Block(const std::uint8_t* block, std::array<std::uint32_t, 4>& state) noexcept
    {
        std::array<std::uint32_t, 16> words {};
        for (std::size_t i { 0U }; i < 16U; ++i) {
            const std::size_t offset { i * 4U };
            words[i] = static_cast<std::uint32_t>(block[offset])
                | (static_cast<std::uint32_t>(block[offset + 1U]) << 8U)
                | (static_cast<std::uint32_t>(block[offset + 2U]) << 16U)
                | (static_cast<std::uint32_t>(block[offset + 3U]) << 24U);
        }

        std::uint32_t a { state[0] };
        std::uint32_t b { state[1] };
        std::uint32_t c { state[2] };
        std::uint32_t d { state[3] };

        for (std::size_t i { 0U }; i < 64U; ++i) {
            std::uint32_t f { 0U };
            std::size_t g { 0U };

            if (i < 16U) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32U) {
                f = (d & b) | ((~d) & c);
                g = (5U * i + 1U) % 16U;
            } else if (i < 48U) {
                f = b ^ c ^ d;
                g = (3U * i + 5U) % 16U;
            } else {
                f = c ^ (b | (~d));
                g = (7U * i) % 16U;
            }

            const std::uint32_t temp { d };
            d = c;
            c = b;
            b = b + leftRotate(a + f + kTable[i] + words[g], kShifts[i]);
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

    [[nodiscard]] std::string extractQuotedValue(std::string_view source, std::string_view key)
    {
        const std::size_t keyPos { source.find(key) };
        if (keyPos == std::string_view::npos) {
            return {};
        }

        const std::size_t eqPos { source.find('=', keyPos + key.size()) };
        if (eqPos == std::string_view::npos) {
            return {};
        }

        std::size_t valStart { eqPos + 1U };
        while (valStart < source.size() && (source[valStart] == ' ' || source[valStart] == '\t')) {
            ++valStart;
        }

        if (valStart >= source.size()) {
            return {};
        }

        if (source[valStart] == '"') {
            ++valStart;
            const std::size_t valEnd { source.find('"', valStart) };
            if (valEnd != std::string_view::npos) {
                return std::string(source.substr(valStart, valEnd - valStart));
            }
            return std::string(source.substr(valStart));
        }

        std::size_t valEnd { valStart };
        while (valEnd < source.size() && source[valEnd] != ',' && source[valEnd] != ' ' && source[valEnd] != '\r'
            && source[valEnd] != '\n') {
            ++valEnd;
        }
        return std::string(source.substr(valStart, valEnd - valStart));
    }

} // namespace

std::string SightlineDigestAuth::computeMd5Hex(std::string_view data)
{
    std::array<std::uint32_t, 4> state = { 0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U };

    const std::uint64_t bitLength { static_cast<std::uint64_t>(data.size()) * 8U };
    const std::size_t initialSize { data.size() };
    const std::size_t paddingLength { (initialSize % 64U < 56U) ? (56U - (initialSize % 64U))
                                                                : (120U - (initialSize % 64U)) };

    std::vector<std::uint8_t> buffer(initialSize + paddingLength + 8U, 0U);
    if (!data.empty()) {
        std::memcpy(buffer.data(), data.data(), data.size());
    }

    buffer[initialSize] = 0x80U;

    for (std::size_t i { 0U }; i < 8U; ++i) {
        buffer[initialSize + paddingLength + i] = static_cast<std::uint8_t>((bitLength >> (i * 8U)) & 0xffU);
    }

    for (std::size_t offset { 0U }; offset < buffer.size(); offset += 64U) {
        processMd5Block(buffer.data() + offset, state);
    }

    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string hexResult {};
    hexResult.reserve(32U);

    for (std::size_t i { 0U }; i < 4U; ++i) {
        for (std::size_t byteIdx { 0U }; byteIdx < 4U; ++byteIdx) {
            const auto byteVal = static_cast<std::uint8_t>((state[i] >> (byteIdx * 8U)) & 0xffU);
            hexResult.push_back(kHexDigits[(byteVal >> 4U) & 0x0fU]);
            hexResult.push_back(kHexDigits[byteVal & 0x0fU]);
        }
    }

    return hexResult;
}

std::string SightlineDigestAuth::computeHa1(std::string_view user, std::string_view realm, std::string_view pass)
{
    std::string composite {};
    composite.reserve(user.size() + realm.size() + pass.size() + 2U);
    composite.append(user);
    composite.push_back(':');
    composite.append(realm);
    composite.push_back(':');
    composite.append(pass);
    return computeMd5Hex(composite);
}

std::string SightlineDigestAuth::computeHa2(std::string_view method, std::string_view uri)
{
    std::string composite {};
    composite.reserve(method.size() + uri.size() + 1U);
    composite.append(method);
    composite.push_back(':');
    composite.append(uri);
    return computeMd5Hex(composite);
}

std::string SightlineDigestAuth::computeResponse(std::string_view ha1, std::string_view nonce, std::string_view ha2)
{
    std::string composite {};
    composite.reserve(ha1.size() + nonce.size() + ha2.size() + 2U);
    composite.append(ha1);
    composite.push_back(':');
    composite.append(nonce);
    composite.push_back(':');
    composite.append(ha2);
    return computeMd5Hex(composite);
}

std::string SightlineDigestAuth::formatHtpasswdLine(
    std::string_view user, std::string_view realm, std::string_view pass)
{
    std::string line {};
    const std::string ha1 { computeHa1(user, realm, pass) };
    line.reserve(user.size() + realm.size() + ha1.size() + 2U);
    line.append(user);
    line.push_back(':');
    line.append(realm);
    line.push_back(':');
    line.append(ha1);
    return line;
}

std::vector<HtpasswdEntry> SightlineDigestAuth::parseHtpasswd(std::string_view content)
{
    std::vector<HtpasswdEntry> entries {};
    std::size_t startPos { 0U };

    while (startPos < content.size()) {
        std::size_t endPos { content.find('\n', startPos) };
        if (endPos == std::string_view::npos) {
            endPos = content.size();
        }

        std::string_view line { content.substr(startPos, endPos - startPos) };
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        // Ignore empty lines and comment lines
        if (!line.empty() && line.front() != '#') {
            const std::size_t firstColon { line.find(':') };
            if (firstColon != std::string_view::npos) {
                const std::size_t secondColon { line.find(':', firstColon + 1U) };
                if (secondColon != std::string_view::npos) {
                    HtpasswdEntry entry {};
                    entry.username = std::string(line.substr(0U, firstColon));
                    entry.realm = std::string(line.substr(firstColon + 1U, secondColon - (firstColon + 1U)));
                    entry.ha1 = std::string(line.substr(secondColon + 1U));

                    // Transform hash to lowercase if needed
                    std::transform(entry.ha1.begin(), entry.ha1.end(), entry.ha1.begin(),
                        [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });

                    entries.push_back(std::move(entry));
                }
            }
        }

        startPos = endPos + 1U;
    }

    return entries;
}

std::optional<HtpasswdEntry> SightlineDigestAuth::findHtpasswdUser(
    const std::vector<HtpasswdEntry>& entries, std::string_view user)
{
    for (const auto& entry : entries) {
        if (entry.username == user) {
            return entry;
        }
    }
    return std::nullopt;
}

DigestChallenge SightlineDigestAuth::parseDigestChallenge(std::string_view header)
{
    DigestChallenge challenge {};
    challenge.realm = extractQuotedValue(header, "realm");
    challenge.nonce = extractQuotedValue(header, "nonce");
    challenge.opaque = extractQuotedValue(header, "opaque");

    const std::string algo { extractQuotedValue(header, "algorithm") };
    if (!algo.empty()) {
        challenge.algorithm = algo;
    }

    return challenge;
}

std::string SightlineDigestAuth::formatDigestAuthHeader(const DigestResponseParams& params)
{
    std::ostringstream ss {};
    ss << "Digest username=\"" << params.username << "\", "
       << "realm=\"" << params.realm << "\", "
       << "nonce=\"" << params.nonce << "\", "
       << "uri=\"" << params.uri << "\", "
       << "response=\"" << params.response << "\"";

    if (!params.opaque.empty()) {
        ss << ", opaque=\"" << params.opaque << "\"";
    }

    return ss.str();
}

std::string SightlineDigestAuth::sanitizeRtspUri(std::string_view uri)
{
    const std::size_t schemePos { uri.find("://") };
    if (schemePos == std::string_view::npos) {
        return std::string(uri);
    }

    const std::size_t atPos { uri.find('@', schemePos + 3U) };
    if (atPos == std::string_view::npos) {
        return std::string(uri);
    }

    const std::size_t colonPos { uri.find(':', schemePos + 3U) };
    if (colonPos != std::string_view::npos && colonPos < atPos) {
        std::string sanitized {};
        sanitized.reserve(uri.size());
        sanitized.append(uri.substr(0U, colonPos + 1U));
        sanitized.append("***");
        sanitized.append(uri.substr(atPos));
        return sanitized;
    }

    return std::string(uri);
}

} // namespace Sightline
