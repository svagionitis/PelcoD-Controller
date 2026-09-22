/// @file BodePlotWidget.cpp
/// @brief Implementation of high-resolution empirical Bode plot widget.

#include "BodePlotWidget.h"

#include <QBrush>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace PelcoD {

BodePlotWidget::BodePlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void BodePlotWidget::setIdentificationResult(const Tracking::PlantIdentificationResult& result)
{
    m_result = result;
    m_hasData = result.success && !result.bode.frequenciesHz.empty();

    if (m_hasData) {
        const auto& freqs = result.bode.frequenciesHz;
        const auto& mags = result.bode.magnitudeDb;
        const auto& phases = result.bode.phaseDeg;

        m_minFreqHz = std::max(0.01, freqs.front());
        m_maxFreqHz = std::max(m_minFreqHz + 1.0, freqs.back());

        // Autoscale magnitude axis with comfortable margin
        double minM = -30.0;
        double maxM = 10.0;
        for (double m : mags) {
            if (!std::isnan(m) && !std::isinf(m)) {
                minM = std::min(minM, m);
                maxM = std::max(maxM, m);
            }
        }
        m_minMagDb = std::floor((minM - 5.0) / 10.0) * 10.0;
        m_maxMagDb = std::ceil((maxM + 5.0) / 10.0) * 10.0;

        // Autoscale phase axis
        double minP = -200.0;
        double maxP = 20.0;
        for (double p : phases) {
            if (!std::isnan(p) && !std::isinf(p)) {
                minP = std::min(minP, p);
                maxP = std::max(maxP, p);
            }
        }
        m_minPhaseDeg = std::floor((minP - 15.0) / 45.0) * 45.0;
        m_maxPhaseDeg = std::ceil((maxP + 15.0) / 45.0) * 45.0;
    }

    update();
}

void BodePlotWidget::clear()
{
    m_result = Tracking::PlantIdentificationResult {};
    m_hasData = false;
    m_cursorActive = false;
    update();
}

QSize BodePlotWidget::minimumSizeHint() const
{
    return QSize(320, 240);
}

QSize BodePlotWidget::sizeHint() const
{
    return QSize(560, 380);
}

void BodePlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_cursorPos = event->pos();
    m_cursorActive = true;
    update();
}

void BodePlotWidget::leaveEvent(QEvent* /*event*/)
{
    m_cursorActive = false;
    update();
}

double BodePlotWidget::xToFreq(int x, const QRect& plotRect) const
{
    if (plotRect.width() <= 0) {
        return m_minFreqHz;
    }
    const double ratio
        = std::clamp(static_cast<double>(x - plotRect.left()) / static_cast<double>(plotRect.width()), 0.0, 1.0);
    return m_minFreqHz + ratio * (m_maxFreqHz - m_minFreqHz);
}

int BodePlotWidget::freqToX(double f, const QRect& plotRect) const
{
    if (m_maxFreqHz <= m_minFreqHz) {
        return plotRect.left();
    }
    const double ratio = (f - m_minFreqHz) / (m_maxFreqHz - m_minFreqHz);
    return plotRect.left() + static_cast<int>(std::round(ratio * plotRect.width()));
}

void BodePlotWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect client = rect();
    painter.fillRect(client, QColor(13, 17, 23)); // Dark tactical background

    if (!m_hasData) {
        painter.setPen(QColor(139, 148, 158));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        painter.drawText(client, Qt::AlignCenter,
            tr("No Plant Identification Data\nClick 'Start Plant Sweep' to measure empirical Bode response"));
        return;
    }

    // Partition layout into Magnitude (upper) and Phase (lower) panels
    const int marginL = 50;
    const int marginR = 20;
    const int marginT = 24;
    const int marginB = 24;
    const int gap = 16;

    const int totalPlotH = client.height() - marginT - marginB - gap;
    const int subH = totalPlotH / 2;
    const int plotW = client.width() - marginL - marginR;

    const QRect magRect(marginL, marginT, plotW, subH);
    const QRect phaseRect(marginL, marginT + subH + gap, plotW, subH);

    drawGridAndAxes(painter, magRect, phaseRect);
    drawMagnitudeResponse(painter, magRect);
    drawPhaseResponse(painter, phaseRect);
    drawStabilityAnnotations(painter, magRect, phaseRect);

    if (m_cursorActive) {
        drawCursorCrosshair(painter, magRect, phaseRect);
    }
}

void BodePlotWidget::drawGridAndAxes(QPainter& painter, const QRect& magRect, const QRect& phaseRect)
{
    painter.setRenderHint(QPainter::Antialiasing, false);
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    // 1. Grid & labels for Magnitude panel (dB)
    const QPen gridPen(QColor(33, 38, 45), 1, Qt::DotLine);
    const QPen zeroPen(QColor(88, 166, 255, 120), 1, Qt::DashLine);
    const QPen textPen(QColor(139, 148, 158));

    painter.setPen(QColor(48, 54, 61));
    painter.drawRect(magRect);
    painter.drawRect(phaseRect);

    // Magnitude Y-axis ticks
    const double magStep = 10.0;
    for (double m = m_minMagDb; m <= m_maxMagDb; m += magStep) {
        const double ratio = (m - m_minMagDb) / (m_maxMagDb - m_minMagDb);
        const int y = magRect.bottom() - static_cast<int>(std::round(ratio * magRect.height()));

        if (std::abs(m) < 0.1) {
            painter.setPen(zeroPen);
            painter.drawLine(magRect.left(), y, magRect.right(), y);
        } else {
            painter.setPen(gridPen);
            painter.drawLine(magRect.left(), y, magRect.right(), y);
        }

        painter.setPen(textPen);
        painter.drawText(QRect(0, y - 8, magRect.left() - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
            QString::asprintf("%+0.0f dB", m));
    }

    // Phase Y-axis ticks
    const double phaseStep = 45.0;
    const QPen critPen(QColor(255, 82, 82, 140), 1, Qt::DashLine);
    for (double p = m_minPhaseDeg; p <= m_maxPhaseDeg; p += phaseStep) {
        const double ratio = (p - m_minPhaseDeg) / (m_maxPhaseDeg - m_minPhaseDeg);
        const int y = phaseRect.bottom() - static_cast<int>(std::round(ratio * phaseRect.height()));

        if (std::abs(p - (-180.0)) < 0.1) {
            painter.setPen(critPen); // Red line at -180 critical phase crossover
            painter.drawLine(phaseRect.left(), y, phaseRect.right(), y);
        } else {
            painter.setPen(gridPen);
            painter.drawLine(phaseRect.left(), y, phaseRect.right(), y);
        }

        painter.setPen(textPen);
        painter.drawText(QRect(0, y - 8, phaseRect.left() - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
            QString::asprintf("%0.0f°", p));
    }

    // Common Frequency X-axis ticks (at bottom of Phase panel)
    const double fSpan = m_maxFreqHz - m_minFreqHz;
    const double fStep = (fSpan > 8.0) ? 2.0 : 1.0;
    for (double f = std::ceil(m_minFreqHz); f <= m_maxFreqHz; f += fStep) {
        const int x = freqToX(f, phaseRect);
        painter.setPen(gridPen);
        painter.drawLine(x, magRect.top(), x, magRect.bottom());
        painter.drawLine(x, phaseRect.top(), x, phaseRect.bottom());

        painter.setPen(textPen);
        painter.drawText(
            QRect(x - 20, phaseRect.bottom() + 4, 40, 16), Qt::AlignCenter, QString::asprintf("%0.0f Hz", f));
    }

    // Panel titles
    painter.setPen(QColor(88, 166, 255));
    painter.drawText(QRect(magRect.left() + 6, magRect.top() + 4, 200, 16), Qt::AlignLeft, tr("Magnitude |H(f)| [dB]"));

    painter.setPen(QColor(255, 166, 87));
    painter.drawText(QRect(phaseRect.left() + 6, phaseRect.top() + 4, 200, 16), Qt::AlignLeft, tr("Phase ∠H(f) [deg]"));
}

void BodePlotWidget::drawMagnitudeResponse(QPainter& painter, const QRect& magRect)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto& freqs = m_result.bode.frequenciesHz;
    const auto& mags = m_result.bode.magnitudeDb;
    const auto& coh = m_result.bode.coherence;

    if (freqs.size() < 2U) {
        return;
    }

    // 1. Draw Coherence trace (purple dashed line, scaled 0 to 1 inside magnitude plot)
    QPainterPath cohPath;
    bool firstCoh = true;
    for (std::size_t i = 0U; i < freqs.size(); ++i) {
        const double f = freqs[i];
        if (f < m_minFreqHz || f > m_maxFreqHz) {
            continue;
        }
        const int x = freqToX(f, magRect);
        const double c = std::clamp(coh[i], 0.0, 1.0);
        const int y = magRect.bottom() - static_cast<int>(std::round(c * magRect.height()));
        if (firstCoh) {
            cohPath.moveTo(x, y);
            firstCoh = false;
        } else {
            cohPath.lineTo(x, y);
        }
    }
    painter.setPen(QPen(QColor(213, 0, 249, 100), 1, Qt::DashLine));
    painter.drawPath(cohPath);

    // 2. Draw Magnitude response trace (bright cyan / teal)
    QPainterPath magPath;
    bool firstMag = true;
    for (std::size_t i = 0U; i < freqs.size(); ++i) {
        const double f = freqs[i];
        if (f < m_minFreqHz || f > m_maxFreqHz) {
            continue;
        }
        const int x = freqToX(f, magRect);
        const double ratio = (mags[i] - m_minMagDb) / (m_maxMagDb - m_minMagDb);
        const int y = magRect.bottom() - static_cast<int>(std::round(ratio * magRect.height()));

        if (firstMag) {
            magPath.moveTo(x, y);
            firstMag = false;
        } else {
            magPath.lineTo(x, y);
        }
    }
    painter.setPen(QPen(QColor(0, 229, 255), 2));
    painter.drawPath(magPath);
}

void BodePlotWidget::drawPhaseResponse(QPainter& painter, const QRect& phaseRect)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto& freqs = m_result.bode.frequenciesHz;
    const auto& phases = m_result.bode.phaseDeg;

    if (freqs.size() < 2U) {
        return;
    }

    QPainterPath phasePath;
    bool firstPhase = true;
    for (std::size_t i = 0U; i < freqs.size(); ++i) {
        const double f = freqs[i];
        if (f < m_minFreqHz || f > m_maxFreqHz) {
            continue;
        }
        const int x = freqToX(f, phaseRect);
        const double ratio = (phases[i] - m_minPhaseDeg) / (m_maxPhaseDeg - m_minPhaseDeg);
        const int y = phaseRect.bottom() - static_cast<int>(std::round(ratio * phaseRect.height()));

        if (firstPhase) {
            phasePath.moveTo(x, y);
            firstPhase = false;
        } else {
            phasePath.lineTo(x, y);
        }
    }
    painter.setPen(QPen(QColor(255, 145, 0), 2));
    painter.drawPath(phasePath);
}

void BodePlotWidget::drawStabilityAnnotations(QPainter& painter, const QRect& magRect, const QRect& phaseRect)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    const auto& margins = m_result.margins;

    // Gain Crossover Frequency (f_gc) annotation
    if (margins.hasGainCrossover && margins.gainCrossoverFreqHz >= m_minFreqHz
        && margins.gainCrossoverFreqHz <= m_maxFreqHz) {
        const int x = freqToX(margins.gainCrossoverFreqHz, magRect);
        painter.setPen(QPen(QColor(76, 175, 80, 180), 1, Qt::DashDotLine));
        painter.drawLine(x, magRect.top(), x, phaseRect.bottom());

        painter.setPen(QColor(76, 175, 80));
        painter.drawText(QRect(x + 4, magRect.top() + 18, 120, 16), Qt::AlignLeft,
            QString::asprintf("f_gc = %0.2f Hz", margins.gainCrossoverFreqHz));
        painter.drawText(QRect(x + 4, phaseRect.top() + 18, 120, 16), Qt::AlignLeft,
            QString::asprintf("Pm = %0.1f°", margins.phaseMarginDeg));
    }

    // Phase Crossover Frequency (f_180) annotation
    if (margins.hasPhaseCrossover && margins.phaseCrossoverFreqHz >= m_minFreqHz
        && margins.phaseCrossoverFreqHz <= m_maxFreqHz) {
        const int x = freqToX(margins.phaseCrossoverFreqHz, magRect);
        painter.setPen(QPen(QColor(255, 82, 82, 180), 1, Qt::DashDotLine));
        painter.drawLine(x, magRect.top(), x, phaseRect.bottom());

        painter.setPen(QColor(255, 82, 82));
        painter.drawText(QRect(x + 4, magRect.bottom() - 32, 120, 16), Qt::AlignLeft,
            QString::asprintf("f_180 = %0.2f Hz", margins.phaseCrossoverFreqHz));
        painter.drawText(QRect(x + 4, magRect.bottom() - 16, 120, 16), Qt::AlignLeft,
            QString::asprintf("Gm = %0.1f dB", margins.gainMarginDb));
    }

    // Structural resonance mode markers
    for (const auto& peak : m_result.resonancePeaks) {
        if (peak.frequencyHz >= m_minFreqHz && peak.frequencyHz <= m_maxFreqHz) {
            const int x = freqToX(peak.frequencyHz, magRect);
            const double ratio = (peak.peakMagnitudeDb - m_minMagDb) / (m_maxMagDb - m_minMagDb);
            const int y = magRect.bottom() - static_cast<int>(std::round(ratio * magRect.height()));

            painter.setPen(QColor(255, 235, 59));
            painter.setBrush(QColor(255, 235, 59));
            painter.drawEllipse(QPoint(x, y), 3, 3);

            painter.drawText(
                QRect(x - 40, y - 18, 80, 14), Qt::AlignCenter, QString::asprintf("Res %0.1fHz", peak.frequencyHz));
        }
    }
}

void BodePlotWidget::drawCursorCrosshair(QPainter& painter, const QRect& magRect, const QRect& phaseRect)
{
    if (m_cursorPos.x() < magRect.left() || m_cursorPos.x() > magRect.right()) {
        return;
    }

    const int x = m_cursorPos.x();
    const double f = xToFreq(x, magRect);

    // Draw vertical crosshair line across both plots
    painter.setPen(QPen(QColor(255, 255, 255, 90), 1, Qt::DashLine));
    painter.drawLine(x, magRect.top(), x, phaseRect.bottom());

    // Interpolate magnitude, phase, and coherence at cursor frequency f
    const auto& freqs = m_result.bode.frequenciesHz;
    const auto& mags = m_result.bode.magnitudeDb;
    const auto& phases = m_result.bode.phaseDeg;
    const auto& coh = m_result.bode.coherence;

    double curMag = 0.0;
    double curPhase = 0.0;
    double curCoh = 0.0;

    for (std::size_t i = 1U; i < freqs.size(); ++i) {
        if (freqs[i] >= f) {
            const double frac = (f - freqs[i - 1U]) / (freqs[i] - freqs[i - 1U] + 1e-12);
            curMag = mags[i - 1U] + frac * (mags[i] - mags[i - 1U]);
            curPhase = phases[i - 1U] + frac * (phases[i] - phases[i - 1U]);
            curCoh = coh[i - 1U] + frac * (coh[i] - coh[i - 1U]);
            break;
        }
    }

    // Header telemetry badge
    painter.setPen(QColor(240, 246, 252));
    QFont font = painter.font();
    font.setPointSize(8);
    font.setBold(true);
    painter.setFont(font);

    const QString badge = QString::asprintf(
        "Cursor: %0.2f Hz  |  Mag: %+0.1f dB  |  Phase: %+0.1f°  |  Coh: %0.2f", f, curMag, curPhase, curCoh);
    painter.drawText(QRect(magRect.left() + 180, 4, magRect.width() - 180, 18), Qt::AlignRight, badge);
}

} // namespace PelcoD
