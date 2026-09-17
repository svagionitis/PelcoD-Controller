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

    const auto extNode = findNodeWithSuffix(capNode, "Extension");
    if (extNode) {
        const auto devIoNode = findNodeWithSuffix(extNode, "DeviceIO");
        if (devIoNode) {
            const auto xaddr = findNodeWithSuffix(devIoNode, "XAddr");
            if (xaddr) {
                caps.deviceIoXAddr = xaddr.text().as_string();
            }
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
        if (m_capabilities.deviceIoXAddr.empty() && !m_deviceEndpoint.empty()) {
            const auto pos = m_deviceEndpoint.find("/onvif/");
            if (pos != std::string::npos) {
                m_capabilities.deviceIoXAddr = m_deviceEndpoint.substr(0, pos) + "/onvif/deviceio_service";
            }
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
       << "    <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << panTranslation << "\" y=\"" << tiltTranslation
       << "\"/>\n"
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

std::optional<std::string> OnvifClient::sendAuxiliaryCommand(
    const std::string& profileToken, const std::string& auxiliaryData)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<tptz:SendAuxiliaryCommand>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:AuxiliaryData>" << auxiliaryData << "</tptz:AuxiliaryData>\n"
       << "</tptz:SendAuxiliaryCommand>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseSendAuxiliaryCommandResponse(resp.body);
}

std::optional<std::string> OnvifClient::parseSendAuxiliaryCommandResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "AuxiliaryResponse");
    if (respNode) {
        return respNode.text().as_string();
    }

    const auto mainNode = findRecursiveNodeWithSuffix(doc, "SendAuxiliaryCommandResponse");
    if (mainNode) {
        return "";
    }

    return std::nullopt;
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

static std::uint32_t parseIsoDurationSeconds(std::string_view str)
{
    if (str.empty()) {
        return 5U;
    }
    std::size_t pos = 0;
    if (str.rfind("PT", 0) == 0) {
        pos = 2;
    }
    std::uint32_t val = 0;
    while (pos < str.size() && std::isdigit(static_cast<unsigned char>(str[pos]))) {
        val = val * 10 + static_cast<std::uint32_t>(str[pos] - '0');
        ++pos;
    }
    if (pos < str.size() && (str[pos] == 'M' || str[pos] == 'm')) {
        val *= 60U;
    }
    return (val > 0) ? val : 5U;
}

static PresetTour parsePresetTourNode(const pugi::xml_node& node)
{
    PresetTour tour {};
    tour.token = node.attribute("token").as_string();
    if (tour.token.empty()) {
        const auto tokChild = findNodeWithSuffix(node, "token");
        if (tokChild) {
            tour.token = tokChild.text().as_string();
        }
    }

    const auto nameNode = findNodeWithSuffix(node, "Name");
    if (nameNode) {
        tour.name = nameNode.text().as_string();
    }

    const auto autoStartNode = findNodeWithSuffix(node, "AutoStart");
    if (autoStartNode) {
        tour.autoStart = autoStartNode.text().as_bool(false);
    }

    const auto statusNode = findRecursiveNodeWithSuffix(node, "Status");
    if (statusNode) {
        const auto stateNode = findNodeWithSuffix(statusNode, "State");
        if (stateNode) {
            const std::string stateStr = stateNode.text().as_string();
            if (stateStr == "Touring" || stateStr == "Running") {
                tour.status = PresetTourState::Touring;
            } else if (stateStr == "Paused") {
                tour.status = PresetTourState::Paused;
            } else if (stateStr == "Extended") {
                tour.status = PresetTourState::Extended;
            } else {
                tour.status = PresetTourState::Idle;
            }
        }
    }

    std::vector<pugi::xml_node> spotNodes {};
    collectNodesWithSuffix(node, "TourSpot", spotNodes);
    for (const auto& spotNode : spotNodes) {
        PresetTourSpot spot {};
        const auto detailNode = findRecursiveNodeWithSuffix(spotNode, "PresetDetail");
        if (detailNode) {
            const auto pTokNode = findNodeWithSuffix(detailNode, "PresetToken");
            if (pTokNode) {
                spot.presetToken = pTokNode.text().as_string();
            }
        }
        if (spot.presetToken.empty()) {
            const auto pTokNode = findRecursiveNodeWithSuffix(spotNode, "PresetToken");
            if (pTokNode) {
                spot.presetToken = pTokNode.text().as_string();
            }
        }

        const auto speedNode = findRecursiveNodeWithSuffix(spotNode, "Speed");
        if (speedNode) {
            const auto pt = findNodeWithSuffix(speedNode, "PanTilt");
            if (pt) {
                spot.speed = pt.attribute("x").as_float(1.0f);
            } else {
                spot.speed = speedNode.text().as_float(1.0f);
            }
        }

        const auto stayNode = findNodeWithSuffix(spotNode, "StayTime");
        if (stayNode) {
            spot.stayTimeSeconds = parseIsoDurationSeconds(stayNode.text().as_string());
        }

        if (!spot.presetToken.empty()) {
            tour.spots.push_back(std::move(spot));
        }
    }

    return tour;
}

std::vector<PresetTour> OnvifClient::parsePresetToursResponse(const std::string& xml)
{
    std::vector<PresetTour> tours {};
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return tours;
    }

    std::vector<pugi::xml_node> tourNodes {};
    collectNodesWithSuffix(doc, "PresetTour", tourNodes);
    for (const auto& node : tourNodes) {
        PresetTour tour = parsePresetTourNode(node);
        if (!tour.token.empty() || !tour.name.empty()) {
            tours.push_back(std::move(tour));
        }
    }
    return tours;
}

std::optional<PresetTour> OnvifClient::parsePresetTourResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto tourNode = findRecursiveNodeWithSuffix(doc, "PresetTour");
    if (!tourNode) {
        return std::nullopt;
    }

    return parsePresetTourNode(tourNode);
}

std::optional<std::string> OnvifClient::parseCreatePresetTourResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto tokNode = findRecursiveNodeWithSuffix(doc, "PresetTourToken");
    if (tokNode) {
        return tokNode.text().as_string();
    }
    return std::nullopt;
}

std::vector<PresetTour> OnvifClient::getPresetTours(const std::string& profileToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return {};
    }

    std::ostringstream ss {};
    ss << "<tptz:GetPresetTours>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "</tptz:GetPresetTours>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }

    return parsePresetToursResponse(resp.body);
}

std::optional<PresetTour> OnvifClient::getPresetTour(const std::string& profileToken, const std::string& tourToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<tptz:GetPresetTour>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PresetTourToken>" << tourToken << "</tptz:PresetTourToken>\n"
       << "</tptz:GetPresetTour>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parsePresetTourResponse(resp.body);
}

std::optional<std::string> OnvifClient::createPresetTour(const std::string& profileToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<tptz:CreatePresetTour>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "</tptz:CreatePresetTour>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseCreatePresetTourResponse(resp.body);
}

bool OnvifClient::modifyPresetTour(const std::string& profileToken, const PresetTour& tour)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:ModifyPresetTour>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PresetTour token=\"" << tour.token << "\">\n";
    if (!tour.name.empty()) {
        ss << "    <tt:Name>" << tour.name << "</tt:Name>\n";
    }
    ss << "    <tt:AutoStart>" << (tour.autoStart ? "true" : "false") << "</tt:AutoStart>\n";
    for (const auto& spot : tour.spots) {
        ss << "    <tt:TourSpot>\n"
           << "      <tt:PresetDetail>\n"
           << "        <tt:PresetToken>" << spot.presetToken << "</tt:PresetToken>\n"
           << "      </tt:PresetDetail>\n"
           << "      <tt:Speed>\n"
           << "        <tt:PanTilt x=\"" << std::fixed << std::setprecision(2) << spot.speed << "\" y=\"" << spot.speed
           << "\"/>\n"
           << "      </tt:Speed>\n"
           << "      <tt:StayTime>PT" << spot.stayTimeSeconds << "S</tt:StayTime>\n"
           << "    </tt:TourSpot>\n";
    }
    ss << "  </tptz:PresetTour>\n"
       << "</tptz:ModifyPresetTour>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::operatePresetTour(
    const std::string& profileToken, const std::string& tourToken, PresetTourOperation op)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::string opStr = "Start";
    if (op == PresetTourOperation::Stop) {
        opStr = "Stop";
    } else if (op == PresetTourOperation::Pause) {
        opStr = "Pause";
    }

    std::ostringstream ss {};
    ss << "<tptz:OperatePresetTour>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PresetTourToken>" << tourToken << "</tptz:PresetTourToken>\n"
       << "  <tptz:Operation>" << opStr << "</tptz:Operation>\n"
       << "</tptz:OperatePresetTour>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.ptzXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::removePresetTour(const std::string& profileToken, const std::string& tourToken)
{
    if (m_capabilities.ptzXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    if (m_capabilities.ptzXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<tptz:RemovePresetTour>\n"
       << "  <tptz:ProfileToken>" << profileToken << "</tptz:ProfileToken>\n"
       << "  <tptz:PresetTourToken>" << tourToken << "</tptz:PresetTourToken>\n"
       << "</tptz:RemovePresetTour>";

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

std::optional<FocusStatus20> OnvifClient::getFocusStatus(const std::string& videoSourceToken)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    if (m_capabilities.imagingXAddr.empty()) {
        return std::nullopt;
    }

    std::ostringstream ss {};
    ss << "<timg:GetStatus xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "</timg:GetStatus>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }
    return parseFocusStatusResponse(resp.body);
}

bool OnvifClient::moveFocusContinuous(const std::string& videoSourceToken, float speed)
{
    return moveFocus(videoSourceToken, speed);
}

bool OnvifClient::moveFocusAbsolute(const std::string& videoSourceToken, float position, float speed)
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
       << "    <tt:Absolute>\n"
       << "      <tt:Position>" << std::fixed << std::setprecision(2) << position << "</tt:Position>\n"
       << "      <tt:Speed>" << std::fixed << std::setprecision(2) << speed << "</tt:Speed>\n"
       << "    </tt:Absolute>\n"
       << "  </timg:Focus>\n"
       << "</timg:Move>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::moveFocusRelative(const std::string& videoSourceToken, float distance, float speed)
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
       << "    <tt:Relative>\n"
       << "      <tt:Distance>" << std::fixed << std::setprecision(2) << distance << "</tt:Distance>\n"
       << "      <tt:Speed>" << std::fixed << std::setprecision(2) << speed << "</tt:Speed>\n"
       << "    </tt:Relative>\n"
       << "  </timg:Focus>\n"
       << "</timg:Move>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    return resp.isSuccess();
}

std::vector<ImagingPreset> OnvifClient::getImagingPresets(const std::string& videoSourceToken)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    if (m_capabilities.imagingXAddr.empty()) {
        return {};
    }

    std::ostringstream ss {};
    ss << "<timg:GetPresets xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "</timg:GetPresets>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseImagingPresetsResponse(resp.body);
}

bool OnvifClient::setCurrentImagingPreset(const std::string& videoSourceToken, const std::string& presetToken)
{
    if (m_capabilities.imagingXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    if (m_capabilities.imagingXAddr.empty()) {
        return false;
    }

    std::ostringstream ss {};
    ss << "<timg:SetCurrentPreset xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\n"
       << "  <timg:VideoSourceToken>" << videoSourceToken << "</timg:VideoSourceToken>\n"
       << "  <timg:PresetToken>" << presetToken << "</timg:PresetToken>\n"
       << "</timg:SetCurrentPreset>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(m_capabilities.imagingXAddr, reqXml);
    return resp.isSuccess();
}

std::vector<RelayOutputConfig> OnvifClient::getRelayOutputs()
{
    if (m_capabilities.deviceIoXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    const std::string endpoint
        = !m_capabilities.deviceIoXAddr.empty() ? m_capabilities.deviceIoXAddr : m_deviceEndpoint;

    const std::string body = "<tmd:GetRelayOutputs xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\"/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(endpoint, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseRelayOutputsResponse(resp.body);
}

std::vector<std::string> OnvifClient::getRelayOutputOptions(const std::string& relayToken)
{
    if (m_capabilities.deviceIoXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    const std::string endpoint
        = !m_capabilities.deviceIoXAddr.empty() ? m_capabilities.deviceIoXAddr : m_deviceEndpoint;

    std::ostringstream ss;
    ss << "<tmd:GetRelayOutputOptions xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\n"
       << "  <tmd:RelayOutputToken>" << relayToken << "</tmd:RelayOutputToken>\n"
       << "</tmd:GetRelayOutputOptions>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(endpoint, reqXml);
    if (!resp.isSuccess()) {
        return { "Bistable", "Monostable" };
    }

    std::vector<std::string> options;
    pugi::xml_document doc;
    if (doc.load_string(resp.body.c_str())) {
        const auto optNode = findRecursiveNodeWithSuffix(doc, "RelayOutputOptions");
        if (optNode) {
            for (auto child = optNode.first_child(); child; child = child.next_sibling()) {
                if (std::string(child.name()).find("Mode") != std::string::npos) {
                    options.push_back(child.text().as_string());
                }
            }
        }
    }
    if (options.empty()) {
        options = { "Bistable", "Monostable" };
    }
    return options;
}

bool OnvifClient::setRelayOutputSettings(const std::string& relayToken, const RelayOutputConfig& settings)
{
    if (m_capabilities.deviceIoXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    const std::string endpoint
        = !m_capabilities.deviceIoXAddr.empty() ? m_capabilities.deviceIoXAddr : m_deviceEndpoint;

    std::ostringstream ss;
    ss << "<tmd:SetRelayOutputSettings xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
       << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
       << "  <tmd:RelayOutputToken>" << relayToken << "</tmd:RelayOutputToken>\n"
       << "  <tmd:Properties>\n"
       << "    <tt:Mode>" << relayModeToString(settings.mode) << "</tt:Mode>\n"
       << "    <tt:DelayTime>PT" << static_cast<int>(settings.delayTimeSeconds) << "S</tt:DelayTime>\n"
       << "    <tt:IdleState>" << relayIdleStateToString(settings.idleState) << "</tt:IdleState>\n"
       << "  </tmd:Properties>\n"
       << "</tmd:SetRelayOutputSettings>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(endpoint, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::setRelayOutputState(const std::string& relayToken, RelayLogicalState state)
{
    if (m_capabilities.deviceIoXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    const std::string endpoint
        = !m_capabilities.deviceIoXAddr.empty() ? m_capabilities.deviceIoXAddr : m_deviceEndpoint;

    std::ostringstream ss;
    ss << "<tmd:SetRelayOutputState xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\n"
       << "  <tmd:RelayOutputToken>" << relayToken << "</tmd:RelayOutputToken>\n"
       << "  <tmd:LogicalState>" << relayLogicalStateToString(state) << "</tmd:LogicalState>\n"
       << "</tmd:SetRelayOutputState>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(endpoint, reqXml);
    return resp.isSuccess();
}

std::vector<DigitalInputConfig> OnvifClient::getDigitalInputs()
{
    if (m_capabilities.deviceIoXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }
    const std::string endpoint
        = !m_capabilities.deviceIoXAddr.empty() ? m_capabilities.deviceIoXAddr : m_deviceEndpoint;

    const std::string body = "<tmd:GetDigitalInputs xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\"/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(endpoint, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseDigitalInputsResponse(resp.body);
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

    const std::string body = "<tev:CreatePullPointSubscription xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
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

namespace {

    OsdConfig parseOsdNode(const pugi::xml_node& osdNode)
    {
        OsdConfig osd {};
        osd.token = osdNode.attribute("token").as_string();

        const auto vsn = findRecursiveNodeWithSuffix(osdNode, "VideoSourceConfigurationToken");
        if (vsn) {
            osd.videoSourceToken = vsn.text().as_string();
        }

        const auto posNode = findRecursiveNodeWithSuffix(osdNode, "Position");
        if (posNode) {
            const auto typeNode = findNodeWithSuffix(posNode, "Type");
            const std::string posType = typeNode ? typeNode.text().as_string() : "UpperLeft";
            if (posType == "UpperRight") {
                osd.position = OsdPositionType::UpperRight;
            } else if (posType == "LowerLeft") {
                osd.position = OsdPositionType::LowerLeft;
            } else if (posType == "LowerRight") {
                osd.position = OsdPositionType::LowerRight;
            } else if (posType == "Custom") {
                osd.position = OsdPositionType::Custom;
                const auto pNode = findNodeWithSuffix(posNode, "Pos");
                if (pNode) {
                    osd.customX = pNode.attribute("x").as_float(0.0f);
                    osd.customY = pNode.attribute("y").as_float(0.0f);
                }
            } else {
                osd.position = OsdPositionType::UpperLeft;
            }
        }

        const auto textStringNode = findRecursiveNodeWithSuffix(osdNode, "TextString");
        if (textStringNode) {
            const auto typeNode = findNodeWithSuffix(textStringNode, "Type");
            const std::string textType = typeNode ? typeNode.text().as_string() : "Plain";
            osd.isDateAndTime = (textType == "DateAndTime");

            const auto plainNode = findNodeWithSuffix(textStringNode, "PlainText");
            if (plainNode) {
                osd.plainText = plainNode.text().as_string();
            }

            const auto fsNode = findNodeWithSuffix(textStringNode, "FontSize");
            if (fsNode) {
                osd.fontSize = fsNode.text().as_uint(24U);
            }

            const auto dfNode = findNodeWithSuffix(textStringNode, "DateFormat");
            if (dfNode) {
                osd.dateFormat = dfNode.text().as_string();
            }

            const auto tfNode = findNodeWithSuffix(textStringNode, "TimeFormat");
            if (tfNode) {
                osd.timeFormat = tfNode.text().as_string();
            }
        }

        return osd;
    }

} // namespace

std::vector<OsdConfig> OnvifClient::getOSDs(const std::string& videoSourceConfigurationToken)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::ostringstream ss {};
    ss << "<trt:GetOSDs>\n";
    if (!videoSourceConfigurationToken.empty()) {
        ss << "  <trt:ConfigurationToken>" << videoSourceConfigurationToken << "</trt:ConfigurationToken>\n";
    }
    ss << "</trt:GetOSDs>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }

    return parseOsdListResponse(resp.body);
}

std::optional<OsdConfig> OnvifClient::getOSD(const std::string& osdToken)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::ostringstream ss {};
    ss << "<trt:GetOSD>\n"
       << "  <trt:OSDToken>" << osdToken << "</trt:OSDToken>\n"
       << "</trt:GetOSD>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }

    return parseOsdResponse(resp.body);
}

std::string OnvifClient::createOSD(const OsdConfig& osd)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::string posStr = "UpperLeft";
    if (osd.position == OsdPositionType::UpperRight) {
        posStr = "UpperRight";
    } else if (osd.position == OsdPositionType::LowerLeft) {
        posStr = "LowerLeft";
    } else if (osd.position == OsdPositionType::LowerRight) {
        posStr = "LowerRight";
    } else if (osd.position == OsdPositionType::Custom) {
        posStr = "Custom";
    }

    std::ostringstream ss {};
    ss << "<trt:CreateOSD>\n"
       << "  <trt:OSD";
    if (!osd.token.empty()) {
        ss << " token=\"" << osd.token << "\"";
    }
    ss << ">\n"
       << "    <tt:VideoSourceConfigurationToken>" << osd.videoSourceToken << "</tt:VideoSourceConfigurationToken>\n"
       << "    <tt:Type>Text</tt:Type>\n"
       << "    <tt:Position>\n"
       << "      <tt:Type>" << posStr << "</tt:Type>\n";
    if (osd.position == OsdPositionType::Custom) {
        ss << "      <tt:Pos x=\"" << osd.customX << "\" y=\"" << osd.customY << "\"/>\n";
    }
    ss << "    </tt:Position>\n"
       << "    <tt:TextString>\n"
       << "      <tt:Type>" << (osd.isDateAndTime ? "DateAndTime" : "Plain") << "</tt:Type>\n";
    if (!osd.isDateAndTime) {
        ss << "      <tt:PlainText>" << osd.plainText << "</tt:PlainText>\n";
    } else {
        ss << "      <tt:DateFormat>" << osd.dateFormat << "</tt:DateFormat>\n"
           << "      <tt:TimeFormat>" << osd.timeFormat << "</tt:TimeFormat>\n";
    }
    ss << "      <tt:FontSize>" << osd.fontSize << "</tt:FontSize>\n"
       << "    </tt:TextString>\n"
       << "  </trt:OSD>\n"
       << "</trt:CreateOSD>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }

    auto res = parseCreateOsdResponse(resp.body);
    return res.value_or("");
}

bool OnvifClient::setOSD(const OsdConfig& osd)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::string posStr = "UpperLeft";
    if (osd.position == OsdPositionType::UpperRight) {
        posStr = "UpperRight";
    } else if (osd.position == OsdPositionType::LowerLeft) {
        posStr = "LowerLeft";
    } else if (osd.position == OsdPositionType::LowerRight) {
        posStr = "LowerRight";
    } else if (osd.position == OsdPositionType::Custom) {
        posStr = "Custom";
    }

    std::ostringstream ss {};
    ss << "<trt:SetOSD>\n"
       << "  <trt:OSD token=\"" << osd.token << "\">\n"
       << "    <tt:VideoSourceConfigurationToken>" << osd.videoSourceToken << "</tt:VideoSourceConfigurationToken>\n"
       << "    <tt:Type>Text</tt:Type>\n"
       << "    <tt:Position>\n"
       << "      <tt:Type>" << posStr << "</tt:Type>\n";
    if (osd.position == OsdPositionType::Custom) {
        ss << "      <tt:Pos x=\"" << osd.customX << "\" y=\"" << osd.customY << "\"/>\n";
    }
    ss << "    </tt:Position>\n"
       << "    <tt:TextString>\n"
       << "      <tt:Type>" << (osd.isDateAndTime ? "DateAndTime" : "Plain") << "</tt:Type>\n";
    if (!osd.isDateAndTime) {
        ss << "      <tt:PlainText>" << osd.plainText << "</tt:PlainText>\n";
    } else {
        ss << "      <tt:DateFormat>" << osd.dateFormat << "</tt:DateFormat>\n"
           << "      <tt:TimeFormat>" << osd.timeFormat << "</tt:TimeFormat>\n";
    }
    ss << "      <tt:FontSize>" << osd.fontSize << "</tt:FontSize>\n"
       << "    </tt:TextString>\n"
       << "  </trt:OSD>\n"
       << "</trt:SetOSD>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::deleteOSD(const std::string& osdToken)
{
    if (m_capabilities.mediaXAddr.empty()) {
        static_cast<void>(getCapabilities());
    }

    const std::string targetUrl = !m_capabilities.mediaXAddr.empty() ? m_capabilities.mediaXAddr : m_deviceEndpoint;

    std::ostringstream ss {};
    ss << "<trt:DeleteOSD>\n"
       << "  <trt:OSDToken>" << osdToken << "</trt:OSDToken>\n"
       << "</trt:DeleteOSD>";

    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::vector<OsdConfig> OnvifClient::parseOsdListResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return {};
    }

    std::vector<pugi::xml_node> osdNodes {};
    collectNodesWithSuffix(doc, "OSD", osdNodes);

    std::vector<OsdConfig> result {};
    result.reserve(osdNodes.size());
    for (const auto& node : osdNodes) {
        result.push_back(parseOsdNode(node));
    }
    return result;
}

std::optional<OsdConfig> OnvifClient::parseOsdResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto osdNode = findRecursiveNodeWithSuffix(doc, "OSD");
    if (!osdNode) {
        return std::nullopt;
    }

    return parseOsdNode(osdNode);
}

std::optional<std::string> OnvifClient::parseCreateOsdResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto tokenNode = findRecursiveNodeWithSuffix(doc, "OSDToken");
    if (!tokenNode) {
        return std::nullopt;
    }

    return tokenNode.text().as_string();
}

std::vector<OnvifUser> OnvifClient::parseUsersResponse(const std::string& xml)
{
    std::vector<OnvifUser> users {};
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return users;
    }
    std::vector<pugi::xml_node> userNodes {};
    collectNodesWithSuffix(doc, "User", userNodes);
    for (const auto& uNode : userNodes) {
        OnvifUser u {};
        const auto un = findNodeWithSuffix(uNode, "Username");
        if (un) {
            u.username = un.text().as_string();
        }
        const auto pw = findNodeWithSuffix(uNode, "Password");
        if (pw) {
            u.password = pw.text().as_string();
        }
        const auto ul = findNodeWithSuffix(uNode, "UserLevel");
        if (ul) {
            u.level = userLevelFromString(ul.text().as_string());
        }
        if (!u.username.empty()) {
            users.push_back(u);
        }
    }
    return users;
}

std::vector<OnvifUser> OnvifClient::getUsers()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetUsers/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseUsersResponse(resp.body);
}

bool OnvifClient::createUsers(const std::vector<OnvifUser>& users)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:CreateUsers>";
    for (const auto& u : users) {
        ss << "<tds:User>"
           << "<tt:Username>" << u.username << "</tt:Username>"
           << "<tt:Password>" << u.password << "</tt:Password>"
           << "<tt:UserLevel>" << userLevelToString(u.level) << "</tt:UserLevel>"
           << "</tds:User>";
    }
    ss << "</tds:CreateUsers>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::setUser(const OnvifUser& user)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetUser>"
       << "<tds:User>"
       << "<tt:Username>" << user.username << "</tt:Username>";
    if (!user.password.empty()) {
        ss << "<tt:Password>" << user.password << "</tt:Password>";
    }
    ss << "<tt:UserLevel>" << userLevelToString(user.level) << "</tt:UserLevel>"
       << "</tds:User>"
       << "</tds:SetUser>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::deleteUsers(const std::vector<std::string>& usernames)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:DeleteUsers>";
    for (const auto& name : usernames) {
        ss << "<tds:Username>" << name << "</tds:Username>";
    }
    ss << "</tds:DeleteUsers>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::vector<NetworkInterfaceConfig> OnvifClient::parseNetworkInterfacesResponse(const std::string& xml)
{
    std::vector<NetworkInterfaceConfig> ifaces {};
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return ifaces;
    }
    std::vector<pugi::xml_node> ifaceNodes {};
    collectNodesWithSuffix(doc, "NetworkInterfaces", ifaceNodes);
    for (const auto& node : ifaceNodes) {
        NetworkInterfaceConfig cfg {};
        cfg.token = node.attribute("token").as_string();
        const auto en = findNodeWithSuffix(node, "Enabled");
        if (en) {
            cfg.enabled = en.text().as_bool(true);
        }
        const auto info = findNodeWithSuffix(node, "Info");
        if (info) {
            const auto nm = findNodeWithSuffix(info, "Name");
            if (nm) {
                cfg.name = nm.text().as_string();
            }
            const auto hw = findNodeWithSuffix(info, "HwAddress");
            if (hw) {
                cfg.hwAddress = hw.text().as_string();
            }
            const auto mtu = findNodeWithSuffix(info, "MTU");
            if (mtu) {
                cfg.mtu = mtu.text().as_int(1500);
            }
        }
        const auto ipv4 = findNodeWithSuffix(node, "IPv4");
        if (ipv4) {
            const auto ipv4En = findNodeWithSuffix(ipv4, "Enabled");
            if (ipv4En) {
                cfg.ipv4.enabled = ipv4En.text().as_bool(true);
            }
            const auto conf = findNodeWithSuffix(ipv4, "Config");
            if (conf) {
                const auto dhcp = findNodeWithSuffix(conf, "DHCP");
                if (dhcp) {
                    cfg.ipv4.dhcp = dhcp.text().as_bool(false);
                }
                const auto manual = findNodeWithSuffix(conf, "Manual");
                if (manual) {
                    const auto addr = findNodeWithSuffix(manual, "Address");
                    if (addr) {
                        cfg.ipv4.manualAddress = addr.text().as_string();
                    }
                    const auto pfx = findNodeWithSuffix(manual, "PrefixLength");
                    if (pfx) {
                        cfg.ipv4.prefixLength = pfx.text().as_int(24);
                    }
                }
            }
        }
        if (cfg.name.empty() && !cfg.token.empty()) {
            cfg.name = cfg.token;
        }
        if (!cfg.token.empty()) {
            ifaces.push_back(cfg);
        }
    }
    return ifaces;
}

std::vector<NetworkInterfaceConfig> OnvifClient::getNetworkInterfaces()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetNetworkInterfaces/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseNetworkInterfacesResponse(resp.body);
}

bool OnvifClient::setNetworkInterfaces(const NetworkInterfaceConfig& config)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetNetworkInterfaces>"
       << "<tds:InterfaceToken>" << config.token << "</tds:InterfaceToken>"
       << "<tds:NetworkInterface>"
       << "<tt:Enabled>" << (config.enabled ? "true" : "false") << "</tt:Enabled>"
       << "<tt:MTU>" << config.mtu << "</tt:MTU>"
       << "<tt:IPv4>"
       << "<tt:Enabled>" << (config.ipv4.enabled ? "true" : "false") << "</tt:Enabled>"
       << "<tt:Manual>"
       << "<tt:Address>" << config.ipv4.manualAddress << "</tt:Address>"
       << "<tt:PrefixLength>" << config.ipv4.prefixLength << "</tt:PrefixLength>"
       << "</tt:Manual>"
       << "<tt:DHCP>" << (config.ipv4.dhcp ? "true" : "false") << "</tt:DHCP>"
       << "</tt:IPv4>"
       << "</tds:NetworkInterface>"
       << "</tds:SetNetworkInterfaces>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::string OnvifClient::parseNetworkDefaultGatewayResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return {};
    }
    const auto gwNode = findRecursiveNodeWithSuffix(doc, "IPv4Address");
    if (gwNode) {
        return gwNode.text().as_string();
    }
    return {};
}

std::string OnvifClient::getNetworkDefaultGateway()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetNetworkDefaultGateway/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseNetworkDefaultGatewayResponse(resp.body);
}

bool OnvifClient::setNetworkDefaultGateway(const std::string& gateway)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetNetworkDefaultGateway>"
       << "<tds:IPv4Address>" << gateway << "</tds:IPv4Address>"
       << "</tds:SetNetworkDefaultGateway>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::optional<DnsConfig> OnvifClient::parseDnsResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }
    const auto dnsNode = findRecursiveNodeWithSuffix(doc, "DNSInformation");
    if (!dnsNode) {
        return std::nullopt;
    }
    DnsConfig cfg {};
    const auto dhcp = findNodeWithSuffix(dnsNode, "FromDHCP");
    if (dhcp) {
        cfg.fromDhcp = dhcp.text().as_bool(false);
    }
    std::vector<pugi::xml_node> sdNodes {};
    collectNodesWithSuffix(dnsNode, "SearchDomain", sdNodes);
    for (const auto& sd : sdNodes) {
        cfg.searchDomains.push_back(sd.text().as_string());
    }
    std::vector<pugi::xml_node> manualNodes {};
    collectNodesWithSuffix(dnsNode, "DNSManual", manualNodes);
    for (const auto& m : manualNodes) {
        const auto ip = findNodeWithSuffix(m, "IPv4Address");
        if (ip) {
            cfg.dnsServers.push_back(ip.text().as_string());
        }
    }
    return cfg;
}

std::optional<DnsConfig> OnvifClient::getDNS()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetDNS/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }
    return parseDnsResponse(resp.body);
}

bool OnvifClient::setDNS(const DnsConfig& dns)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetDNS>"
       << "<tds:FromDHCP>" << (dns.fromDhcp ? "true" : "false") << "</tds:FromDHCP>";
    for (const auto& sd : dns.searchDomains) {
        ss << "<tds:SearchDomain>" << sd << "</tds:SearchDomain>";
    }
    for (const auto& server : dns.dnsServers) {
        ss << "<tds:DNSManual>"
           << "<tt:Type>IPv4</tt:Type>"
           << "<tt:IPv4Address>" << server << "</tt:IPv4Address>"
           << "</tds:DNSManual>";
    }
    ss << "</tds:SetDNS>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::optional<NtpConfig> OnvifClient::parseNtpResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }
    const auto ntpNode = findRecursiveNodeWithSuffix(doc, "NTPInformation");
    if (!ntpNode) {
        return std::nullopt;
    }
    NtpConfig cfg {};
    const auto dhcp = findNodeWithSuffix(ntpNode, "FromDHCP");
    if (dhcp) {
        cfg.fromDhcp = dhcp.text().as_bool(false);
    }
    std::vector<pugi::xml_node> manualNodes {};
    collectNodesWithSuffix(ntpNode, "NTPManual", manualNodes);
    for (const auto& m : manualNodes) {
        const auto dnsNm = findNodeWithSuffix(m, "DNSname");
        if (dnsNm) {
            cfg.manualServers.push_back(dnsNm.text().as_string());
        } else {
            const auto ip = findNodeWithSuffix(m, "IPv4Address");
            if (ip) {
                cfg.manualServers.push_back(ip.text().as_string());
            }
        }
    }
    return cfg;
}

std::optional<NtpConfig> OnvifClient::getNTP()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetNTP/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return std::nullopt;
    }
    return parseNtpResponse(resp.body);
}

bool OnvifClient::setNTP(const NtpConfig& ntp)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetNTP>"
       << "<tds:FromDHCP>" << (ntp.fromDhcp ? "true" : "false") << "</tds:FromDHCP>";
    for (const auto& srv : ntp.manualServers) {
        ss << "<tds:NTPManual>"
           << "<tt:Type>DNS</tt:Type>"
           << "<tt:DNSname>" << srv << "</tt:DNSname>"
           << "</tds:NTPManual>";
    }
    ss << "</tds:SetNTP>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::string OnvifClient::parseHostnameResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return {};
    }
    const auto nameNode = findRecursiveNodeWithSuffix(doc, "Name");
    if (nameNode) {
        return nameNode.text().as_string();
    }
    return {};
}

std::string OnvifClient::getHostname()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetHostname/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseHostnameResponse(resp.body);
}

bool OnvifClient::setHostname(const std::string& hostname)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetHostname>"
       << "<tds:Name>" << hostname << "</tds:Name>"
       << "</tds:SetHostname>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::setSystemDateAndTime(const SystemDateTimeConfig& dt)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetSystemDateAndTime>"
       << "<tds:DateTimeType>" << dt.dateTimeType << "</tds:DateTimeType>"
       << "<tds:DaylightSavings>" << (dt.daylightSavings ? "true" : "false") << "</tds:DaylightSavings>"
       << "<tds:TimeZone><tt:TZ>" << dt.timeZone << "</tt:TZ></tds:TimeZone>"
       << "<tds:UTCDateTime>"
       << "<tt:Time>"
       << "<tt:Hour>" << dt.hour << "</tt:Hour>"
       << "<tt:Minute>" << dt.minute << "</tt:Minute>"
       << "<tt:Second>" << dt.second << "</tt:Second>"
       << "</tt:Time>"
       << "<tt:Date>"
       << "<tt:Year>" << dt.year << "</tt:Year>"
       << "<tt:Month>" << dt.month << "</tt:Month>"
       << "<tt:Day>" << dt.day << "</tt:Day>"
       << "</tt:Date>"
       << "</tds:UTCDateTime>"
       << "</tds:SetSystemDateAndTime>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::setSystemFactoryDefault(FactoryDefaultType type)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string defStr = (type == FactoryDefaultType::Hard) ? "Hard" : "Soft";
    const std::string body = "<tds:SetSystemFactoryDefault><tds:FactoryDefault>" + defStr
        + "</tds:FactoryDefault></tds:SetSystemFactoryDefault>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::vector<std::string> OnvifClient::parseScopesResponse(const std::string& xml)
{
    std::vector<std::string> scopes {};
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return scopes;
    }
    std::vector<pugi::xml_node> scopeNodes {};
    collectNodesWithSuffix(doc, "ScopeItem", scopeNodes);
    for (const auto& s : scopeNodes) {
        scopes.push_back(s.text().as_string());
    }
    return scopes;
}

std::vector<std::string> OnvifClient::getScopes()
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    const std::string body = "<tds:GetScopes/>";
    const std::string reqXml = wrapSoapEnvelope(body);
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    if (!resp.isSuccess()) {
        return {};
    }
    return parseScopesResponse(resp.body);
}

bool OnvifClient::addScopes(const std::vector<std::string>& scopes)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:AddScopes>";
    for (const auto& s : scopes) {
        ss << "<tds:ScopeItem>" << s << "</tds:ScopeItem>";
    }
    ss << "</tds:AddScopes>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::removeScopes(const std::vector<std::string>& scopes)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:RemoveScopes>";
    for (const auto& s : scopes) {
        ss << "<tds:ScopeItem>" << s << "</tds:ScopeItem>";
    }
    ss << "</tds:RemoveScopes>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

bool OnvifClient::setScopes(const std::vector<std::string>& scopes)
{
    const std::string targetUrl = !m_capabilities.deviceXAddr.empty() ? m_capabilities.deviceXAddr : m_deviceEndpoint;
    std::ostringstream ss {};
    ss << "<tds:SetScopes>";
    for (const auto& s : scopes) {
        ss << "<tds:ScopeItem>" << s << "</tds:ScopeItem>";
    }
    ss << "</tds:SetScopes>";
    const std::string reqXml = wrapSoapEnvelope(ss.str());
    const HttpResponse resp = m_httpClient.sendPost(targetUrl, reqXml);
    return resp.isSuccess();
}

std::optional<FocusStatus20> OnvifClient::parseFocusStatusResponse(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return std::nullopt;
    }

    const auto statusNode = findRecursiveNodeWithSuffix(doc, "FocusStatus20");
    if (!statusNode) {
        return std::nullopt;
    }

    FocusStatus20 st;
    const auto pos = findNodeWithSuffix(statusNode, "Position");
    if (pos) {
        st.position = pos.text().as_float(0.0f);
    }
    const auto move = findNodeWithSuffix(statusNode, "MoveStatus");
    if (move) {
        st.moveStatus = move.text().as_string("IDLE");
    }
    const auto err = findNodeWithSuffix(statusNode, "Error");
    if (err) {
        st.error = err.text().as_string();
    }
    return st;
}

std::vector<ImagingPreset> OnvifClient::parseImagingPresetsResponse(const std::string& xml)
{
    std::vector<ImagingPreset> presets;
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return presets;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "GetPresetsResponse");
    if (!respNode) {
        return presets;
    }

    for (auto child = respNode.first_child(); child; child = child.next_sibling()) {
        if (std::string(child.name()).find("Preset") != std::string::npos) {
            ImagingPreset p;
            p.token = child.attribute("token").as_string();
            p.type = child.attribute("type").as_string("Custom");
            const auto nameNode = findNodeWithSuffix(child, "Name");
            if (nameNode) {
                p.name = nameNode.text().as_string();
            }
            presets.push_back(p);
        }
    }
    return presets;
}

std::vector<RelayOutputConfig> OnvifClient::parseRelayOutputsResponse(const std::string& xml)
{
    std::vector<RelayOutputConfig> relays;
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return relays;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "GetRelayOutputsResponse");
    if (!respNode) {
        return relays;
    }

    for (auto child = respNode.first_child(); child; child = child.next_sibling()) {
        if (std::string(child.name()).find("RelayOutputs") != std::string::npos) {
            RelayOutputConfig r;
            r.token = child.attribute("token").as_string();

            const auto propNode = findNodeWithSuffix(child, "Properties");
            if (propNode) {
                const auto modeNode = findNodeWithSuffix(propNode, "Mode");
                if (modeNode) {
                    r.mode = relayModeFromString(modeNode.text().as_string());
                }
                const auto delayNode = findNodeWithSuffix(propNode, "DelayTime");
                if (delayNode) {
                    const std::string dt = delayNode.text().as_string();
                    const auto posT = dt.find('T');
                    const auto posS = dt.find('S');
                    if (posT != std::string::npos && posS != std::string::npos && posS > posT + 1) {
                        try {
                            r.delayTimeSeconds = std::stof(dt.substr(posT + 1, posS - posT - 1));
                        } catch (...) {
                        }
                    }
                }
                const auto idleNode = findNodeWithSuffix(propNode, "IdleState");
                if (idleNode) {
                    r.idleState = relayIdleStateFromString(idleNode.text().as_string());
                }
            }

            const auto stateNode = findNodeWithSuffix(child, "LogicalState");
            if (stateNode) {
                r.logicalState = relayLogicalStateFromString(stateNode.text().as_string());
            }

            relays.push_back(r);
        }
    }
    return relays;
}

std::vector<DigitalInputConfig> OnvifClient::parseDigitalInputsResponse(const std::string& xml)
{
    std::vector<DigitalInputConfig> inputs;
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return inputs;
    }

    const auto respNode = findRecursiveNodeWithSuffix(doc, "GetDigitalInputsResponse");
    if (!respNode) {
        return inputs;
    }

    for (auto child = respNode.first_child(); child; child = child.next_sibling()) {
        if (std::string(child.name()).find("DigitalInputs") != std::string::npos) {
            DigitalInputConfig in;
            in.token = child.attribute("token").as_string();
            const auto idleNode = findNodeWithSuffix(child, "IdleState");
            if (idleNode) {
                in.idleState = relayIdleStateFromString(idleNode.text().as_string());
            }
            inputs.push_back(in);
        }
    }
    return inputs;
}

} // namespace PelcoD::Onvif
