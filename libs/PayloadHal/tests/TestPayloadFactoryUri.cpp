#include "PayloadFactory.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

    TEST(TestPayloadFactoryUri, SimScheme)
    {
        auto p1 = PayloadFactory::createFromUri("sim://");
        ASSERT_NE(p1, nullptr);
        EXPECT_NE(p1->panTilt(), nullptr);
        EXPECT_NE(p1->primaryCamera(), nullptr);
        EXPECT_NE(p1->secondaryCamera(), nullptr);
        EXPECT_NE(p1->lrf(), nullptr);

        auto p2 = PayloadFactory::createFromUri("sim");
        ASSERT_NE(p2, nullptr);
    }

    TEST(TestPayloadFactoryUri, OnvifScheme)
    {
        auto p = PayloadFactory::createFromUri("onvif://admin:pass@127.0.0.1:8080/onvif/device_service");
        ASSERT_NE(p, nullptr);
        EXPECT_NE(p->panTilt(), nullptr);
        EXPECT_NE(p->primaryCamera(), nullptr);
    }

    TEST(TestPayloadFactoryUri, PelcoDSchemeNetworkAndSerial)
    {
        // Network TCP
        auto pNetTcp = PayloadFactory::createFromUri("pelcod://127.0.0.1:4001?addr=2&proto=tcp");
        ASSERT_NE(pNetTcp, nullptr);
        EXPECT_NE(pNetTcp->panTilt(), nullptr);
        EXPECT_NE(pNetTcp->primaryCamera(), nullptr);

        // Network UDP
        auto pNetUdp = PayloadFactory::createFromUri("pelcod://192.168.1.50:4001?addr=3&proto=udp");
        ASSERT_NE(pNetUdp, nullptr);
        EXPECT_NE(pNetUdp->panTilt(), nullptr);

        // Serial path
        auto pSerial = PayloadFactory::createFromUri("pelcod:///dev/ttyUSB0?baud=9600&addr=1");
        ASSERT_NE(pSerial, nullptr);
        EXPECT_NE(pSerial->panTilt(), nullptr);
    }

    TEST(TestPayloadFactoryUri, SerialSchemeProtocols)
    {
        // Serial Pelco-D
        auto pPelco = PayloadFactory::createFromUri("serial:///dev/ttyUSB0?baud=19200&protocol=pelcod&addr=3");
        ASSERT_NE(pPelco, nullptr);
        EXPECT_NE(pPelco->panTilt(), nullptr);

        // Serial Fujinon SX800
        auto pFuji = PayloadFactory::createFromUri("serial:///dev/ttyUSB1?baud=38400&protocol=fujinon&addr=2");
        ASSERT_NE(pFuji, nullptr);
        EXPECT_NE(pFuji->panTilt(), nullptr);
        EXPECT_NE(pFuji->primaryCamera(), nullptr);
    }

    TEST(TestPayloadFactoryUri, TcpAndUdpSchemes)
    {
        // Raw TCP
        auto pTcp = PayloadFactory::createFromUri("tcp://127.0.0.1:4001?protocol=pelcod&addr=1");
        ASSERT_NE(pTcp, nullptr);
        EXPECT_NE(pTcp->panTilt(), nullptr);

        // Raw UDP with Fujinon
        auto pUdp = PayloadFactory::createFromUri("udp://127.0.0.1:4001?protocol=fujinon&addr=2");
        ASSERT_NE(pUdp, nullptr);
        EXPECT_NE(pUdp->panTilt(), nullptr);
    }

    TEST(TestPayloadFactoryUri, FujinonScheme)
    {
        // Network Fujinon
        auto pNet = PayloadFactory::createFromUri("fujinon://127.0.0.1:4001?addr=1");
        ASSERT_NE(pNet, nullptr);
        EXPECT_NE(pNet->panTilt(), nullptr);
        EXPECT_NE(pNet->primaryCamera(), nullptr);

        // Serial Fujinon
        auto pSerial = PayloadFactory::createFromUri("fujinon:///dev/ttyUSB0?baud=9600&addr=1");
        ASSERT_NE(pSerial, nullptr);
        EXPECT_NE(pSerial->panTilt(), nullptr);
    }

    TEST(TestPayloadFactoryUri, PelcoDViscaCompositeScheme)
    {
        // Network composite: PTZ on 4001, Camera on 4002
        auto pNet = PayloadFactory::createFromUri(
            "pelcod-visca://127.0.0.1:4001?cam_host=127.0.0.1&cam_port=4002&ptz_addr=1&cam_addr=1");
        ASSERT_NE(pNet, nullptr);
        EXPECT_NE(pNet->panTilt(), nullptr);
        EXPECT_NE(pNet->primaryCamera(), nullptr);
        EXPECT_EQ(pNet->secondaryCamera(), nullptr);
        EXPECT_EQ(pNet->lrf(), nullptr);

        // Serial composite: PTZ on /dev/ttyUSB0, Camera on /dev/ttyUSB1
        auto pSerial = PayloadFactory::createFromUri(
            "pelcod-visca://?ptz_dev=/dev/ttyUSB0&cam_dev=/dev/ttyUSB1&baud=9600&ptz_addr=1&cam_addr=1");
        ASSERT_NE(pSerial, nullptr);
        EXPECT_NE(pSerial->panTilt(), nullptr);
        EXPECT_NE(pSerial->primaryCamera(), nullptr);
    }

    TEST(TestPayloadFactoryUri, MalformedAndInvalidUris)
    {
        // Empty string
        EXPECT_EQ(PayloadFactory::createFromUri(""), nullptr);

        // Unknown scheme
        EXPECT_EQ(PayloadFactory::createFromUri("unknown://127.0.0.1:4001"), nullptr);

        // Invalid baud rate
        EXPECT_EQ(PayloadFactory::createFromUri("serial:///dev/ttyUSB0?baud=12345&protocol=pelcod"), nullptr);

        // Out of bounds address
        EXPECT_EQ(PayloadFactory::createFromUri("pelcod://127.0.0.1:4001?addr=300"), nullptr);
        EXPECT_EQ(PayloadFactory::createFromUri("fujinon://127.0.0.1:4001?addr=40"), nullptr);

        // Incomplete composite (missing camera port/dev)
        EXPECT_EQ(PayloadFactory::createFromUri("pelcod-visca://?ptz_dev=/dev/ttyUSB0"), nullptr);

        // Unknown protocol for serial
        EXPECT_EQ(PayloadFactory::createFromUri("serial:///dev/ttyUSB0?baud=9600&protocol=nonexistent"), nullptr);
    }

} // namespace
} // namespace PayloadHal
