#pragma once

/// @file PayloadFactory.h
/// @brief Factory for instantiating concrete and simulated IPayload stations.

#include "IPayload.h"
#include "Onvif/OnvifClient.h"
#include "PelcoDCore/PelcoDDevice.h"
#include "PelcoDFujinon/FujinonSX800Device.h"

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
    /// @param[in] uri Unified connection URI.
    /// @return Shared pointer to configured IPayload station, or nullptr if URI is unsupported.
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
};

} // namespace PayloadHal
