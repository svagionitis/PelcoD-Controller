#include "DeviceSystemAdapter.h"

#include <cstdint>

#include <algorithm>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "OnvifSecurity.h"

namespace Onvif {

DeviceSystemAdapter::DeviceSystemAdapter()
{
    m_certificates.push_back(
        OnvifSecurity::generateSelfSignedCertificate("Cert_Server", "CN=PelcoD-Camera, O=PelcoD", 365));
}

void DeviceSystemAdapter::setEventPublisher(EventCallback publisher)
{
    std::scoped_lock lock(m_mutex);
    m_eventPublisher = std::move(publisher);
}

// =========================================================================
// IDeviceIoHandler Implementation
// =========================================================================

std::vector<RelayOutputConfig> DeviceSystemAdapter::handleGetRelayOutputs()
{
    std::scoped_lock lock(m_mutex);
    return m_relays;
}

std::vector<std::string> DeviceSystemAdapter::handleGetRelayOutputOptions(const std::string& /*token*/)
{
    return { "Bistable", "Monostable" };
}

bool DeviceSystemAdapter::handleSetRelayOutputSettings(const std::string& token, const RelayOutputConfig& settings)
{
    std::scoped_lock lock(m_mutex);
    for (auto& r : m_relays) {
        if (r.token == token) {
            r = settings;
            return true;
        }
    }
    return false;
}

bool DeviceSystemAdapter::handleSetRelayOutputState(const std::string& token, RelayLogicalState state)
{
    EventCallback publisher { };
    bool changed = false;
    {
        std::scoped_lock lock(m_mutex);
        for (auto& r : m_relays) {
            if (r.token == token) {
                r.logicalState = state;
                changed = true;
                publisher = m_eventPublisher;
                break;
            }
        }
    }

    if (changed && publisher) {
        OnvifEvent ev { };
        ev.topic = "tns1:Device/Trigger/Relay";
        ev.sourceName = "Relay";
        ev.sourceValue = token;
        ev.dataName = "LogicalState";
        ev.dataValue = (state == RelayLogicalState::Active) ? "active" : "inactive";
        publisher(ev);
    }

    return changed;
}

std::vector<DigitalInputConfig> DeviceSystemAdapter::handleGetDigitalInputs()
{
    std::scoped_lock lock(m_mutex);
    return m_digitalInputs;
}

// =========================================================================
// IDeviceManagementHandler Implementation
// =========================================================================

std::string DeviceSystemAdapter::handleGetSystemLog(SystemLogType logType)
{
    if (logType == SystemLogType::Access) {
        return "2026-09-17T00:00:00Z [INFO] ACCESS LOG: admin logged in from 192.168.1.100\n";
    }
    return "2026-09-17T00:00:00Z [INFO] Pelco-D PTZ Device initialised successfully on address 1\n";
}

SystemSupportInfo DeviceSystemAdapter::handleGetSystemSupportInfo()
{
    SystemSupportInfo info { };
    info.rawDiagnostics = "Pelco-D Controller Support Info v1.0 (Core HW: Virtual-PTZ-Bridge)";
    return info;
}

std::string DeviceSystemAdapter::handleGetSystemBackup()
{
    return "{\"configVersion\":\"1.0\",\"deviceName\":\"Pelco-D Bridge\"}";
}

bool DeviceSystemAdapter::handleRestoreSystem(const std::string& backupData)
{
    return !backupData.empty();
}

std::string DeviceSystemAdapter::handleGetEndpointReference()
{
    return "urn:uuid:12345678-1234-5678-1234-567812345678";
}

std::vector<OnvifCertificate> DeviceSystemAdapter::handleGetCertificates()
{
    std::scoped_lock lock(m_mutex);
    return m_certificates;
}

std::optional<CertificateInformation> DeviceSystemAdapter::handleGetCertInformation(const std::string& certificateId)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& c : m_certificates) {
        if (c.certificateId == certificateId) {
            return c.info;
        }
    }
    return std::nullopt;
}

OnvifCertificate DeviceSystemAdapter::handleCreateCertificate(
    const std::string& certificateId, const std::string& subject, std::int32_t daysValid)
{
    std::scoped_lock lock(m_mutex);
    auto cert = OnvifSecurity::generateSelfSignedCertificate(certificateId, subject, daysValid);
    m_certificates.push_back(cert);
    return cert;
}

Pkcs10Request DeviceSystemAdapter::handleGetPkcs10Request(const std::string& certificateId, const std::string& subject)
{
    return OnvifSecurity::generatePkcs10Csr(certificateId, subject);
}

bool DeviceSystemAdapter::handleLoadCertificates(const std::vector<OnvifCertificate>& certificates)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& newCert : certificates) {
        bool replaced = false;
        for (auto& existing : m_certificates) {
            if (existing.certificateId == newCert.certificateId) {
                existing = newCert;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            m_certificates.push_back(newCert);
        }
    }
    return true;
}

bool DeviceSystemAdapter::handleDeleteCertificate(const std::string& certificateId)
{
    std::scoped_lock lock(m_mutex);
    const auto origSize = m_certificates.size();
    m_certificates.erase(std::remove_if(m_certificates.begin(), m_certificates.end(),
                             [&certificateId](const OnvifCertificate& c) { return c.certificateId == certificateId; }),
        m_certificates.end());
    return m_certificates.size() < origSize;
}

ClientCertificateMode DeviceSystemAdapter::handleGetClientCertMode()
{
    std::scoped_lock lock(m_mutex);
    return m_clientCertMode;
}

bool DeviceSystemAdapter::handleSetClientCertMode(ClientCertificateMode mode)
{
    std::scoped_lock lock(m_mutex);
    m_clientCertMode = mode;
    return true;
}

std::optional<LocationEntity> DeviceSystemAdapter::handleGetGeoLocation(const std::string& /*entityToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_cameraLocation;
}

bool DeviceSystemAdapter::handleSetGeoLocation(const LocationEntity& location)
{
    std::function<void(const LocationEntity&)> cb;
    {
        std::scoped_lock lock(m_mutex);
        m_cameraLocation = location;
        cb = m_locationCallback;
    }
    if (cb) {
        cb(location);
    }
    return true;
}

bool DeviceSystemAdapter::handleDeleteGeoLocation(const std::string& /*entityToken*/)
{
    std::scoped_lock lock(m_mutex);
    m_cameraLocation = LocationEntity { };
    return true;
}

LocationEntity DeviceSystemAdapter::cameraLocation() const
{
    std::scoped_lock lock(m_mutex);
    return m_cameraLocation;
}

void DeviceSystemAdapter::setCameraLocation(const LocationEntity& location)
{
    std::scoped_lock lock(m_mutex);
    m_cameraLocation = location;
}

void DeviceSystemAdapter::setLocationCallback(std::function<void(const LocationEntity&)> cb)
{
    std::scoped_lock lock(m_mutex);
    m_locationCallback = std::move(cb);
}

} // namespace Onvif
