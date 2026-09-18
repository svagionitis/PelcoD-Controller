#pragma once

/// @file SpectrogramWidget.h
/// @brief Interactive 2D Time-Frequency Spectrogram Waterfall widget with instantaneous slice plot.

#include "SpectrogramColorMap.h"
#include "Stft.h"

#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QMutex>
#include <QPaintEvent>
#include <QWidget>
#include <vector>

namespace PelcoDApp {

/// @class SpectrogramWidget
/// @brief Renders real-time 2D time-frequency spectrogram waterfall and power spectrum slice.
class SpectrogramWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param[in] parent Optional parent widget.
    explicit SpectrogramWidget(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~SpectrogramWidget() override = default;

    /// @brief Updates the active colormap preset.
    void setColorPreset(PelcoD::SpectrogramColorMap::Preset preset);

    /// @brief Retrieves the active colormap preset.
    [[nodiscard]] PelcoD::SpectrogramColorMap::Preset colorPreset() const noexcept;

    /// @brief Updates the display with the latest STFT frame history.
    /// @param[in] frames Chronological list of spectrogram frames.
    /// @param[in] freqs Center frequencies for each bin in Hertz.
    void updateSpectrogram(const std::vector<PelcoD::SpectrogramFrame>& frames, const std::vector<double>& freqs);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void renderWaterfall(QPainter& painter, const QRect& rect);
    void renderSlice(QPainter& painter, const QRect& rect);
    void renderBadges(QPainter& painter, const QRect& rect);

    mutable QMutex m_mutex;
    PelcoD::SpectrogramColorMap::Preset m_preset { PelcoD::SpectrogramColorMap::Preset::Inferno };

    std::vector<PelcoD::SpectrogramFrame> m_frames {};
    std::vector<double> m_freqs {};

    QImage m_waterfallImage {};
    double m_minDb { -60.0 };
    double m_maxDb { 0.0 };

    double m_latestPeakHz { 0.0 };
    double m_latestCentroidHz { 0.0 };
    double m_latestFlatness { 0.0 };
    double m_latestPowerRatio { 0.0 };
};

} // namespace PelcoDApp
