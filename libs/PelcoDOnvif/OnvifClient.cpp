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

        const auto vscNode = findRecursiveNodeWithSuffix(node, "VideoSourceConfiguration");
        if (vscNode) {
            const auto srcTokNode = findNodeWithSuffix(vscNode, "SourceToken");
            if (srcTokNode) {
                prof.videoSourceToken = srcTokNode.text().as_string();
            } else {
                prof.videoSourceToken = vscNode.attribute("token").as_string();
            }
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

std::optional<std::string> OnvifClient::parseSystemRebootResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "SystemRebootResponse");
    if (!respNode) {
        return std::nullopt;
    }

    const auto msgNode = findNodeWithSuffix(respNode, "Message");
    if (msgNode) {
        return msgNode.text().as_string();
    }
    return std::string("Reboot initiated");
}

bool OnvifClient::systemReboot()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:SystemReboot/>";
    const std::string reqXml = wrapSoapEnvelope(body);

    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::optional<std::string> OnvifClient::parseSnapshotUriResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto mediaUriNode = findRecursiveNodeWithSuffix(doc, "MediaUri");
    if (!mediaUriNode) {
        return std::nullopt;
    }

    const auto uriNode = findNodeWithSuffix(mediaUriNode, "Uri");
    if (uriNode) {
        return uriNode.text().as_string();
    }
    return std::nullopt;
}

std::optional<std::string> OnvifClient::getSnapshotUri(const std::string& profileToken, bool injectCredentials)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::ostringstream ss {};
    ss << "<trt:GetSnapshotUri>\n"
       << "  <trt:ProfileToken>" << profileToken << "</trt:ProfileToken>\n"
       << "</trt:GetSnapshotUri>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    auto result = parseSnapshotUriResponse(resp.body);
    if (result && injectCredentials && !m_credentials.username.empty() && !result->empty()) {
        constexpr const char* kHttpPrefix = "http://";
        constexpr const char* kHttpsPrefix = "https://";
        if (result->rfind(kHttpPrefix, 0) == 0 && result->find('@') == std::string::npos) {
            std::string injected = kHttpPrefix;
            injected += m_credentials.username;
            if (!m_credentials.password.empty()) {
                injected += ":" + m_credentials.password;
            }
            injected += "@";
            injected += result->substr(std::strlen(kHttpPrefix));
            *result = std::move(injected);
        } else if (result->rfind(kHttpsPrefix, 0) == 0 && result->find('@') == std::string::npos) {
            std::string injected = kHttpsPrefix;
            injected += m_credentials.username;
            if (!m_credentials.password.empty()) {
                injected += ":" + m_credentials.password;
            }
            injected += "@";
            injected += result->substr(std::strlen(kHttpsPrefix));
            *result = std::move(injected);
        }
    }
    return result;
}

bool OnvifClient::relativeMove(
    const std::string& profileToken, double panTranslation, double tiltTranslation, double zoomTranslation)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:RelativeMove>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:Translation>\n"
       << "    <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << panTranslation << "\" y=\""
       << tiltTranslation << "\"/>\n"
       << "    <tt:Zoom x=\"" << zoomTranslation << "\"/>\n"
       << "  </tptz:Translation>\n"
       << "</tptz:RelativeMove>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::gotoHomePosition(const std::string& profileToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:GotoHomePosition>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "</tptz:GotoHomePosition>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::setHomePosition(const std::string& profileToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:SetHomePosition>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "</tptz:SetHomePosition>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

std::vector<PtzPreset> OnvifClient::parsePresetsResponse(const std::string& xml)
{
    std::vector<PtzPreset> presets {};
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return presets;
    }

    std::vector<pugi::xml_node> presetNodes {};
    collectNodesWithSuffix(doc, "Preset", presetNodes);

    for (const auto& node : presetNodes) {
        PtzPreset p {};
        p.token = node.attribute("token").as_string();
        if (p.token.empty()) {
            const auto tokChild = findNodeWithSuffix(node, "token");
            if (tokChild) {
                p.token = tokChild.text().as_string();
            }
        }

        const auto nameNode = findNodeWithSuffix(node, "Name");
        if (nameNode) {
            p.name = nameNode.text().as_string();
        }

        const auto posNode = findRecursiveNodeWithSuffix(node, "PTZPosition");
        if (posNode) {
            const auto pt = findNodeWithSuffix(posNode, "PanTilt");
            if (pt) {
                p.pan = pt.attribute("x").as_double(0.0);
                p.tilt = pt.attribute("y").as_double(0.0);
            }
            const auto z = findNodeWithSuffix(posNode, "Zoom");
            if (z) {
                p.zoom = z.attribute("x").as_double(0.0);
            }
        }

        if (!p.token.empty() || !p.name.empty()) {
            presets.push_back(std::move(p));
        }
    }
    return presets;
}

std::vector<PtzPreset> OnvifClient::getPresets(const std::string& profileToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return {};
    }

    std::ostringstream ss {};
    ss << "<tptz:GetPresets>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "</tptz:GetPresets>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }

    return parsePresetsResponse(resp.body);
}

std::optional<std::string> OnvifClient::parseSetPresetResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "SetPresetResponse");
    if (!respNode) {
        return std::nullopt;
    }

    const auto tokNode = findNodeWithSuffix(respNode, "PresetToken");
    if (tokNode) {
        return tokNode.text().as_string();
    }
    return std::nullopt;
}

std::optional<std::string> OnvifClient::setPreset(
    const std::string& profileToken, const std::string& presetName, const std::string& presetToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<tptz:SetPreset>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n";
    if (!presetName.empty()) {
        ss << "  <tptz:PresetName>" << presetName << "</tptz:PresetName>\n";
    }
    if (!presetToken.empty()) {
        ss << "  <tptz:PresetToken>" << presetToken << "</tptz:PresetToken>\n";
    }
    ss << "</tptz:SetPreset>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseSetPresetResponse(resp.body);
}

bool OnvifClient::gotoPreset(const std::string& profileToken, const std::string& presetToken, double speed)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:GotoPreset>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PresetToken>" << presetToken << "</tptz:PresetToken>\n"
       << "  <tptz:Speed>\n"
       << "    <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << speed << "\" y=\"" << speed << "\"/>\n"
       << "    <tt:Zoom x=\"" << speed << "\"/>\n"
       << "  </tptz:Speed>\n"
       << "</tptz:GotoPreset>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::removePreset(const std::string& profileToken, const std::string& presetToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:RemovePreset>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PresetToken>" << presetToken << "</tptz:PresetToken>\n"
       << "</tptz:RemovePreset>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

std::optional<ImagingSettings> OnvifClient::parseImagingSettingsResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto imgSettingsNode = findRecursiveNodeWithSuffix(doc, "ImagingSettings");
    if (!imgSettingsNode) {
        return std::nullopt;
    }

    ImagingSettings settings {};
    const auto brightNode = findNodeWithSuffix(imgSettingsNode, "Brightness");
    if (brightNode) {
        settings.brightness = brightNode.text().as_float(settings.brightness);
    }
    const auto satNode = findNodeWithSuffix(imgSettingsNode, "ColorSaturation");
    if (satNode) {
        settings.colorSaturation = satNode.text().as_float(settings.colorSaturation);
    }
    const auto contrastNode = findNodeWithSuffix(imgSettingsNode, "Contrast");
    if (contrastNode) {
        settings.contrast = contrastNode.text().as_float(settings.contrast);
    }
    const auto sharpNode = findNodeWithSuffix(imgSettingsNode, "Sharpness");
    if (sharpNode) {
        settings.sharpness = sharpNode.text().as_float(settings.sharpness);
    }
    const auto irNode = findNodeWithSuffix(imgSettingsNode, "IrCutFilter");
    if (irNode) {
        settings.irCutFilter = irNode.text().as_string(settings.irCutFilter.c_str());
    }

    const auto blcNode = findNodeWithSuffix(imgSettingsNode, "BacklightCompensation");
    if (blcNode) {
        const auto modeNode = findNodeWithSuffix(blcNode, "Mode");
        if (modeNode) {
            settings.backlightCompensation = (std::string(modeNode.text().as_string()) == "ON");
        }
        const auto lvlNode = findNodeWithSuffix(blcNode, "Level");
        if (lvlNode) {
            settings.backlightLevel = lvlNode.text().as_float(0.0f);
        }
    }

    const auto wdrNode = findNodeWithSuffix(imgSettingsNode, "WideDynamicRange");
    if (wdrNode) {
        const auto modeNode = findNodeWithSuffix(wdrNode, "Mode");
        if (modeNode) {
            settings.wideDynamicRange = (std::string(modeNode.text().as_string()) == "ON");
        }
        const auto lvlNode = findNodeWithSuffix(wdrNode, "Level");
        if (lvlNode) {
            settings.wdrLevel = lvlNode.text().as_float(0.0f);
        }
    }

    const auto focusNode = findNodeWithSuffix(imgSettingsNode, "Focus");
    if (focusNode) {
        const auto autoModeNode = findNodeWithSuffix(focusNode, "AutoFocusMode");
        if (autoModeNode) {
            settings.autoFocusMode = autoModeNode.text().as_string("AUTO");
        }
    }

    return settings;
}

std::optional<ImagingSettings> OnvifClient::getImagingSettings(const std::string& videoSourceToken)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.imagingXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<timg:GetImagingSettings xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "</timg:GetImagingSettings>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseImagingSettingsResponse(resp.body);
}

bool OnvifClient::setImagingSettings(
    const std::string& videoSourceToken, const ImagingSettings& settings, bool forcePersistence)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.imagingXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<timg:SetImagingSettings xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
       << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "  <timg:ImagingSettings>\n"
       << "    <tt:Brightness>" << std::fixed << std::setprecision(1) << settings.brightness << "</tt:Brightness>\n"
       << "    <tt:ColorSaturation>" << settings.colorSaturation << "</tt:ColorSaturation>\n"
       << "    <tt:Contrast>" << settings.contrast << "</tt:Contrast>\n"
       << "    <tt:Sharpness>" << settings.sharpness << "</tt:Sharpness>\n"
       << "    <tt:IrCutFilter>" << settings.irCutFilter << "</tt:IrCutFilter>\n"
       << "    <tt:BacklightCompensation>\n"
       << "      <tt:Mode>" << (settings.backlightCompensation ? "ON" : "OFF") << "</tt:Mode>\n"
       << "      <tt:Level>" << settings.backlightLevel << "</tt:Level>\n"
       << "    </tt:BacklightCompensation>\n"
       << "    <tt:WideDynamicRange>\n"
       << "      <tt:Mode>" << (settings.wideDynamicRange ? "ON" : "OFF") << "</tt:Mode>\n"
       << "      <tt:Level>" << settings.wdrLevel << "</tt:Level>\n"
       << "    </tt:WideDynamicRange>\n"
       << "    <tt:Focus>\n"
       << "      <tt:AutoFocusMode>" << settings.autoFocusMode << "</tt:AutoFocusMode>\n"
       << "    </tt:Focus>\n"
       << "  </timg:ImagingSettings>\n"
       << "  <timg:ForcePersistence>" << (forcePersistence ? "true" : "false") << "</timg:ForcePersistence>\n"
       << "</timg:SetImagingSettings>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::moveFocus(const std::string& videoSourceToken, float speed)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.imagingXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<timg:Move xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
       << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "  <timg:Focus>\n"
       << "    <tt:Continuous>\n"
       << "      <tt:Speed>" << std::fixed << std::setprecision(2) << speed << "</tt:Speed>\n"
       << "    </tt:Continuous>\n"
       << "  </timg:Focus>\n"
       << "</timg:Move>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::stopFocus(const std::string& videoSourceToken)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.imagingXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<timg:Stop xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "</timg:Stop>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    return resp.isSuccess();
}

std::optional<std::string> OnvifClient::parseCreatePullPointSubscriptionResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "CreatePullPointSubscriptionResponse");
    if (!respNode) {
        return std::nullopt;
    }

    const auto addrNode = findRecursiveNodeWithSuffix(respNode, "Address");
    if (addrNode) {
        const std::string addr = addrNode.text().as_string();
        if (!addr.empty()) {
            return addr;
        }
    }
    return std::nullopt;
}

std::optional<std::string> OnvifClient::createPullPointSubscription()
{
    if (m_capabilities.eventsXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.eventsXAddr.empty()) {
        return std::nullopt;
    }

    const std::string body =
        "<tev:CreatePullPointSubscription xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
        "  <tev:InitialTerminationTime>PT60S</tev:InitialTerminationTime>\n"
        "</tev:CreatePullPointSubscription>";

    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.eventsXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseCreatePullPointSubscriptionResponse(resp.body);
}

std::vector<OnvifEvent> OnvifClient::parsePullMessagesResponse(const std::string& xml)
{
    std::vector<OnvifEvent> events {};

    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return events;
    }

    std::vector<pugi::xml_node> msgNodes {};
    collectNodesWithSuffix(doc, "NotificationMessage", msgNodes);

    for (const auto& msgNode : msgNodes) {
        OnvifEvent ev {};

        const auto topicNode = findNodeWithSuffix(msgNode, "Topic");
        if (topicNode) {
            ev.topic = topicNode.text().as_string();
        }

        const auto messageNode = findNodeWithSuffix(msgNode, "Message");
        if (messageNode) {
            ev.utcTime = messageNode.attribute("UtcTime").as_string();

            const auto sourceNode = findNodeWithSuffix(messageNode, "Source");
            if (sourceNode) {
                const auto simpleItem = findRecursiveNodeWithSuffix(sourceNode, "SimpleItem");
                if (simpleItem) {
                    ev.sourceName = simpleItem.attribute("Name").as_string();
                    ev.sourceValue = simpleItem.attribute("Value").as_string();
                }
            }

            const auto dataNode = findNodeWithSuffix(messageNode, "Data");
            if (dataNode) {
                const auto simpleItem = findRecursiveNodeWithSuffix(dataNode, "SimpleItem");
                if (simpleItem) {
                    ev.dataName = simpleItem.attribute("Name").as_string();
                    ev.dataValue = simpleItem.attribute("Value").as_string();
                }
            }
        }

        if (!ev.topic.empty() || !ev.dataName.empty()) {
            events.push_back(std::move(ev));
        }
    }

    return events;
}

std::vector<OnvifEvent> OnvifClient::pullMessages(
    const std::string& subscriptionUrl, int timeoutSeconds, int messageLimit)
{
    if (subscriptionUrl.empty()) {
        return {};
    }

    std::ostringstream ss {};
    ss << "<tev:PullMessages xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
       << "  <tev:Timeout>PT" << timeoutSeconds << "S</tev:Timeout>\n"
       << "  <tev:MessageLimit>" << messageLimit << "</tev:MessageLimit>\n"
       << "</tev:PullMessages>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(subscriptionUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }

    return parsePullMessagesResponse(resp.body);
}

bool OnvifClient::unsubscribe(const std::string& subscriptionUrl)
{
    if (subscriptionUrl.empty()) {
        return false;
    }

    const std::string body = "<wsnt:Unsubscribe xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\"/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(subscriptionUrl, reqXml);
    return resp.isSuccess();
}

} // namespace PelcoD::Onvif
