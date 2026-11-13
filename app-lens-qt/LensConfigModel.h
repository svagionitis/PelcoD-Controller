#pragma once

/// @file LensConfigModel.h
/// @brief QObject ViewModel exposing lens configuration and tuning parameters to QML.

#include <cstdint>

#include <QObject>
#include <QString>

namespace LensApp {

/// @class LensConfigModel
/// @brief Configurable properties for lens controller axes, speeds, and presets.
class LensConfigModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int defaultZoomSpeed READ defaultZoomSpeed WRITE setDefaultZoomSpeed NOTIFY configChanged)
    Q_PROPERTY(int defaultFocusSpeed READ defaultFocusSpeed WRITE setDefaultFocusSpeed NOTIFY configChanged)
    Q_PROPERTY(int microStepSize READ microStepSize WRITE setMicroStepSize NOTIFY configChanged)
    Q_PROPERTY(QString channel0Name READ channel0Name WRITE setChannel0Name NOTIFY configChanged)
    Q_PROPERTY(QString channel1Name READ channel1Name WRITE setChannel1Name NOTIFY configChanged)
    Q_PROPERTY(QString channel2Name READ channel2Name WRITE setChannel2Name NOTIFY configChanged)
    Q_PROPERTY(QString channel3Name READ channel3Name WRITE setChannel3Name NOTIFY configChanged)
    Q_PROPERTY(int cafThreshold READ cafThreshold WRITE setCafThreshold NOTIFY configChanged)
    Q_PROPERTY(int cafReArmFrames READ cafReArmFrames WRITE setCafReArmFrames NOTIFY configChanged)

public:
    explicit LensConfigModel(QObject* parent = nullptr);
    ~LensConfigModel() override = default;

    [[nodiscard]] int defaultZoomSpeed() const noexcept;
    [[nodiscard]] int defaultFocusSpeed() const noexcept;
    [[nodiscard]] int microStepSize() const noexcept;
    [[nodiscard]] QString channel0Name() const;
    [[nodiscard]] QString channel1Name() const;
    [[nodiscard]] QString channel2Name() const;
    [[nodiscard]] QString channel3Name() const;
    [[nodiscard]] int cafThreshold() const noexcept;
    [[nodiscard]] int cafReArmFrames() const noexcept;

public slots:
    void setDefaultZoomSpeed(int speed);
    void setDefaultFocusSpeed(int speed);
    void setMicroStepSize(int steps);
    void setChannel0Name(const QString& name);
    void setChannel1Name(const QString& name);
    void setChannel2Name(const QString& name);
    void setChannel3Name(const QString& name);
    void setCafThreshold(int thresh);
    void setCafReArmFrames(int frames);
    void resetDefaults();

signals:
    void configChanged();

private:
    int m_defaultZoomSpeed { 127 };
    int m_defaultFocusSpeed { 127 };
    int m_microStepSize { 50 };
    QString m_channel0Name { QStringLiteral("Ch 0: EO Wide") };
    QString m_channel1Name { QStringLiteral("Ch 1: Spotter Tele") };
    QString m_channel2Name { QStringLiteral("Ch 2: Thermal LWIR") };
    QString m_channel3Name { QStringLiteral("Ch 3: SWIR Optical") };
    int m_cafThreshold { 15 };
    int m_cafReArmFrames { 10 };
};

} // namespace LensApp
