/// @file LensWorker.cpp
#include "LensWorker.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <utility>

#include <QObject>
#include <QTimer>

#include "LensController.h"
#include "LensTypes.h"
#include "MockLensDriver.h"

namespace LensApp {

LensWorker::LensWorker(QObject* parent)
    : QObject(parent)
{
}

LensWorker::~LensWorker()
{
    stopWorker();
}

bool LensWorker::isRunning() const noexcept
{
    return m_isRunning;
}

int LensWorker::activeChannel() const noexcept
{
    return m_activeChannel;
}

void LensWorker::initWorker()
{
    if (m_isRunning) {
        return;
    }

    for (std::uint8_t i = 0U; i < 4U; ++i) {
        auto mockDriver = std::make_unique<LensControl::MockLensDriver>();
        m_controller.setDriver(LensControl::CameraIndex { i }, std::move(mockDriver));
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LensWorker::processLoopTick);
    m_timer->start(33);

    m_isRunning = true;
    emit workerInitialized();
}

void LensWorker::stopWorker()
{
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    m_isRunning = false;
}

void LensWorker::onSetActiveChannel(int channel)
{
    const int clamped = std::clamp(channel, 0, 3);
    m_activeChannel = clamped;
}

void LensWorker::onSetLensMode(int channel, int mode)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    const auto lMode = static_cast<LensControl::LensMode>(std::clamp(mode, 0, 5));
    m_controller.setLensMode(cam, lMode);
}

void LensWorker::onMoveFocusStep(int channel, int steps)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    m_controller.moveFocusStep(cam, static_cast<std::int16_t>(steps));
}

void LensWorker::onMoveZoomStep(int channel, int steps)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    m_controller.moveZoomStep(cam, static_cast<std::int16_t>(steps));
}

void LensWorker::onSetFocusTarget(int channel, int position)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    const auto clamped = static_cast<std::uint16_t>(std::clamp(position, 0, 65535));
    m_controller.setFocusTarget(cam, LensControl::MotorPosition { clamped });
}

void LensWorker::onSetZoomTarget(int channel, int position)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    const auto clamped = static_cast<std::uint16_t>(std::clamp(position, 0, 65535));
    m_controller.setZoomTarget(cam, LensControl::MotorPosition { clamped });
}

void LensWorker::onSetFocusSpeed(int channel, int speed)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    const auto clamped = static_cast<std::uint8_t>(std::clamp(speed, 0, 255));
    m_controller.setFocusSpeed(cam, LensControl::MotorSpeed { clamped });
}

void LensWorker::onSetZoomSpeed(int channel, int speed)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    const auto clamped = static_cast<std::uint8_t>(std::clamp(speed, 0, 255));
    m_controller.setZoomSpeed(cam, LensControl::MotorSpeed { clamped });
}

void LensWorker::onStopMotion(int channel)
{
    if (channel < 0 || channel > 3) {
        return;
    }
    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(channel) };
    m_controller.stopMotion(cam);
}

void LensWorker::onEmergencyStop()
{
    for (std::uint8_t i = 0U; i < 4U; ++i) {
        m_controller.stopMotion(LensControl::CameraIndex { i });
    }
}

void LensWorker::processLoopTick()
{
    for (std::uint8_t i = 0U; i < 4U; ++i) {
        m_controller.update(LensControl::CameraIndex { i });
    }

    const auto cam = LensControl::CameraIndex { static_cast<std::uint8_t>(m_activeChannel) };
    const auto status = m_controller.getLensStatus(cam);

    const int focus = static_cast<int>(status.focusPos.value());
    const int zoom = static_cast<int>(status.zoomPos.value());
    const int limits = static_cast<int>(status.focusLimits | status.zoomLimits);
    const int mode = static_cast<int>(status.mode);
    const bool afActive = status.isAfActive;

    emit statusUpdated(focus, zoom, limits, mode, afActive);
}

} // namespace LensApp
