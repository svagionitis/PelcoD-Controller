/// @file TestNetworkCrypto.cpp
/// @brief Unit tests for self-contained Base64 and SHA-1 implementations.

#include "network/Base64.h"
#include "network/Sha1.h"

#include <gtest/gtest.h>

using namespace Nmea::Network;

TEST(TestNetworkCrypto, Base64EncodingAndDecoding)
{
    // Standard test vectors
    EXPECT_EQ(Base64::encode(""), "");
    EXPECT_EQ(Base64::encode("f"), "Zg==");
    EXPECT_EQ(Base64::encode("fo"), "Zm8=");
    EXPECT_EQ(Base64::encode("foo"), "Zm9v");
    EXPECT_EQ(Base64::encode("foob"), "Zm9vYg==");
    EXPECT_EQ(Base64::encode("fooba"), "Zm9vYmE=");
    EXPECT_EQ(Base64::encode("foobar"), "Zm9vYmFy");

    // Roundtrip decoding
    const std::string text = "Maritime Ethernet Interconnect IEC 61162-460!";
    const std::string encoded = Base64::encode(text);
    const std::vector<std::uint8_t> decodedBytes = Base64::decode(encoded);
    const std::string decodedText(decodedBytes.begin(), decodedBytes.end());
    EXPECT_EQ(decodedText, text);
}

TEST(TestNetworkCrypto, Sha1Rfc3174Vectors)
{
    // RFC 3174 Test 1: "abc"
    EXPECT_EQ(Sha1::computeHex("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");

    // RFC 3174 Test 2: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    EXPECT_EQ(Sha1::computeHex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

    // Empty string
    EXPECT_EQ(Sha1::computeHex(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(TestNetworkCrypto, WebSocketAcceptHeaderRfc6455)
{
    // RFC 6455 Section 1.3 example key and expected accept token
    const std::string clientKey = "dGhlIHNhbXBsZSBub25jZQ==";
    const std::string acceptToken = Sha1::computeWebSocketAccept(clientKey);
    EXPECT_EQ(acceptToken, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}
