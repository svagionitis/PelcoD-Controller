/// @file LensConfigModel.cpp
#include "LensConfigModel.h"

#include <cstdint>

#include <algorithm>

#include <QString>

namespace LensApp {

LensConfigModel::LensConfigModel(QObject* parent)
    : QObject(parent)
{
}

int LensConfigModel::defaultZoomSpeed() const noexcept
{
    return m_defaultZoomSpeed;
}

int LensConfigModel::defaultFocusSpeed() const noexcept
{
    return m_defaultFocusSpeed;
}

int LensConfigModel::microStepSize() const noexcept
{
    return m_microStepSize;
}

QString LensConfigModel::channel0Name() const
{
    return m_channel0Name;
}

QString LensConfigModel::channel1Name() const
{
    return m_channel1Name;
}

QString LensConfigModel::channel2Name() const
{
    return m_channel2Name;
}

QString LensConfigModel::channel3Name() const
{
    return m_channel3Name;
}

int LensConfigModel::cafThreshold() const noexcept
{
    return m_cafThreshold;
}

int LensConfigModel::cafReArmFrames() const noexcept
{
    return m_cafReArmFrames;
}

void LensConfigModel::setDefaultZoomSpeed(int speed)
{
    const int clamped = std::clamp(speed, 0, 255);
    if (m_defaultZoomSpeed != clamped) {
        m_defaultZoomSpeed = clamped;
        emit configChanged();
    }
}

void LensConfigModel::setDefaultFocusSpeed(int speed)
{
    const int clamped = std::clamp(speed, 0, 255);
    if (m_defaultFocusSpeed != clamped) {
        m_defaultFocusSpeed = clamped;
        emit configChanged();
    }
}

void LensConfigModel::setMicroStepSize(int steps)
{
    const int clamped = std::clamp(steps, 1, 10000);
    if (m_microStepSize != clamped) {
        m_microStepSize = clamped;
        emit configChanged();
    }
}

void LensConfigModel::setChannel0Name(const QString& name)
{
    if (m_channel0Name != name && !name.trimmed().isEmpty()) {
        m_channel0Name = name;
        emit configChanged();
    }
}

void LensConfigModel::setChannel1Name(const QString& name)
{
    if (m_channel1Name != name && !name.trimmed().isEmpty()) {
        m_channel1Name = name;
        emit configChanged();
    }
}

void LensConfigModel::setChannel2Name(const QString& name)
{
    if (m_channel2Name != name && !name.trimmed().isEmpty()) {
        m_channel2Name = name;
        emit configChanged();
    }
}

void LensConfigModel::setChannel3Name(const QString& name)
{
    if (m_channel3Name != name && !name.trimmed().isEmpty()) {
        m_channel3Name = name;
        emit configChanged();
    }
}

void LensConfigModel::setCafThreshold(int thresh)
{
    const int clamped = std::clamp(thresh, 1, 100);
    if (m_cafThreshold != clamped) {
        m_cafThreshold = clamped;
        emit configChanged();
    }
}

void LensConfigModel::setCafReArmFrames(int frames)
{
    const int clamped = std::clamp(frames, 1, 120);
    if (m_cafReArmFrames != clamped) {
        m_cafReArmFrames = clamped;
        emit configChanged();
    }
}

void LensConfigModel::resetDefaults()
{
    m_defaultZoomSpeed = 127;
    m_defaultFocusSpeed = 127;
    m_microStepSize = 50;
    m_channel0Name = QStringLiteral("Ch 0: EO Wide");
    m_channel1Name = QStringLiteral("Ch 1: Spotter Tele");
    m_channel2Name = QStringLiteral("Ch 2: Thermal LWIR");
    m_channel3Name = QStringLiteral("Ch 3: SWIR Optical");
    m_cafThreshold = 15;
    m_cafReArmFrames = 10;
    emit configChanged();
}

} // namespace LensApp
