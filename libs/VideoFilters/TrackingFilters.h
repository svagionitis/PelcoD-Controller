/**
 * @file TrackingFilters.h
 * @brief Video filters for motion detection, target tracking, kinematics estimation, tripwire intrusion detection, and optical flow fields.
 */

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "DecoderTypes.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#ifndef VIDEOFILTERS_API
#define VIDEOFILTERS_API
#endif

#if defined(PELCOD_HAS_FILTERS)

namespace Video::Filters {

/**
 * @class MovingTargetIndicatorFilter
 * @brief Ground Moving Target Indication (GMTI/MTI) detecting moving objects against a static/stabilized background.
 */
class VIDEOFILTERS_API MovingTargetIndicatorFilter : public IFrameProcessor {
public:
    struct TargetBox {
        int x { 0 };
        int y { 0 };
        int width { 0 };
        int height { 0 };
        int id { 0 };
    };

    MovingTargetIndicatorFilter(int minArea = 100, int maxArea = 50000, int maxTargets = 16);
    ~MovingTargetIndicatorFilter() override;

    MovingTargetIndicatorFilter(const MovingTargetIndicatorFilter&) = delete;
    MovingTargetIndicatorFilter& operator=(const MovingTargetIndicatorFilter&) = delete;
    MovingTargetIndicatorFilter(MovingTargetIndicatorFilter&&) noexcept;
    MovingTargetIndicatorFilter& operator=(MovingTargetIndicatorFilter&&) noexcept;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setMinArea(int minArea)
    {
        m_minArea = minArea;
    }
    int getMinArea() const
    {
        return m_minArea;
    }

    void setMaxArea(int maxArea)
    {
        m_maxArea = maxArea;
    }
    int getMaxArea() const
    {
        return m_maxArea;
    }

    void setMaxTargets(int maxTargets)
    {
        m_maxTargets = maxTargets;
    }
    int getMaxTargets() const
    {
        return m_maxTargets;
    }

    void reset();
    std::size_t getTargetCount() const;
    std::vector<TargetBox> getTargets() const;

private:
    int m_minArea;
    int m_maxArea;
    int m_maxTargets;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class OpticalFlowFieldFilter
 * @brief Computes dense Gunnar Farneback optical flow and visualizes velocity vectors or color fields.
 */
class VIDEOFILTERS_API OpticalFlowFieldFilter : public IFrameProcessor {
public:
    enum class DisplayMode {
        VectorArrows, ///< Tactical velocity arrows on a spatial grid
        ColorFlow ///< Directional color wheel (Hue = angle, Saturation/Value = speed)
    };

    OpticalFlowFieldFilter(DisplayMode mode = DisplayMode::VectorArrows, int gridStep = 16, double minVelocity = 1.5,
        double arrowScale = 2.0);
    ~OpticalFlowFieldFilter() override;

    OpticalFlowFieldFilter(const OpticalFlowFieldFilter&) = delete;
    OpticalFlowFieldFilter& operator=(const OpticalFlowFieldFilter&) = delete;
    OpticalFlowFieldFilter(OpticalFlowFieldFilter&&) noexcept;
    OpticalFlowFieldFilter& operator=(OpticalFlowFieldFilter&&) noexcept;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setDisplayMode(DisplayMode mode)
    {
        m_mode = mode;
    }
    DisplayMode getDisplayMode() const
    {
        return m_mode;
    }

    void setGridStep(int step)
    {
        m_gridStep = std::max(4, step);
    }
    int getGridStep() const
    {
        return m_gridStep;
    }

    void setMinVelocity(double minVel)
    {
        m_minVelocity = std::max(0.0, minVel);
    }
    double getMinVelocity() const
    {
        return m_minVelocity;
    }

    void setArrowScale(double scale)
    {
        m_arrowScale = scale;
    }
    double getArrowScale() const
    {
        return m_arrowScale;
    }

    void reset();

private:
    DisplayMode m_mode;
    int m_gridStep;
    double m_minVelocity;
    double m_arrowScale;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class CentroidTargetTrackerFilter
 * @brief Locks onto and tracks a visual target, computing azimuth/elevation boresight error telemetry for PTZ tracking.
 */
class VIDEOFILTERS_API CentroidTargetTrackerFilter : public IFrameProcessor {
public:
    struct TargetState {
        int x { 0 };
        int y { 0 };
        int width { 0 };
        int height { 0 };
        double errorX { 0.0 }; ///< Normalized X offset from boresight (-1.0 left to +1.0 right)
        double errorY { 0.0 }; ///< Normalized Y offset from boresight (-1.0 up to +1.0 down)
        double vx { 0.0 }; ///< Target velocity X in px/frame
        double vy { 0.0 }; ///< Target velocity Y in px/frame
        double ax { 0.0 }; ///< Target acceleration X in px/frame^2
        double ay { 0.0 }; ///< Target acceleration Y in px/frame^2
        double confidence { 0.0 }; ///< Tracking confidence (0.0 to 1.0)
        bool locked { false };
        bool isCoasting { false }; ///< True if target is temporarily occluded and coasting on prediction
        double predictedErrorX { 0.0 }; ///< Latency-compensated predicted boresight error X
        double predictedErrorY { 0.0 }; ///< Latency-compensated predicted boresight error Y
        double scaleFactor { 1.0 }; ///< Current scale ratio relative to initial acquisition
        double appearanceScore { 1.0 }; ///< Appearance signature correlation score (0.0 to 1.0)
        double normalizedWidth { 0.0 }; ///< Target width normalized by viewport frame width [0.0 to 1.0]
        double normalizedHeight { 0.0 }; ///< Target height normalized by viewport frame height [0.0 to 1.0]
        double turnRateRps { 0.0 }; ///< Estimated angular turn rate (rad/s)
        double headingDeg { 0.0 }; ///< Heading angle in degrees [0, 360)
        double predictedTargetX { 0.0 }; ///< Projected future X in pixels at lookahead horizon
        double predictedTargetY { 0.0 }; ///< Projected future Y in pixels at lookahead horizon
        double uncertaintyMajor { 0.0 }; ///< Kalman uncertainty semi-major axis (pixels)
        double uncertaintyMinor { 0.0 }; ///< Kalman uncertainty semi-minor axis (pixels)
        double uncertaintyAngleDeg { 0.0 }; ///< Kalman uncertainty orientation angle (degrees)
    };

    using TrajectoryConfig = PelcoD::Video::TrajectoryConfig;
    using PredictiveLeadConfig = PelcoD::Video::PredictiveLeadConfig;

    CentroidTargetTrackerFilter(bool autoAcquire = true, int targetWidth = 40, int targetHeight = 40);
    ~CentroidTargetTrackerFilter() override;

    CentroidTargetTrackerFilter(const CentroidTargetTrackerFilter&) = delete;
    CentroidTargetTrackerFilter& operator=(const CentroidTargetTrackerFilter&) = delete;
    CentroidTargetTrackerFilter(CentroidTargetTrackerFilter&&) noexcept;
    CentroidTargetTrackerFilter& operator=(CentroidTargetTrackerFilter&&) noexcept;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setAutoAcquire(bool autoAcquire)
    {
        m_autoAcquire = autoAcquire;
    }
    bool isAutoAcquire() const
    {
        return m_autoAcquire;
    }

    void acquireTarget(int x, int y, int width, int height);
    void releaseTarget();

    bool isTargetLocked() const;
    TargetState getTargetState(double lookaheadLatencySeconds = -1.0) const;

    /// @brief Sets the dynamic lookahead latency (in seconds) used by default.
    void setDynamicLookaheadLatency(double seconds) noexcept;

    /// @brief Gets the current dynamic lookahead latency (in seconds).
    double getDynamicLookaheadLatency() const noexcept;

    void setMaxCoastFrames(int frames) noexcept;
    int getMaxCoastFrames() const noexcept;
    void setProcessNoise(double qPos, double qVel, double qAcc = 1e-1) noexcept;
    void setMeasurementNoise(double rPos) noexcept;
    void setAdaptiveProcessNoiseEnabled(bool enabled) noexcept;
    bool isAdaptiveProcessNoiseEnabled() const noexcept;

    void setScaleAdaptation(bool enabled) noexcept;
    bool isScaleAdaptation() const noexcept;
    void setAppearanceFusion(bool enabled) noexcept;
    bool isAppearanceFusion() const noexcept;
    void setAppearanceLearningRate(double rate) noexcept;
    double getAppearanceLearningRate() const noexcept;

    void setTrajectoryTrail(bool enabled, int maxPoints = 30) noexcept;
    bool isTrajectoryTrail() const noexcept;
    int getTrajectoryMaxPoints() const noexcept;

    void setPredictiveVector(bool enabled, double lookaheadSeconds = 1.5) noexcept;
    bool isPredictiveVector() const noexcept;
    double getPredictiveVectorLookahead() const noexcept;

    /// @brief Configure historical trajectory breadcrumbs parameters.
    void setTrajectoryConfig(const TrajectoryConfig& config) noexcept;
    /// @brief Get current historical trajectory breadcrumbs parameters.
    [[nodiscard]] TrajectoryConfig getTrajectoryConfig() const noexcept;

    /// @brief Configure predictive lead vector and interception reticle parameters.
    void setPredictiveLeadConfig(const PredictiveLeadConfig& config) noexcept;
    /// @brief Get current predictive lead vector and interception reticle parameters.
    [[nodiscard]] PredictiveLeadConfig getPredictiveLeadConfig() const noexcept;

    /// @brief Set current mechanical PTZ boresight lead offset for on-screen setpoint marker.
    /// @param[in] leadX Normalized horizontal lead offset [-1.0, 1.0].
    /// @param[in] leadY Normalized vertical lead offset [-1.0, 1.0].
    void setBoresightLeadOffset(double leadX, double leadY) noexcept;
    /// @brief Get current mechanical PTZ boresight lead offset.
    [[nodiscard]] std::pair<double, double> getBoresightLeadOffset() const noexcept;

private:
    bool m_autoAcquire;
    int m_defaultWidth;
    int m_defaultHeight;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class PerimeterTripwireFilter
 * @brief Virtual security tripwire detecting directional line-crossing intrusions with visual alarms.
 */
class VIDEOFILTERS_API PerimeterTripwireFilter : public IFrameProcessor {
public:
    enum class Direction {
        Bidirectional, ///< Crossings in either direction trigger alarm
        A_to_B, ///< Only crossings from A side to B side trigger alarm
        B_to_A ///< Only crossings from B side to A side trigger alarm
    };

    PerimeterTripwireFilter(double x1Norm = 0.1, double y1Norm = 0.5, double x2Norm = 0.9, double y2Norm = 0.5,
        Direction direction = Direction::Bidirectional);
    ~PerimeterTripwireFilter() override;

    PerimeterTripwireFilter(const PerimeterTripwireFilter&) = delete;
    PerimeterTripwireFilter& operator=(const PerimeterTripwireFilter&) = delete;
    PerimeterTripwireFilter(PerimeterTripwireFilter&&) noexcept;
    PerimeterTripwireFilter& operator=(PerimeterTripwireFilter&&) noexcept;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setTripwire(double x1Norm, double y1Norm, double x2Norm, double y2Norm);
    void getTripwire(double& x1Norm, double& y1Norm, double& x2Norm, double& y2Norm) const;

    void setDirection(Direction dir)
    {
        m_direction = dir;
    }
    Direction getDirection() const
    {
        return m_direction;
    }

    bool hasAlarm() const;
    std::size_t getIntrusionCount() const;
    void resetIntrusionCount();

private:
    double m_x1Norm;
    double m_y1Norm;
    double m_x2Norm;
    double m_y2Norm;
    Direction m_direction;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class MotionHeatmapFilter
 * @brief Temporal motion accumulation buffer revealing high-traffic paths and unauthorized loitering zones.
 */
class VIDEOFILTERS_API MotionHeatmapFilter : public IFrameProcessor {
public:
    MotionHeatmapFilter(double decayFactor = 0.95, double opacity = 0.40, int threshold = 20);
    ~MotionHeatmapFilter() override;

    MotionHeatmapFilter(const MotionHeatmapFilter&) = delete;
    MotionHeatmapFilter& operator=(const MotionHeatmapFilter&) = delete;
    MotionHeatmapFilter(MotionHeatmapFilter&&) noexcept;
    MotionHeatmapFilter& operator=(MotionHeatmapFilter&&) noexcept;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setDecayFactor(double decay)
    {
        m_decayFactor = std::max(0.01, std::min(0.999, decay));
    }
    double getDecayFactor() const
    {
        return m_decayFactor;
    }

    void setOpacity(double opacity)
    {
        m_opacity = std::max(0.0, std::min(1.0, opacity));
    }
    double getOpacity() const
    {
        return m_opacity;
    }

    void setThreshold(int thresh)
    {
        m_threshold = std::max(1, std::min(255, thresh));
    }
    int getThreshold() const
    {
        return m_threshold;
    }

    void reset();

private:
    double m_decayFactor;
    double m_opacity;
    int m_threshold;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Video::Filters

namespace Video {
using Filters::CentroidTargetTrackerFilter;
using Filters::MotionHeatmapFilter;
using Filters::MovingTargetIndicatorFilter;
using Filters::OpticalFlowFieldFilter;
using Filters::PerimeterTripwireFilter;
} // namespace Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
