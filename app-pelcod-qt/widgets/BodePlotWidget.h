#pragma once

/// @file BodePlotWidget.h
/// @brief Interactive Qt widget for rendering empirical plant frequency response (Bode plot) and stability margins.

#include "PlantIdentifier.h"

#include <QColor>
#include <QFont>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPoint>
#include <QWidget>

namespace PelcoD {

/// @class BodePlotWidget
/// @brief 2-Panel high-resolution empirical Bode plot visualizer.
/// @details Plots Magnitude (dB), Phase (deg), Coherence, 0 dB and -180 deg stability thresholds,
///          gain crossover (f_gc), phase crossover (f_180), and resonance peak annotations.
class BodePlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit BodePlotWidget(QWidget* parent = nullptr);
    ~BodePlotWidget() override = default;

    /// @brief Update widget with complete plant identification results.
    /// @param[in] result Outcome containing Bode vectors, stability margins, and resonance peaks.
    void setIdentificationResult(const Tracking::PlantIdentificationResult& result);

    /// @brief Clear plots and reset to idle state.
    void clear();

    /// @brief Minimum recommended widget size.
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// @brief Default preferred widget size.
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawGridAndAxes(QPainter& painter, const QRect& magRect, const QRect& phaseRect);
    void drawMagnitudeResponse(QPainter& painter, const QRect& magRect);
    void drawPhaseResponse(QPainter& painter, const QRect& phaseRect);
    void drawStabilityAnnotations(QPainter& painter, const QRect& magRect, const QRect& phaseRect);
    void drawCursorCrosshair(QPainter& painter, const QRect& magRect, const QRect& phaseRect);

    [[nodiscard]] double xToFreq(int x, const QRect& plotRect) const;
    [[nodiscard]] int freqToX(double f, const QRect& plotRect) const;

    Tracking::PlantIdentificationResult m_result {};
    bool m_hasData { false };

    // Mouse hover inspection cursor
    bool m_cursorActive { false };
    QPoint m_cursorPos {};

    // Dynamic frequency and amplitude display ranges
    double m_minFreqHz { 0.1 };
    double m_maxFreqHz { 15.0 };
    double m_minMagDb { -40.0 };
    double m_maxMagDb { 20.0 };
    double m_minPhaseDeg { -270.0 };
    double m_maxPhaseDeg { 45.0 };
};

} // namespace PelcoD
