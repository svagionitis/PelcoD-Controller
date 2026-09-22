#pragma once

#include "SonyCameraModel.h"
#include "SonyViscaTypes.h"
#include <Transport/ITransport.h>
#include <ViscaFrame.h>
#include <ViscaRxAccumulator.h>
#include <ViscaTypes.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Visca::Sony {

/// @class MockSonyCamera
/// @brief Simulated Sony FCB camera implementing Transport::ITransport.
/// @details Accurately emulates 2-socket state machine pacing, ACK/Completion timing,
/// register banks (0x00 to 0x7F), and Block Inquiries (00 to 05) for FCB-EV9520L and FCB-EW9500H.
class MockSonyCamera : public ::Transport::ITransport {
public:
    /// @brief Constructs a simulated camera.
    /// @param[in] model Camera model to emulate (EV9520L or EW9500H).
    /// @param[in] address Bus address of camera (1..7).
    explicit MockSonyCamera(SonyCameraModelType model = SonyCameraModelType::FCB_EV9520L, uint8_t address = 1);

    ~MockSonyCamera() override = default;

    // --- ITransport Interface Implementation ---
    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    bool sendData(const std::vector<uint8_t>& data) override;
    void setDataCallback(DataReceivedCallback callback) override;
    void setStateCallback(StateChangedCallback callback) override;
    bool setBaudRate(uint32_t baudRate) override;
    [[nodiscard]] uint32_t getBaudRate() const noexcept override;

    // --- Simulator Configuration & State Inspection ---
    void setModelType(SonyCameraModelType model);
    [[nodiscard]] SonyCameraModelType modelType() const noexcept;

    void setCameraAddress(uint8_t address) noexcept;
    [[nodiscard]] uint8_t cameraAddress() const noexcept;

    [[nodiscard]] uint16_t zoomPosition() const noexcept;
    void setZoomPosition(uint16_t pos) noexcept;

    [[nodiscard]] uint16_t focusPosition() const noexcept;
    void setFocusPosition(uint16_t pos) noexcept;

    [[nodiscard]] uint8_t registerValue(uint8_t reg) const noexcept;
    void setRegisterValue(uint8_t reg, uint8_t val) noexcept;

    [[nodiscard]] SonyExposureMode exposureMode() const noexcept;
    [[nodiscard]] SonyWhiteBalanceMode wbMode() const noexcept;
    [[nodiscard]] SonyStabilizerMode stabilizerMode() const noexcept;
    [[nodiscard]] SonyDefogMode defogMode() const noexcept;

    /// @brief Injects a specific error code for the next received command.
    void injectNextError(ViscaErrorCode code) noexcept;

private:
    void processIncomingFrame(const ViscaFrame& frame);
    void handleCommand(const ViscaFrame& frame);
    void handleInquiry(const ViscaFrame& frame);
    void sendResponse(const ViscaFrame& frame);

    ViscaFrame buildBlock00Response() const;
    ViscaFrame buildBlock01Response() const;
    ViscaFrame buildBlock02Response() const;
    ViscaFrame buildBlock03Response() const;
    ViscaFrame buildBlock04Response() const;

    mutable std::mutex m_mutex {};
    std::atomic<bool> m_open { true };
    uint32_t m_baudRate { 9600 };
    DataReceivedCallback m_dataCallback { nullptr };
    StateChangedCallback m_stateCallback { nullptr };

    ViscaRxAccumulator m_accumulator {};

    SonyCameraModelType m_modelType { SonyCameraModelType::FCB_EV9520L };
    uint8_t m_address { 1 };

    // Socket simulation
    bool m_socket1Busy { false };
    bool m_socket2Busy { false };

    // Camera state
    uint16_t m_zoomPosition { 0x0000 };
    uint16_t m_focusPosition { 0x1000 };
    uint16_t m_focusNearLimit { 0x1000 };
    bool m_focusAuto { true };
    bool m_dzoomOn { false };
    bool m_dzoomCombine { true };

    SonyExposureMode m_exposureMode { SonyExposureMode::FullAuto };
    uint8_t m_shutterPosition { 0x10 };
    uint8_t m_irisPosition { 0x0C };
    uint8_t m_gainPosition { 0x04 };
    uint8_t m_exposureCompPosition { 0x07 };
    bool m_exposureCompOn { false };

    SonyWhiteBalanceMode m_wbMode { SonyWhiteBalanceMode::Auto };
    uint16_t m_rGain { 0x0180 };
    uint16_t m_bGain { 0x0180 };

    SonyStabilizerMode m_stabilizerMode { SonyStabilizerMode::Normal };
    bool m_stabilizerOn { false };
    SonyDefogMode m_defogMode { SonyDefogMode::Off };
    bool m_icrOn { false };

    std::array<uint8_t, 128> m_registers {};
    ViscaErrorCode m_injectedError { ViscaErrorCode::None };
};

} // namespace Visca::Sony
