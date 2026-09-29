/// @file TestIec61162_460Firewall.cpp
/// @brief Unit tests for IEC 61162-460 Secure Marine Gateway Firewall and BAM alert integration.

#include "bam/BridgeAlertManager.h"
#include "network/Iec61162_460Firewall.h"

#include <gtest/gtest.h>

using namespace Nmea::Network;
using namespace Nmea::Bam;

TEST(TestIec61162_460Firewall, MacAntiSpoofingValidation)
{
    Iec61162_460Firewall firewall;
    firewall.bindMacAddress("192.168.1.100", "00:1A:2B:3C:4D:5E");

    // Valid MAC matches binding
    EXPECT_TRUE(firewall.inspectInbound(SecurityZone::BridgeNetwork, "192.168.1.100",
                                        "00:1A:2B:3C:4D:5E", "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));

    // Spoofed MAC fails inspection
    EXPECT_FALSE(firewall.inspectInbound(SecurityZone::BridgeNetwork, "192.168.1.100",
                                         "AA:BB:CC:DD:EE:FF", "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));

    EXPECT_EQ(firewall.getViolationCount(), 1U);
    const auto incidents = firewall.getIncidentHistory();
    ASSERT_EQ(incidents.size(), 1U);
    EXPECT_EQ(incidents[0].violationType, SecurityViolationType::MacIpMismatch);
}

TEST(TestIec61162_460Firewall, ZoneSentenceFormatterWhitelist)
{
    Iec61162_460Firewall firewall;

    FirewallRule rule {};
    rule.allowedZone = SecurityZone::ExternalCamera;
    rule.ipCidr = "192.168.20.0/24";
    rule.allowedFormatters = { "XDR", "HDT" }; // Only allow environmental / heading sentences
    firewall.addRule(rule);

    // Permitted sentence
    EXPECT_TRUE(firewall.inspectInbound(SecurityZone::ExternalCamera, "192.168.20.50",
                                        "", "$HEHDT,224.5,T*25"));

    // Disallowed sentence formatter (GGA) from camera zone
    EXPECT_FALSE(firewall.inspectInbound(SecurityZone::ExternalCamera, "192.168.20.50",
                                         "", "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));

    EXPECT_EQ(firewall.getViolationCount(), 1U);
    const auto incidents = firewall.getIncidentHistory();
    ASSERT_EQ(incidents.size(), 1U);
    EXPECT_EQ(incidents[0].violationType, SecurityViolationType::DisallowedSentence);
}

TEST(TestIec61162_460Firewall, TrafficRateLimitingAndBamAlert)
{
    Iec61162_460Firewall firewall;
    BamConfig cfg {};
    cfg.talkerId = "GP";
    auto bam = std::make_shared<BridgeAlertManager>(cfg);
    firewall.attachAlertManager(bam);

    FirewallRule rule {};
    rule.allowedZone = SecurityZone::BridgeNetwork;
    rule.ipCidr = "192.168.1.0/24";
    rule.maxPacketsPerSec = 5U; // Low limit to test flooding quickly
    firewall.addRule(rule);

    // First 5 packets pass
    for (std::size_t i = 0; i < 5U; ++i) {
        EXPECT_TRUE(firewall.inspectInbound(SecurityZone::BridgeNetwork, "192.168.1.10",
                                            "", "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));
    }

    // 6th packet exceeds rate limit and triggers alert
    EXPECT_FALSE(firewall.inspectInbound(SecurityZone::BridgeNetwork, "192.168.1.10",
                                         "", "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"));

    EXPECT_EQ(firewall.getViolationCount(), 1U);
    EXPECT_EQ(bam->activeAlerts().size(), 1U);
    const auto alertOpt = bam->alertById(kAlertIdNetworkFlood, 1U);
    ASSERT_TRUE(alertOpt.has_value());
    EXPECT_EQ(alertOpt->alertText, "Bridge Network Flood Detected");
}

TEST(TestIec61162_460Firewall, OutboundEgressProtection)
{
    Iec61162_460Firewall firewall;

    // Normal sentence to internet zone allowed (e.g. basic position)
    EXPECT_TRUE(firewall.inspectOutbound(SecurityZone::InternetShore, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"));

    // Sensitive Bridge Alert Management or proprietary camera command blocked to internet
    EXPECT_FALSE(firewall.inspectOutbound(SecurityZone::InternetShore, "$GPALF,1,1,0,123519,B,W,A,46001,1,1,0,Security Violation*00"));
    EXPECT_FALSE(firewall.inspectOutbound(SecurityZone::InternetShore, "$PFEC,GPflir,cam,1,zoom,4*7A"));
}
