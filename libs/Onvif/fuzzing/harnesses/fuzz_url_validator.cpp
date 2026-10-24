/// @file fuzz_url_validator.cpp
/// @brief libFuzzer harness for SSRF Notification URL validation.

#include <cstddef>
#include <cstdint>

#include <string_view>

#include "NotificationUrlValidator.h"
#include "OnvifServerTypes.h"

namespace {

constexpr std::size_t kMaxUrlBytes { 4096U };

[[nodiscard]] Onvif::NotificationConfig createFuzzConfig()
{
    Onvif::NotificationConfig cfg { };
    cfg.allowLoopback = false;
    cfg.allowPrivateSubnets = false;
    cfg.allowedHosts = { "notifications.internal.domain", "192.168.1.200" };
    cfg.blockedHosts = { "malicious.internal.domain" };
    return cfg;
}

} // namespace

extern "C" int FuzzUrlValidatorTest(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0U || size > kMaxUrlBytes) {
        return 0;
    }

    const std::string_view candidateUrl(reinterpret_cast<const char*>(data), size);
    static const Onvif::NotificationConfig config = createFuzzConfig();

    // 1. Validate with loopback disabled
    const Onvif::UrlCheckResult res1 = Onvif::NotificationUrlValidator::validateUrl(candidateUrl, config, false);
    static_cast<void>(res1.isValid());

    // 2. Validate with server bound to loopback
    const Onvif::UrlCheckResult res2 = Onvif::NotificationUrlValidator::validateUrl(candidateUrl, config, true);
    static_cast<void>(res2.isValid());

    // 3. Test standalone host classifier
    static_cast<void>(Onvif::NotificationUrlValidator::isLoopbackHost(candidateUrl));

    return 0;
}

#if !defined(FUZZ_STANDALONE_REGRESSION)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return FuzzUrlValidatorTest(data, size);
}
#endif
