#pragma once

/// @file DeviceSystemAdapter.h
/// @brief Adapter managing ONVIF Device Management and DeviceIO subsystem features.

#include <cstdint>

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"

namespace Onvif {

/// @class DeviceSystemAdapter
/// @brief Manages system-level telemetry, security certificates, relay I/O, and device location.
/// @details Implements IDeviceManagementHandler and IDeviceIoHandler per ISP segregation.
class DeviceSystemAdapter : public IDeviceManagementHandler, public IDeviceIoHandler {
public:
    /// @brief Constructs default DeviceSystemAdapter with self-signed certificate and default relays.
    DeviceSystemAdapter();

    ~DeviceSystemAdapter() override = default;

    DeviceSystemAdapter(const DeviceSystemAdapter&) = delete;
    DeviceSystemAdapter& operator=(const DeviceSystemAdapter&) = delete;
    DeviceSystemAdapter(DeviceSystemAdapter&&) = delete;
    DeviceSystemAdapter& operator=(DeviceSystemAdapter&&) = delete;

    /// @brief Registers an event publisher callback for relay state changes.
    /// @param[in] publisher Callback function invoked when relay triggers.
    void setEventPublisher(EventCallback publisher);

    // =========================================================================
    // IDeviceIoHandler Implementation
    // =========================================================================

    /// @brief Retrieves all configured relay outputs.
    /// @return Vector of RelayOutputConfig.
    [[nodiscard]] std::vector<RelayOutputConfig> handleGetRelayOutputs() override;

    /// @brief Retrieves configuration options for a given relay.
    /// @param[in] token Relay token.
    /// @return Vector of supported modes ("Monostable", "Bistable").
    [[nodiscard]] std::vector<std::string> handleGetRelayOutputOptions(const std::string& token) override;

    /// @brief Applies updated mode, delay time, or idle state to a relay.
    /// @param[in] token Relay token.
    /// @param[in] settings Updated configuration parameters.
    /// @return True if configuration was applied.
    [[nodiscard]] bool handleSetRelayOutputSettings(
        const std::string& token, const RelayOutputConfig& settings) override;

    /// @brief Changes the logical state (Active/Inactive) of a relay.
    /// @param[in] token Relay token.
    /// @param[in] state Desired logical state.
    /// @return True if state was changed.
    [[nodiscard]] bool handleSetRelayOutputState(const std::string& token, RelayLogicalState state) override;

    /// @brief Retrieves all configured digital inputs.
    /// @return Vector of DigitalInputConfig.
    [[nodiscard]] std::vector<DigitalInputConfig> handleGetDigitalInputs() override;

    // =========================================================================
    // IDeviceManagementHandler Implementation
    // =========================================================================

    /// @brief Retrieves system or access log content.
    /// @param[in] logType Type of log requested.
    /// @return String containing log lines.
    [[nodiscard]] std::string handleGetSystemLog(SystemLogType logType) override;

    /// @brief Generates comprehensive system and diagnostic support telemetry.
    /// @return SystemSupportInfo structure.
    [[nodiscard]] SystemSupportInfo handleGetSystemSupportInfo() override;

    /// @brief Backs up system settings, returning an archive payload string.
    /// @return Serialized backup payload.
    [[nodiscard]] std::string handleGetSystemBackup() override;

    /// @brief Restores system settings from an archive payload string.
    /// @param[in] backupData Backup payload to restore.
    /// @return True on success.
    [[nodiscard]] bool handleRestoreSystem(const std::string& backupData) override;

    /// @brief Retrieves device unique endpoint reference URN.
    /// @return URN endpoint string.
    [[nodiscard]] std::string handleGetEndpointReference() override;

    /// @brief Retrieves list of installed X.509 certificates.
    /// @return Vector of OnvifCertificate records.
    [[nodiscard]] std::vector<OnvifCertificate> handleGetCertificates() override;

    /// @brief Retrieves detailed information for a specific certificate ID.
    /// @param[in] certificateId Certificate identifier.
    /// @return CertificateInformation struct or nullopt if not found.
    [[nodiscard]] std::optional<CertificateInformation> handleGetCertInformation(
        const std::string& certificateId) override;

    /// @brief Generates a self-signed X.509 certificate on device.
    /// @param[in] certificateId Desired certificate token.
    /// @param[in] subject Distinguished name.
    /// @param[in] daysValid Validity period in days.
    /// @return Created OnvifCertificate.
    [[nodiscard]] OnvifCertificate handleCreateCertificate(
        const std::string& certificateId, const std::string& subject, int daysValid = 365) override;

    /// @brief Generates a PKCS#10 Certificate Signing Request (CSR).
    /// @param[in] certificateId Certificate identifier.
    /// @param[in] subject Subject DN.
    /// @return Pkcs10Request struct with Base64/PEM CSR.
    [[nodiscard]] Pkcs10Request handleGetPkcs10Request(
        const std::string& certificateId, const std::string& subject) override;

    /// @brief Loads or updates signed certificates onto the device.
    /// @param[in] certificates List of certificates to store.
    /// @return True on success.
    [[nodiscard]] bool handleLoadCertificates(const std::vector<OnvifCertificate>& certificates) override;

    /// @brief Deletes a certificate by token ID.
    /// @param[in] certificateId Certificate token to delete.
    /// @return True if deleted.
    [[nodiscard]] bool handleDeleteCertificate(const std::string& certificateId) override;

    /// @brief Queries client certificate authentication mode.
    /// @return ClientCertificateMode enum.
    [[nodiscard]] ClientCertificateMode handleGetClientCertMode() override;

    /// @brief Configures client certificate authentication mode.
    /// @param[in] mode Desired client certificate mode.
    /// @return True on success.
    bool handleSetClientCertMode(ClientCertificateMode mode) override;

    /// @brief Queries device geographic location and mounting orientation.
    /// @param[in] entityToken Entity identifier.
    /// @return LocationEntity if configured.
    [[nodiscard]] std::optional<LocationEntity> handleGetGeoLocation(const std::string& entityToken) override;

    /// @brief Updates device geographic location and mounting orientation.
    /// @param[in] location Updated LocationEntity.
    /// @return True on success.
    [[nodiscard]] bool handleSetGeoLocation(const LocationEntity& location) override;

    /// @brief Deletes device geographic location configuration.
    /// @param[in] entityToken Entity identifier.
    /// @return True if deleted.
    [[nodiscard]] bool handleDeleteGeoLocation(const std::string& entityToken) override;

    /// @brief Gets the configured camera mounting location.
    /// @return LocationEntity snapshot.
    [[nodiscard]] LocationEntity cameraLocation() const;

    /// @brief Sets the camera mounting location and orientation.
    /// @param[in] location Target LocationEntity.
    void setCameraLocation(const LocationEntity& location);

    /// @brief Registers a callback to receive location updates.
    /// @param[in] cb Callback invoked when geolocation changes.
    void setLocationCallback(std::function<void(const LocationEntity&)> cb);

private:
    mutable std::mutex m_mutex { };
    EventCallback m_eventPublisher { };
    std::function<void(const LocationEntity&)> m_locationCallback { };
    std::vector<RelayOutputConfig> m_relays { { "Relay_1", RelayMode::Bistable, 0.0f, RelayIdleState::Open,
                                                  RelayLogicalState::Inactive },
        { "Relay_2", RelayMode::Bistable, 0.0f, RelayIdleState::Open, RelayLogicalState::Inactive } };
    std::vector<DigitalInputConfig> m_digitalInputs { { "Input_1", RelayIdleState::Open, "Alarm", false } };
    std::vector<OnvifCertificate> m_certificates { };
    ClientCertificateMode m_clientCertMode { ClientCertificateMode::Off };
    LocationEntity m_cameraLocation { "Device", "Location_1", true, { 37.7749, -122.4194, 10.0 }, { 0.0, 0.0, 0.0 } };
};

} // namespace Onvif
