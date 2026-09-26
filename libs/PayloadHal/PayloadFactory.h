#pragma once

/// @file PayloadFactory.h
/// @brief Factory for instantiating concrete and simulated IPayload stations.

#include "IPayload.h"
#include "Onvif/OnvifClient.h"
#include "PelcoDCore/PelcoDDevice.h"
#include "PelcoDFujinon/FujinonSX800Device.h"
#include "SonyFCBDevice.h"

#include <memory>
#include <string>

namespace PayloadHal {

/// @class PayloadFactory
/// @brief Instantiation engine resolving URI endpoints and hardware protocols into IPayload stations.
class PayloadFactory {
public:
    /// @brief Creates an IPayload instance from a unified connection URI.
    /// @details Supported schemes:
    ///          - "sim://": Instantiates a SimulatedPayload multi-sensor station.
    ///          - "onvif://[user:password@]host[:port][/device_service_path]": Instantiates an OnvifPayloadAdapter.
    ///          - "pelcod://host[:port][?addr=N][&proto=tcp|udp]": Network Pelco-D PTZ station.
    ///          - "pelcod:///dev/ttyX[?baud=N][&addr=N]": Serial Pelco-D PTZ station.
    ///          - "serial:///dev/ttyX?baud=N&protocol=pelcod|fujinon[&addr=N]": Serial protocol endpoint.
    ///          - "tcp://host:port?protocol=pelcod|fujinon[&addr=N]": Raw TCP socket endpoint.
    ///          - "udp://host:port?protocol=pelcod|fujinon[&addr=N]": Raw UDP socket endpoint.
    ///          - "fujinon://host[:port][?addr=N]": Fujinon SX800 long-range camera payload.
    ///          - "pelcod-visca://...": Composite station combining Pelco-D PT and Sony FCB optics.
    /// @param[in] uri Unified connection URI.
    /// @return Shared pointer to configured IPayload station, or nullptr if URI is unsupported or malformed.
    [[nodiscard]] static std::shared_ptr<IPayload> createFromUri(const std::string& uri);

    /// @brief Creates a fully simulated multi-sensor payload station (Gimbal + EO + Thermal + LRF).
    /// @return Shared pointer to SimulatedPayload.
    [[nodiscard]] static std::shared_ptr<IPayload> createSimulatedPayload();

    /// @brief Creates an IPayload station wrapping a standard Pelco-D device.
    /// @param[in] device Underlying PelcoDDevice instance.
    /// @return Shared pointer to composite IPayload.
    [[nodiscard]] static std::shared_ptr<IPayload> createPelcoDPayload(std::shared_ptr<PelcoD::PelcoDDevice> device);

    /// @brief Creates an IPayload station wrapping a Fujinon SX800 camera and PT head.
    /// @param[in] device Underlying FujinonSX800Device instance.
    /// @return Shared pointer to PelcoDFujinonPayloadAdapter.
    [[nodiscard]] static std::shared_ptr<IPayload> createFujinonPayload(
        std::shared_ptr<PelcoD::FujinonSX800Device> device);

    /// @brief Creates an IPayload station wrapping a Pelco-D PT head and Sony FCB block camera.
    /// @param[in] ptzDevice Underlying PelcoDDevice instance.
    /// @param[in] cameraDevice Underlying SonyFCBDevice instance.
    /// @return Shared pointer to PelcoDViscaCompositePayload.
    [[nodiscard]] static std::shared_ptr<IPayload> createPelcoDViscaPayload(
        std::shared_ptr<PelcoD::PelcoDDevice> ptzDevice, std::shared_ptr<Visca::Sony::SonyFCBDevice> cameraDevice);

    /// @brief Creates an IPayload station wrapping arbitrary IPanTiltUnit and ICameraPayload instances.
    /// @param[in] ptu Underlying IPanTiltUnit instance.
    /// @param[in] camera Underlying ICameraPayload instance.
    /// @return Shared pointer to PelcoDViscaCompositePayload.
    [[nodiscard]] static std::shared_ptr<IPayload> createPelcoDViscaPayload(
        std::shared_ptr<IPanTiltUnit> ptu, std::shared_ptr<ICameraPayload> camera);

    /// @brief Creates an IPayload station wrapping an ONVIF Profile S/T camera.
    /// @param[in] client Underlying OnvifClient instance.
    /// @param[in] profileToken Optional active media profile token.
    /// @return Shared pointer to OnvifPayloadAdapter.
    [[nodiscard]] static std::shared_ptr<IPayload> createOnvifPayload(
        std::shared_ptr<Onvif::OnvifClient> client, const std::string& profileToken = "");

    /// @brief Creates an IPayload station connecting to an ONVIF camera endpoint.
    /// @param[in] deviceEndpoint Camera device service HTTP URL.
    /// @param[in] credentials Authentication credentials.
    /// @param[in] profileToken Optional active media profile token.
    /// @return Shared pointer to OnvifPayloadAdapter.
    [[nodiscard]] static std::shared_ptr<IPayload> createOnvifPayload(const std::string& deviceEndpoint,
        const Onvif::SecurityCredentials& credentials = {}, const std::string& profileToken = "");

    /// @brief Creates an ILaserRangeFinder instance from a connection URI.
    /// @details Supported formats:
    ///          - "lrf://serial/COM3?baud=115200&proto=nmea"
    ///          - "lrf://tcp/192.168.1.100:4001?proto=ascii"
    ///          - "lrf://udp/192.168.1.100:4001?proto=binary"
    ///          - "lrf://sim": Simulated LRF
    /// @param[in] uri LRF connection URI string.
    /// @return Shared pointer to ILaserRangeFinder, or nullptr if unsupported/malformed.
    [[nodiscard]] static std::shared_ptr<ILaserRangeFinder> createLrfFromUri(const std::string& uri);
};

} // namespace PayloadHal
