#include "SonyFCBDevice.h"
#include <ViscaBuilder.h>
#include <ViscaParser.h>

namespace Visca::Sony {

SonyFCBDevice::SonyFCBDevice(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress)
    : m_device(std::move(transport), cameraAddress)
{
    m_capabilities = SonyCameraModel::getCapabilities(SonyCameraModelType::Unknown);
}

bool SonyFCBDevice::initialize()
{
    static_cast<void>(m_device.ifClear());

    const ViscaFrame verInq = ViscaBuilder::versionInquiry(m_device.cameraAddress());
    const InquiryResult res = m_device.sendInquirySync(verInq);

    if (res.success) {
        const auto verInfo = ViscaParser::parseVersionInquiry(res.responseFrame);
        if (verInfo.has_value()) {
            m_modelType = SonyCameraModel::identify(verInfo->vendorId, verInfo->modelId);
            m_capabilities = SonyCameraModel::getCapabilities(m_modelType);
            return true;
        }
    }

    m_modelType = SonyCameraModelType::GenericSony;
    m_capabilities = SonyCameraModel::getCapabilities(m_modelType);
    return false;
}

SonyCameraModelType SonyFCBDevice::modelType() const noexcept
{
    return m_modelType;
}

const CameraCapabilities& SonyFCBDevice::capabilities() const noexcept
{
    return m_capabilities;
}

SonyFCBStatus SonyFCBDevice::status() const noexcept
{
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

bool SonyFCBDevice::pollStatus()
{
    bool allSuccess = true;
    SonyFCBStatus tempStatus = status();

    for (uint8_t block = 0; block <= 4; ++block) {
        const ViscaFrame inq = SonyViscaBuilder::blockInquiry(m_device.cameraAddress(), block);
        const InquiryResult res = m_device.sendInquirySync(inq);
        if (res.success) {
            SonyViscaParser::parseBlock(block, res.responseFrame, tempStatus);
        } else {
            allSuccess = false;
        }
    }

    {
        std::scoped_lock lock(m_statusMutex);
        m_status = tempStatus;
    }
    return allSuccess;
}

bool SonyFCBDevice::setZoomDirect(uint16_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::zoomDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::zoomTele(uint8_t speed)
{
    const ViscaFrame cmd = SonyViscaBuilder::zoomTeleVariable(m_device.cameraAddress(), speed);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::zoomWide(uint8_t speed)
{
    const ViscaFrame cmd = SonyViscaBuilder::zoomWideVariable(m_device.cameraAddress(), speed);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::zoomStop()
{
    const ViscaFrame cmd = SonyViscaBuilder::zoomStop(m_device.cameraAddress());
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setFocusAuto(bool autoMode)
{
    const ViscaFrame cmd = SonyViscaBuilder::focusAuto(m_device.cameraAddress(), autoMode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setFocusDirect(uint16_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::focusDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::focusFar(uint8_t speed)
{
    const ViscaFrame cmd = SonyViscaBuilder::focusFarVariable(m_device.cameraAddress(), speed);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::focusNear(uint8_t speed)
{
    const ViscaFrame cmd = SonyViscaBuilder::focusNearVariable(m_device.cameraAddress(), speed);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::focusStop()
{
    const ViscaFrame cmd = SonyViscaBuilder::focusStop(m_device.cameraAddress());
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::focusOnePush()
{
    const ViscaFrame cmd = SonyViscaBuilder::focusOnePush(m_device.cameraAddress());
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setFocusNearLimit(uint16_t limit)
{
    const ViscaFrame cmd = SonyViscaBuilder::focusNearLimit(m_device.cameraAddress(), limit);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setExposureMode(SonyExposureMode mode)
{
    const ViscaFrame cmd = SonyViscaBuilder::exposureMode(m_device.cameraAddress(), mode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setShutter(uint8_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::shutterDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setIris(uint8_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::irisDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setGain(uint8_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::gainDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setExposureCompensation(bool on, uint8_t position)
{
    const ViscaFrame onCmd = SonyViscaBuilder::exposureComp(m_device.cameraAddress(), on);
    if (!m_device.sendCommandSync(onCmd).success) {
        return false;
    }
    if (on) {
        const ViscaFrame posCmd = SonyViscaBuilder::exposureCompDirect(m_device.cameraAddress(), position);
        return m_device.sendCommandSync(posCmd).success;
    }
    return true;
}

bool SonyFCBDevice::setWhiteBalance(SonyWhiteBalanceMode mode)
{
    const ViscaFrame cmd = SonyViscaBuilder::wbMode(m_device.cameraAddress(), mode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setRgaiDirect(uint8_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::rGainDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setBgainDirect(uint8_t position)
{
    const ViscaFrame cmd = SonyViscaBuilder::bGainDirect(m_device.cameraAddress(), position);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::triggerOnePushWb()
{
    const ViscaFrame cmd = SonyViscaBuilder::wbOnePushTrigger(m_device.cameraAddress());
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setStabilizer(SonyStabilizerMode mode)
{
    const ViscaFrame cmd = SonyViscaBuilder::stabilizer(m_device.cameraAddress(), mode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setDefog(SonyDefogMode mode)
{
    const ViscaFrame cmd = SonyViscaBuilder::defog(m_device.cameraAddress(), mode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setIcr(bool on)
{
    const ViscaFrame cmd = SonyViscaBuilder::icr(m_device.cameraAddress(), on);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setAutoIcr(bool on)
{
    const ViscaFrame cmd = SonyViscaBuilder::autoIcr(m_device.cameraAddress(), on);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setDistortionCompensation(bool on)
{
    if (!m_capabilities.supportsDistortionCompensation) {
        return false;
    }
    const uint8_t val = on ? 0x01 : 0x00;
    const ViscaFrame cmd = SonyViscaBuilder::writeRegister(m_device.cameraAddress(), Register::kDistortionComp, val);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setOpticalAxisGapCompensation(bool on)
{
    if (!m_capabilities.supportsOpticalAxisGapCompensation) {
        return false;
    }
    const uint8_t val = on ? 0x01 : 0x00;
    const ViscaFrame cmd = SonyViscaBuilder::writeRegister(m_device.cameraAddress(), Register::kOpticalAxisGap, val);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setOperatingMode(uint8_t mode)
{
    if (mode >= 0x25 && !m_capabilities.supports4K) {
        // Mode requires 4K UHD capability which this camera does not support
        return false;
    }
    const ViscaFrame cmd = SonyViscaBuilder::writeRegister(m_device.cameraAddress(), Register::kOperatingMode, mode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setLvdsMode(uint8_t mode)
{
    if (!m_capabilities.supportsLvdsMode) {
        return false;
    }
    const ViscaFrame cmd = SonyViscaBuilder::writeRegister(m_device.cameraAddress(), Register::kLvdsMode, mode);
    return m_device.sendCommandSync(cmd).success;
}

bool SonyFCBDevice::setDigitalOutputMode(uint8_t mode)
{
    if (!m_capabilities.supportsTmdsOutput) {
        return false;
    }
    const ViscaFrame cmd = SonyViscaBuilder::writeRegister(m_device.cameraAddress(), Register::kDigitalOutput, mode);
    return m_device.sendCommandSync(cmd).success;
}

} // namespace Visca::Sony
