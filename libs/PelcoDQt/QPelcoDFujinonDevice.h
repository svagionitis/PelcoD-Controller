#pragma once

/// @file QPelcoDFujinonDevice.h
/// @brief Qt 6 adapter wrapping FujinonSX800Device for Fujinon optical camera control.

#include "FujinonSX800Device.h"
#include "FujinonTypes.h"
#include "QFujinonSX800Device.h"
#include "QPelcoDDevice.h"

#include <memory>

namespace PelcoDQt {

/// @class QPelcoDFujinonDevice
/// @brief Specialization of QPelcoDDevice providing Fujinon SX800 profile controls and telemetry.
class QPelcoDFujinonDevice : public QPelcoDDevice {
    Q_OBJECT

public:
    /// @brief Construct a QPelcoDFujinonDevice with optional transport and address.
    /// @param[in] transport Underlying communication transport interface.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] parent Optional QObject parent.
    explicit QPelcoDFujinonDevice(
        std::shared_ptr<PelcoD::ITransport> transport = nullptr, std::uint8_t address = 1U, QObject* parent = nullptr);

    /// @brief Destructor.
    ~QPelcoDFujinonDevice() override;

    /// @brief Retrieve shared pointer to underlying core FujinonSX800Device.
    /// @return Shared pointer to PelcoD::FujinonSX800Device, or nullptr if unset.
    [[nodiscard]] std::shared_ptr<PelcoD::FujinonSX800Device> fujinonCoreDevice() const noexcept;

    /// @brief Retrieve pointer to underlying Fujinon SX800 Qt adapter profile.
    /// @return Pointer to QFujinonSX800Device, or nullptr if uninitialized.
    [[nodiscard]] QFujinonSX800Device* fujinonDevice() const noexcept;

    /// @brief Retrieve latest snapshot of Fujinon optical and sensor telemetry.
    /// @return Current FujinonStatus struct.
    [[nodiscard]] PelcoD::FujinonStatus fujinonStatus() const;

    /// @brief Configure autofocus operational mode on the underlying Fujinon camera.
    /// @param[in] mode AF on, AF off (manual), or Quick AF.
    void setAutoFocus(PelcoD::FujinonAutoFocusMode mode);

    /// @brief Trigger One-Push Auto Focus execution.
    void onePushAF();

signals:
    /// @brief Emitted when an extended Fujinon telemetry packet is received.
    /// @param[in] status Updated Fujinon telemetry snapshot.
    void fujinonStatusUpdated(const PelcoD::FujinonStatus& status);

protected:
    /// @brief Factory method creating the specialized FujinonSX800Device core instance.
    /// @param[in] transport Transport interface.
    /// @param[in] address Device bus address.
    /// @return Shared pointer to newly created PelcoD::PelcoDDevice base reference.
    [[nodiscard]] std::shared_ptr<PelcoD::PelcoDDevice> createCoreDevice(
        std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address) override;

    /// @brief Subclass hook to register Fujinon telemetry callbacks upon initialization.
    void onCallbacksInitialized() override;

private:
    std::shared_ptr<PelcoD::FujinonSX800Device> m_fujinonDevice {};
    QFujinonSX800Device* m_fujinonAdapter { nullptr };
};

} // namespace PelcoDQt
