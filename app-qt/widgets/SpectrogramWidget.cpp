/// @file SpectrogramWidget.cpp
/// @brief Implementation of interactive 2D Time-Frequency Spectrogram Waterfall widget.

#include "SpectrogramWidget.h"

#include <QPainter>
#include <QPen>
#include <QFont>
#include <algorithm>
#include <cmath>

namespace PelcoDApp {

SpectrogramWidget::SpectrogramWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void SpectrogramWidget::setColorPreset(PelcoD::SpectrogramColorMap::Preset preset)
{
    QMutexLocker locker(&m_mutex);
    m_preset = preset;
    update();
}

PelcoD::SpectrogramColorMap::Preset SpectrogramWidget::colorPreset() const noexcept
{
    QMutexLocker locker(&m_mutex);
    return m_preset;
}

void SpectrogramWidget::updateSpectrogram(const std::vector<PelcoD::SpectrogramFrame>& frames,
    const std::vector<double>& freqs)
{
    QMutexLocker locker(&m_mutex);
    m_frames = frames;
    m_freqs = freqs;

    if (!m_frames.empty()) {
        const auto& latest = m_frames.back();
        m_latestPeakHz = latest.peakFrequencyHz;
        m_latestCentroidHz = latest.spectralCentroidHz;
        m_latestFlatness = latest.spectralFlatness;
        m_latestPowerRatio = latest.powerRatio;
    }

    update();
}

void SpectrogramWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect bounds = rect();
    painter.fillRect(bounds, QColor(24, 24, 28)); // Dark surveillance theme background

    QMutexLocker locker(&m_mutex);

    if (m_frames.empty() || m_freqs.empty()) {
        painter.setPen(QColor(120, 120, 130));
        painter.setFont(QFont("Segoe UI", 10));
        painter.drawText(bounds, Qt::AlignCenter, tr("Waiting for signal telemetry..."));
        return;
    }

    const int headerHeight = 24;
    const int sliceWidth = 80;
    const int margin = 4;
    const int axisLeft = 36;

    const QRect headerRect(margin, margin, bounds.width() - 2 * margin, headerHeight);
    const QRect waterfallRect(axisLeft, headerHeight + margin, bounds.width() - axisLeft - sliceWidth - 2 * margin,
        bounds.height() - headerHeight - 2 * margin);
    const QRect sliceRect(waterfallRect.right() + margin, waterfallRect.top(), sliceWidth, waterfallRect.height());

    renderBadges(painter, headerRect);
    renderWaterfall(painter, waterfallRect);
    renderSlice(painter, sliceRect);
}

void SpectrogramWidget::renderBadges(QPainter& painter, const QRect& rect)
{
    painter.save();
    painter.setFont(QFont("Segoe UI", 9, QFont::Bold));

    // Badge 1: Peak Frequency
    QString peakStr = QString("PEAK: %1 Hz").arg(m_latestPeakHz, 0, 'f', 1);
    QRect peakRect(rect.left(), rect.top() + 2, 110, rect.height() - 4);
    painter.fillRect(peakRect, QColor(35, 65, 45));
    painter.setPen(QColor(76, 175, 80)); // Vibrant Green
    painter.drawText(peakRect, Qt::AlignCenter, peakStr);

    // Badge 2: Spectral Centroid
    QString centStr = QString("CENTROID: %1 Hz").arg(m_latestCentroidHz, 0, 'f', 1);
    QRect centRect(peakRect.right() + 6, rect.top() + 2, 130, rect.height() - 4);
    painter.fillRect(centRect, QColor(25, 55, 75));
    painter.setPen(QColor(33, 150, 243)); // Cyan/Blue
    painter.drawText(centRect, Qt::AlignCenter, centStr);

    // Badge 3: Flatness (Entropy)
    const bool isHarmonic = (m_latestFlatness < 0.15 && m_latestPowerRatio > 0.35);
    QString flatStr = isHarmonic ? QString("HUNTING DETECTED") : QString("NOISE: %1").arg(m_latestFlatness, 0, 'f', 2);
    QRect flatRect(centRect.right() + 6, rect.top() + 2, 140, rect.height() - 4);
    painter.fillRect(flatRect, isHarmonic ? QColor(80, 25, 25) : QColor(45, 45, 52));
    painter.setPen(isHarmonic ? QColor(244, 67, 54) : QColor(160, 160, 170));
    painter.drawText(flatRect, Qt::AlignCenter, flatStr);

    painter.restore();
}

void SpectrogramWidget::renderWaterfall(QPainter& painter, const QRect& rect)
{
    if (rect.width() <= 10 || rect.height() <= 10) {
        return;
    }

    const int timeCols = static_cast<int>(m_frames.size());
    const int freqRows = static_cast<int>(m_freqs.size());

    // Generate/update waterfall QImage
    QImage img(timeCols, freqRows, QImage::Format_RGB32);

    for (int c = 0; c < timeCols; ++c) {
        const auto& frame = m_frames[c];
        for (int r = 0; r < freqRows; ++r) {
            // Row 0 in QImage is top (highest frequency), Row freqRows-1 is bottom (0 Hz)
            const int binIdx = (freqRows - 1) - r;
            const double db = (binIdx < static_cast<int>(frame.dbSpectrum.size())) ? frame.dbSpectrum[binIdx] : m_minDb;
            const auto rgb = PelcoD::SpectrogramColorMap::mapDb(db, m_minDb, m_maxDb, m_preset);
            img.setPixel(c, r, qRgb(rgb.r, rgb.g, rgb.b));
        }
    }

    // Paint scaled waterfall canvas
    painter.drawImage(rect, img);

    // Draw border
    painter.setPen(QColor(60, 60, 70));
    painter.drawRect(rect);

    // Draw Frequency Y-axis labels and grid ticks
    painter.save();
    painter.setFont(QFont("Segoe UI", 8));
    painter.setPen(QColor(150, 150, 160));

    const double maxFreq = m_freqs.back();
    const int numTicks = 4;
    for (int i = 0; i <= numTicks; ++i) {
        double frac = static_cast<double>(i) / numTicks;
        double f = frac * maxFreq;
        int y = rect.bottom() - static_cast<int>(frac * rect.height());

        // Tick mark and text
        painter.drawLine(rect.left() - 3, y, rect.left(), y);
        QString label = QString::number(f, 'f', 0);
        painter.drawText(QRect(0, y - 8, rect.left() - 4, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // Draw peak tracking ridge line across frames
    if (timeCols > 1 && maxFreq > 0.0) {
        QPen ridgePen(QColor(255, 255, 255, 180));
        ridgePen.setStyle(Qt::DashLine);
        ridgePen.setWidth(1);
        painter.setPen(ridgePen);

        QPoint prevPt;
        for (int c = 0; c < timeCols; ++c) {
            double normF = std::clamp(m_frames[c].peakFrequencyHz / maxFreq, 0.0, 1.0);
            int px = rect.left() + static_cast<int>((static_cast<double>(c) / (timeCols - 1)) * rect.width());
            int py = rect.bottom() - static_cast<int>(normF * rect.height());

            QPoint curPt(px, py);
            if (c > 0) {
                painter.drawLine(prevPt, curPt);
            }
            prevPt = curPt;
        }
    }

    painter.restore();
}

void SpectrogramWidget::renderSlice(QPainter& painter, const QRect& rect)
{
    if (rect.width() <= 10 || rect.height() <= 10 || m_frames.empty()) {
        return;
    }

    painter.save();

    // Background panel for slice plot
    painter.fillRect(rect, QColor(30, 30, 36));
    painter.setPen(QColor(60, 60, 70));
    painter.drawRect(rect);

    const auto& latest = m_frames.back();
    const int freqRows = static_cast<int>(m_freqs.size());
    const double dbSpan = m_maxDb - m_minDb;

    // Draw instantaneous spectrum curve
    QPolygonF poly;
    poly.reserve(freqRows);

    for (int r = 0; r < freqRows; ++r) {
        const double db = (r < static_cast<int>(latest.dbSpectrum.size())) ? latest.dbSpectrum[r] : m_minDb;
        const double normDb = std::clamp((db - m_minDb) / dbSpan, 0.0, 1.0);

        const double y = rect.bottom() - (static_cast<double>(r) / (freqRows - 1)) * rect.height();
        const double x = rect.left() + normDb * (rect.width() - 4);
        poly.append(QPointF(x, y));
    }

    QPen slicePen(QColor(255, 215, 0)); // Gold/Yellow curve
    slicePen.setWidth(2);
    painter.setPen(slicePen);
    painter.drawPolyline(poly);

    // Draw peak frequency indicator circle
    const double maxFreq = m_freqs.back();
    if (maxFreq > 0.0) {
        double normPeakF = std::clamp(m_latestPeakHz / maxFreq, 0.0, 1.0);
        int peakY = rect.bottom() - static_cast<int>(normPeakF * rect.height());

        painter.setBrush(QColor(255, 50, 50));
        painter.setPen(Qt::white);
        painter.drawEllipse(QPoint(rect.right() - 8, peakY), 4, 4);
    }

    painter.restore();
}

} // namespace PelcoDApp
