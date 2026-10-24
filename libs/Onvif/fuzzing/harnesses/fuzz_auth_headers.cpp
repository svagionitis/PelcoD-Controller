/// @file fuzz_auth_headers.cpp
/// @brief libFuzzer harness for WS-Security UsernameToken and HTTP Digest authorization headers.

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "CredentialStore.h"
#include "HttpDigest.h"
#include "NonceCache.h"
#include "OnvifAuthTypes.h"
#include "OnvifTypes.h"
#include "UsernameToken.h"

namespace {

constexpr std::size_t kMaxHeaderBytes { 8192U };

struct AuthFixture {
    std::shared_ptr<Onvif::CredentialStore> store { };
    std::shared_ptr<Onvif::NonceCache> cache { };
    std::unique_ptr<Onvif::UsernameTokenValidator> userValidator { };
    std::unique_ptr<Onvif::DigestValidator> digestValidator { };

    AuthFixture()
    {
        std::vector<Onvif::OnvifUser> seedUsers;
        Onvif::OnvifUser admin { };
        admin.username = "admin";
        admin.password = "AdminPass123!";
        admin.level = Onvif::OnvifUserLevel::Administrator;
        seedUsers.push_back(admin);

        Onvif::OnvifUser op { };
        op.username = "operator";
        op.password = "OperatorPass123!";
        op.level = Onvif::OnvifUserLevel::Operator;
        seedUsers.push_back(op);

        store = std::make_shared<Onvif::CredentialStore>(seedUsers);
        cache = std::make_shared<Onvif::NonceCache>(4096U, std::chrono::seconds(300));

        Onvif::OnvifAuthConfig cfg { };
        cfg.realm = "PelcoD-ONVIF";
        cfg.maxClockSkew = std::chrono::seconds(300);
        cfg.allowPasswordText = false;

        userValidator = std::make_unique<Onvif::UsernameTokenValidator>(store, cache, cfg);
        digestValidator = std::make_unique<Onvif::DigestValidator>(store, cache, cfg);
    }
};

const AuthFixture& getFixture()
{
    static const AuthFixture fixture;
    return fixture;
}

} // namespace

extern "C" int FuzzAuthHeadersTest(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0U || size > kMaxHeaderBytes) {
        return 0;
    }

    const auto& fixture = getFixture();
    const auto now = std::chrono::system_clock::now();
    const std::string_view input(reinterpret_cast<const char*>(data), size);

    // 1. Fuzz HTTP Digest Validator with candidate Authorization header
    static_cast<void>(fixture.digestValidator->validate(input, "POST", "/onvif/device_service", now));

    // 2. Fuzz WS-Security XML Parser
    pugi::xml_document doc;
    if (doc.load_buffer(data, size, pugi::parse_default, pugi::encoding_utf8)) {
        const pugi::xml_node secHeader = Onvif::findSecurityHeader(doc);
        if (secHeader) {
            static_cast<void>(fixture.userValidator->validate(secHeader, now));
        }
    }

    return 0;
}

#if !defined(FUZZ_STANDALONE_REGRESSION)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return FuzzAuthHeadersTest(data, size);
}
#endif
