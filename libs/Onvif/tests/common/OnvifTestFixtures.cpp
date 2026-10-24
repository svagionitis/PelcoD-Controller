/// @file OnvifTestFixtures.cpp
/// @brief Implementation of reusable RAII GoogleTest fixture for OnvifServer tests.

#include "OnvifTestFixtures.h"

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Onvif::Test {

std::uint16_t getFreeEphemeralPort()
{
#if defined(_WIN32)
    WSADATA wsa { };
    static_cast<void>(WSAStartup(MAKEWORD(2, 2), &wsa));
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return 18080U;
    }
    sockaddr_in addr { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // OS allocates ephemeral port
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(sizeof(addr))) != 0) {
        closesocket(sock);
        return 18080U;
    }
    int len = static_cast<int>(sizeof(addr));
    static_cast<void>(getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len));
    const auto port = static_cast<std::uint16_t>(ntohs(addr.sin_port));
    closesocket(sock);
    return port;
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 18080U;
    }
    sockaddr_in addr { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), static_cast<socklen_t>(sizeof(addr))) != 0) {
        close(sock);
        return 18080U;
    }
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    static_cast<void>(getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len));
    const auto port = static_cast<std::uint16_t>(ntohs(addr.sin_port));
    close(sock);
    return port;
#endif
}

bool normalizeXml(std::string_view rawXml, std::string& outFormatted)
{
    pugi::xml_document doc { };
    const auto res = doc.load_buffer(rawXml.data(), rawXml.size());
    if (!res) {
        return false;
    }
    std::ostringstream ss { };
    doc.save(ss, "  ", pugi::format_indent);
    outFormatted = ss.str();
    return true;
}

void OnvifServerFixture::SetUp()
{
    m_port = getFreeEphemeralPort();
    m_config.bindAddress = "127.0.0.1";
    m_config.port = static_cast<int>(m_port);
    m_config.deviceName = "Test Bridge Camera";
    m_config.manufacturer = "PelcoD-Test";
    m_config.model = "Model-XYZ";
    m_config.rtspStreamUri = "rtsp://127.0.0.1:8554/test_stream";
    m_config.defaultUsers = { { "admin", "Adm1n-Pass", OnvifUserLevel::Administrator },
        { "operator", "Operat0r-Pass!", OnvifUserLevel::Operator } };
    m_config.passwordPolicy.minClasses = 1;
    m_config.passwordPolicy.rejectCommonDefaults = false;

    m_ptzHandler = std::make_shared<MockPtzHandler>();
    m_imagingHandler = std::make_shared<MockImagingHandler>();

    m_server = std::make_shared<OnvifServer>(m_config, m_ptzHandler);
    m_server->setImagingHandler(m_imagingHandler);

    ASSERT_TRUE(m_server->start());
    ASSERT_TRUE(m_server->isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    m_client = std::make_unique<OnvifTest::OnvifTestClient>("127.0.0.1", static_cast<int>(m_port));
    m_client->set_connection_timeout(std::chrono::seconds(2));
    m_client->set_read_timeout(std::chrono::seconds(2));
}

void OnvifServerFixture::TearDown()
{
    if (m_server) {
        m_server->stop();
    }
    m_client.reset();
    m_server.reset();
    m_ptzHandler.reset();
    m_imagingHandler.reset();
}

int OnvifServerFixture::sendSoapReq(std::string_view path, std::string_view soapRequest, pugi::xml_document& outDoc)
{
    if (!m_client) {
        return -1;
    }
    auto res = m_client->Post(std::string(path), std::string(soapRequest), "application/soap+xml; charset=utf-8");
    if (!res) {
        return -1;
    }
    const auto parseRes = outDoc.load_string(res->body.c_str());
    if (!parseRes) {
        // Return status even if body wasn't valid XML (e.g., error responses)
        return res->status;
    }
    return res->status;
}

} // namespace Onvif::Test
