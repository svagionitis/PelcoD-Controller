#include "OnvifClient.h"

#include <pugixml.hpp>

#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PelcoD::Onvif {

namespace {

    pugi::xml_node findNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix)
    {
        for (const auto& child : parent.children()) {
            const std::string name = child.name();
            const auto colonPos = name.find(':');
            const std::string localName = (colonPos != std::string::npos) ? name.substr(colonPos + 1) : name;
            if (localName == suffix) {
                return child;
            }
        }
        return {};
    }

    void collectNodesWithSuffix(
        const pugi::xml_node& parent, const std::string& suffix, std::vector<pugi::xml_node>& result)
    {
        for (const auto& child : parent.children()) {
            const std::string name = child.name();
            const auto colonPos = name.find(':');
            const std::string localName = (colonPos != std::string::npos) ? name.substr(colonPos + 1) : name;
            if (localName == suffix) {
                result.push_back(child);
            }
            collectNodesWithSuffix(child, suffix, result);
        }
    }

    pugi::xml_node findRecursiveNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix)
    {
        std::vector<pugi::xml_node> matches {};
        collectNodesWithSuffix(parent, suffix, matches);
        if (!matches.empty()) {
            return matches.front();
        }
        return {};
    }

} // namespace

OnvifClient::OnvifClient(std::string deviceEndpoint, SecurityCredentials credentials)
    : m_deviceEndpoint(std::move(deviceEndpoint))
    , m_credentials(std::move(credentials))
{
}

void OnvifClient::setCredentials(SecurityCredentials credentials)
{
    m_credentials = std::move(credentials);
}

const SecurityCredentials& OnvifClient::credentials() const
{
    return m_credentials;
}

void OnvifClient::setDeviceEndpoint(std::string endpoint)
{
    m_deviceEndpoint = std::move(endpoint);
}

const std::string& OnvifClient::deviceEndpoint() const
{
    return m_deviceEndpoint;
}

void OnvifClient::setTimeout(std::chrono::milliseconds timeout)
{
    m_httpClient.setTimeout(timeout);
}

std::string OnvifClient::wrapSoapEnvelope(const std::string& bodyContent) const
{
    const std::string securityHeader = OnvifSecurity::buildSoapSecurityHeader(m_credentials);

    std::ostringstream ss {};
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
       << "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
       << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
       << "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
       << "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n";

    if (!securityHeader.empty()) {
        ss << "  <s:Header>\n"
           << securityHeader << "\n"
           << "  </s:Header>\n";
    }

    ss << "  <s:Body>\n"
       << bodyContent << "\n"
       << "  </s:Body>\n"
       << "</s:Envelope>";

    return ss.str();
}

std::optional<OnvifCapabilities> OnvifClient::parseCapabilitiesResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto capNode = findRecursiveNodeWithSuffix(doc, "Capabilities");
    if (!capNode) {
        return std::nullopt;
    }

    OnvifCapabilities caps {};

    const auto deviceNode = findNodeWithSuffix(capNode, "Device");
    if (deviceNode) {
        const auto xaddr = findNodeWithSuffix(deviceNode, "XAddr");
        if (xaddr) {
            caps.deviceXAddr = xaddr.text().as_string();
        }
    }

    const auto mediaNode = findNodeWithSuffix(capNode, "Media");
    if (mediaNode) {
        const auto xaddr = findNodeWithSuffix(mediaNode, "XAddr");
        if (xaddr) {
            caps.mediaXAddr = xaddr.text().as_string();
        }
    }

    const auto ptzNode = findNodeWithSuffix(capNode, "PTZ");
    if (ptzNode) {
        const auto xaddr = findNodeWithSuffix(ptzNode, "XAddr");
        if (xaddr) {
            caps.ptzXAddr = xaddr.text().as_string();
        }
    }

    const auto eventsNode = findNodeWithSuffix(capNode, "Events");
    if (eventsNode) {
        const auto xaddr = findNodeWithSuffix(eventsNode, "XAddr");
        if (xaddr) {
            caps.eventsXAddr = xaddr.text().as_string();
        }
    }

    const auto imagingNode = findNodeWithSuffix(capNode, "Imaging");
    if (imagingNode) {
        const auto xaddr = findNodeWithSuffix(imagingNode, "XAddr");
        if (xaddr) {
            caps.imagingXAddr = xaddr.text().as_string();
        }
    }

    return caps;
}

std::optional<OnvifCapabilities> OnvifClient::getCapabilities()
{
    const std::string body = "<tds:GetCapabilities><tds:Category>All</tds:Category></tds:GetCapabilities>";
    const std::string reqXml = wrapSoapEnvelope(body);

    const HttpResponse resp = m_httpClient.sendPost(m_deviceEndpoint, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    const auto parsed = parseCapabilitiesResponse(resp.body);
    if (parsed) {
        m_capabilities = *parsed;
        if (m_capabilities.deviceXAddr.empty()) {
            m_capabilities.deviceXAddr = m_deviceEndpoint;
        }
    }
    return parsed;
}

std::optional<DeviceInformation> OnvifClient::parseDeviceInformationResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "GetDeviceInformationResponse");
    if (!respNode) {
        return std::nullopt;
    }

    DeviceInformation info {};

    const auto mfg = findNodeWithSuffix(respNode, "Manufacturer");
    if (mfg) {
        info.manufacturer = mfg.text().as_string();
    }

    const auto model = findNodeWithSuffix(respNode, "Model");
    if (model) {
        info.model = model.text().as_string();
    }

    const auto fw = findNodeWithSuffix(respNode, "FirmwareVersion");
    if (fw) {
        info.firmwareVersion = fw.text().as_string();
    }

    const auto sn = findNodeWithSuffix(respNode, "SerialNumber");
    if (sn) {
        info.serialNumber = sn.text().as_string();
    }

    const auto hw = findNodeWithSuffix(respNode, "HardwareId");
    if (hw) {
        info.hardwareId = hw.text().as_string();
    }

    return info;
}

std::optional<DeviceInformation> OnvifClient::getDeviceInformation()
{
    const std::string body = "<tds:GetDeviceInformation/>";
    const std::string reqXml = wrapSoapEnvelope(body);

    const HttpResponse resp = m_httpClient.sendPost(m_deviceEndpoint, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseDeviceInformationResponse(resp.body);
}

bool OnvifClient::synchronizeSystemTime()
{
    const std::string body = "<tds:GetSystemDateAndTime/>";
    const std::string reqXml = wrapSoapEnvelope(body);

    const HttpResponse resp = m_httpClient.sendPost(m_deviceEndpoint, reqXml);
    if (!resp.isSuccess()) {
        return false;
    }

    pugi::xml_document doc {};
    if (!doc.load_string(resp.body.c_str())) {
        return false;
    }

    const auto utcTimeNode = findRecursiveNodeWithSuffix(doc, "UTCDateTime");
    if (!utcTimeNode) {
        return false;
    }

    const auto timeNode = findNodeWithSuffix(utcTimeNode, "Time");
    const auto dateNode = findNodeWithSuffix(utcTimeNode, "Date");
    if (!timeNode || !dateNode) {
        return false;
    }

    const int hour = findNodeWithSuffix(timeNode, "Hour").text().as_int(0);
    const int minute = findNodeWithSuffix(timeNode, "Minute").text().as_int(0);
    const int second = findNodeWithSuffix(timeNode, "Second").text().as_int(0);

    const int year = findNodeWithSuffix(dateNode, "Year").text().as_int(1970);
    const int month = findNodeWithSuffix(dateNode, "Month").text().as_int(1);
    const int day = findNodeWithSuffix(dateNode, "Day").text().as_int(1);

    std::tm camTm {};
    camTm.tm_year = year - 1900;
    camTm.tm_mon = month - 1;
    camTm.tm_mday = day;
    camTm.tm_hour = hour;
    camTm.tm_min = minute;
    camTm.tm_sec = second;

#if defined(_WIN32)
    const std::time_t camTimeT = _mkgmtime(&camTm);
#else
    const std::time_t camTimeT = timegm(&camTm);
#endif

    const auto systemNowT = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    m_credentials.clockOffset = std::chrono::seconds(camTimeT - systemNowT);
    return true;
}

std::vector<MediaProfile> OnvifClient::parseProfilesResponse(const std::string& xml)
{
    std::vector<MediaProfile> profiles {};

    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return profiles;
    }

    std::vector<pugi::xml_node> profileNodes {};
    collectNodesWithSuffix(doc, "Profiles", profileNodes);

    for (const auto& node : profileNodes) {
        MediaProfile prof {};
        prof.token = node.attribute("token").as_string();
        if (prof.token.empty()) {
            const auto tokenChild = findNodeWithSuffix(node, "token");
            if (tokenChild) {
                prof.token = tokenChild.text().as_string();
            }
        }

        const auto nameNode = findNodeWithSuffix(node, "Name");
        if (nameNode) {
            prof.name = nameNode.text().as_string();
        }

        const auto vecNode = findRecursiveNodeWithSuffix(node, "VideoEncoderConfiguration");
        if (vecNode) {
            const auto resNode = findNodeWithSuffix(vecNode, "Resolution");
            if (resNode) {
                prof.videoWidth = findNodeWithSuffix(resNode, "Width").text().as_int(0);
                prof.videoHeight = findNodeWithSuffix(resNode, "Height").text().as_int(0);
            }
            const auto encNode = findNodeWithSuffix(vecNode, "Encoding");
            if (encNode) {
                prof.videoEncoding = encNode.text().as_string();
            }
        }

        if (!prof.token.empty()) {
            profiles.push_back(std::move(prof));
        }
    }

    return profiles;
}

std::vector<MediaProfile> OnvifClient::getProfiles()
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    const std::string body = "<trt:GetProfiles/>";
    const std::string reqXml = wrapSoapEnvelope(body);

    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }

    return parseProfilesResponse(resp.body);
}

std::optional<StreamUriInfo> OnvifClient::parseStreamUriResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto mediaUriNode = findRecursiveNodeWithSuffix(doc, "MediaUri");
    if (!mediaUriNode) {
        return std::nullopt;
    }

    StreamUriInfo info {};
    const auto uriNode = findNodeWithSuffix(mediaUriNode, "Uri");
    if (uriNode) {
        info.uri = uriNode.text().as_string();
    }

    const auto invConnect = findNodeWithSuffix(mediaUriNode, "InvalidAfterConnect");
    if (invConnect) {
        info.invalidAfterConnect = invConnect.text().as_bool(false);
    }

    const auto invReboot = findNodeWithSuffix(mediaUriNode, "InvalidAfterReboot");
    if (invReboot) {
        info.invalidAfterReboot = invReboot.text().as_bool(false);
    }

    const auto timeoutNode = findNodeWithSuffix(mediaUriNode, "Timeout");
    if (timeoutNode) {
        info.timeout = timeoutNode.text().as_string();
    }

    return info;
}

std::optional<StreamUriInfo> OnvifClient::getStreamUri(const std::string& profileToken, bool injectCredentials)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::ostringstream ss {};
    ss << "<trt:GetStreamUri>\n"
       << "  <trt:StreamSetup>\n"
       << "    <tt:Stream>RTP-Unicast</tt:Stream>\n"
       << "    <tt:Transport>\n"
       << "      <tt:Protocol>RTSP</tt:Protocol>\n"
       << "    </tt:Transport>\n"
       << "  </trt:StreamSetup>\n"
       << "  <trt:ProfileToken>" << profileToken << "</trt:ProfileToken>\n"
       << "</trt:GetStreamUri>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    auto result = parseStreamUriResponse(resp.body);
    if (result && injectCredentials && !m_credentials.username.empty() && !result->uri.empty()) {
        constexpr const char* kRtspPrefix = "rtsp://";
        if (result->uri.rfind(kRtspPrefix, 0) == 0 && result->uri.find('@') == std::string::npos) {
            std::string injected = kRtspPrefix;
            injected += m_credentials.username;
            if (!m_credentials.password.empty()) {
                injected += ":" + m_credentials.password;
            }
            injected += "@";
            injected += result->uri.substr(std::strlen(kRtspPrefix));
            result->uri = injected;
        }
    }

    return result;
}

bool OnvifClient::continuousMove(const std::string& profileToken, double panSpeed, double tiltSpeed, double zoomSpeed)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:ContinuousMove>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:Velocity>\n"
       << "    <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << panSpeed << "\" y=\"" << tiltSpeed << "\"/>\n"
       << "    <tt:Zoom x=\"" << zoomSpeed << "\"/>\n"
       << "  </tptz:Velocity>\n"
       << "</tptz:ContinuousMove>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::stop(const std::string& profileToken, bool stopPanTilt, bool stopZoom)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:Stop>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PanTilt>" << (stopPanTilt ? "true" : "false") << "</tptz:PanTilt>\n"
       << "  <tptz:Zoom>" << (stopZoom ? "true" : "false") << "</tptz:Zoom>\n"
       << "</tptz:Stop>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

std::optional<PtzStatus> OnvifClient::parsePtzStatusResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto ptzStatusNode = findRecursiveNodeWithSuffix(doc, "PTZStatus");
    if (!ptzStatusNode) {
        return std::nullopt;
    }

    PtzStatus status {};

    const auto posNode = findNodeWithSuffix(ptzStatusNode, "Position");
    if (posNode) {
        const auto panTilt = findNodeWithSuffix(posNode, "PanTilt");
        if (panTilt) {
            status.pan = panTilt.attribute("x").as_double(0.0);
            status.tilt = panTilt.attribute("y").as_double(0.0);
        }
        const auto zoom = findNodeWithSuffix(posNode, "Zoom");
        if (zoom) {
            status.zoom = zoom.attribute("x").as_double(0.0);
        }
    }

    const auto moveNode = findNodeWithSuffix(ptzStatusNode, "MoveStatus");
    if (moveNode) {
        const auto ptStatus = findNodeWithSuffix(moveNode, "PanTilt").text().as_string();
        const auto zStatus = findNodeWithSuffix(moveNode, "Zoom").text().as_string();
        status.isMoving = (std::string(ptStatus) == "MOVING" || std::string(zStatus) == "MOVING");
    }

    const auto timeNode = findNodeWithSuffix(ptzStatusNode, "UtcTime");
    if (timeNode) {
        status.utcTime = timeNode.text().as_string();
    }

    return status;
}

std::optional<PtzStatus> OnvifClient::getStatus(const std::string& profileToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<tptz:GetStatus>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "</tptz:GetStatus>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parsePtzStatusResponse(resp.body);
}

bool OnvifClient::absoluteMove(const std::string& profileToken, double pan, double tilt, double zoom)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:AbsoluteMove>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:Position>\n"
       << "    <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << pan << "\" y=\"" << tilt << "\"/>\n"
       << "    <tt:Zoom x=\"" << zoom << "\"/>\n"
       << "  </tptz:Position>\n"
       << "</tptz:AbsoluteMove>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

} // namespace PelcoD::Onvif
