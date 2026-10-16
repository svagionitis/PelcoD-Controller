/// @file QPelcoDFujinonDevice.cpp
/// @brief Implementation of QPelcoDFujinonDevice Qt adapter.

#include "QPelcoDFujinonDevice.h"

#include <QMetaObject>
#include <utility>

namespace PelcoDQt {

QPelcoDFujinonDevice::QPelcoDFujinonDevice(
    std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address, QObject* parent)
    : QPelcoDDevice(nullptr, address, parent)
{
    if (transport) {
        setTransport(std::move(transport), address);
    }
}

QPelcoDFujinonDevice::~QPelcoDFujinonDevice()
{
    if (m_fujinonAdapter != nullptr) {
        delete m_fujinonAdapter;
        m_fujinonAdapter = nullptr;
    }
}

std::shared_ptr<PelcoD::FujinonSX800Device> QPelcoDFujinonDevice::fujinonCoreDevice() const noexcept
{
    return m_fujinonDevice;
}

QFujinonSX800Device* QPelcoDFujinonDevice::fujinonDevice() const noexcept
{
    return m_fujinonAdapter;
}

PelcoD::FujinonStatus QPelcoDFujinonDevice::fujinonStatus() const
{
    return m_fujinonDevice ? m_fujinonDevice->getFujinonStatus() : PelcoD::FujinonStatus {};
}

void QPelcoDFujinonDevice::setAutoFocus(PelcoD::FujinonAutoFocusMode mode)
{
    if (m_fujinonDevice) {
        static_cast<void>(m_fujinonDevice->setAutoFocus(mode));
    }
}

void QPelcoDFujinonDevice::onePushAF()
{
    if (m_fujinonDevice) {
        static_cast<void>(m_fujinonDevice->onePushAF());
    }
}

std::shared_ptr<PelcoD::PelcoDDevice> QPelcoDFujinonDevice::createCoreDevice(
    std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address)
{
    if (m_fujinonAdapter != nullptr) {
        delete m_fujinonAdapter;
        m_fujinonAdapter = nullptr;
    }
    m_fujinonDevice = std::make_shared<PelcoD::FujinonSX800Device>(std::move(transport), address);
    m_fujinonAdapter = new QFujinonSX800Device(m_fujinonDevice, this);
    return m_fujinonDevice;
}

void QPelcoDFujinonDevice::onCallbacksInitialized()
{
    if (m_fujinonDevice) {
        m_deviceConnections += m_fujinonDevice->addFujinonStatusCallback([this](const PelcoD::FujinonStatus& status) {
            QMetaObject::invokeMethod(this, [this, status] { emit fujinonStatusUpdated(status); });
        });
    }
}

} // namespace PelcoDQt
