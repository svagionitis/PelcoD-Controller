#include "CameraStreamBinder.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

struct CameraStreamBinder::SharedBuffer {
    mutable std::mutex mutex;
    std::deque<CameraTelemetry> cameraHistory;
    std::deque<GimbalTelemetry> gimbalHistory;
    static constexpr std::size_t kMaxHistory { 150 };

    void recordCamera(const CameraTelemetry& telem)
    {
        std::lock_guard<std::mutex> lock(mutex);
        cameraHistory.push_back(telem);
        while (cameraHistory.size() > kMaxHistory) {
            cameraHistory.pop_front();
        }
    }

    void recordGimbal(const GimbalTelemetry& telem)
    {
        std::lock_guard<std::mutex> lock(mutex);
        gimbalHistory.push_back(telem);
        while (gimbalHistory.size() > kMaxHistory) {
            gimbalHistory.pop_front();
        }
    }
};

CameraStreamBinder::CameraStreamBinder(std::shared_ptr<IPayload> payload, VideoStreamProfile profile)
    : m_payload(std::move(payload))
    , m_buffer(std::make_shared<SharedBuffer>())
{
    if (m_payload) {
        if (profile == VideoStreamProfile::Thermal) {
            m_camera = m_payload->secondaryCamera();
            if (!m_camera) {
                m_camera = m_payload->primaryCamera();
            }
        } else {
            m_camera = m_payload->primaryCamera();
            if (!m_camera) {
                m_camera = m_payload->secondaryCamera();
            }
        }
        m_ptu = m_payload->panTilt();
    }

    if (m_camera) {
        const auto desc = m_camera->streamDescriptor(profile);
        if (desc.has_value()) {
            m_descriptor = *desc;
        } else {
            const auto uri = m_camera->videoStreamUri(profile);
            m_descriptor.uri = uri;
            m_descriptor.profile = profile;
            m_descriptor.transport = deduceTransportProtocol(uri);
        }
    } else {
        m_descriptor.profile = profile;
    }

    std::weak_ptr<SharedBuffer> weakBuf = m_buffer;
    if (m_camera) {
        m_camera->registerTelemetryCallback([weakBuf](const CameraTelemetry& telem) {
            if (auto buf = weakBuf.lock()) {
                buf->recordCamera(telem);
            }
        });
        m_buffer->recordCamera(m_camera->currentTelemetry());
    }

    if (m_ptu) {
        m_ptu->registerTelemetryCallback([weakBuf](const GimbalTelemetry& telem) {
            if (auto buf = weakBuf.lock()) {
                buf->recordGimbal(telem);
            }
        });
        m_buffer->recordGimbal(m_ptu->currentTelemetry());
    }
}

CameraStreamBinder::CameraStreamBinder(std::shared_ptr<ICameraPayload> camera,
    std::shared_ptr<IPanTiltUnit> ptu,
    VideoStreamDescriptor descriptor)
    : m_camera(std::move(camera))
    , m_ptu(std::move(ptu))
    , m_descriptor(std::move(descriptor))
    , m_buffer(std::make_shared<SharedBuffer>())
{
    std::weak_ptr<SharedBuffer> weakBuf = m_buffer;
    if (m_camera) {
        m_camera->registerTelemetryCallback([weakBuf](const CameraTelemetry& telem) {
            if (auto buf = weakBuf.lock()) {
                buf->recordCamera(telem);
            }
        });
        m_buffer->recordCamera(m_camera->currentTelemetry());
    }

    if (m_ptu) {
        m_ptu->registerTelemetryCallback([weakBuf](const GimbalTelemetry& telem) {
            if (auto buf = weakBuf.lock()) {
                buf->recordGimbal(telem);
            }
        });
        m_buffer->recordGimbal(m_ptu->currentTelemetry());
    }
}

CameraStreamBinder::~CameraStreamBinder() = default;

void CameraStreamBinder::setPlatformState(const Klv::GeoPoint3D& platformPos, double headingDeg) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_platformPos = platformPos;
    m_platformHeadingDeg = headingDeg;
}

void CameraStreamBinder::clearPlatformState() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_platformPos.reset();
}

void CameraStreamBinder::setFrameCallback(FrameCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameCallback = std::move(cb);
}

void CameraStreamBinder::ingestFrame(RawVideoFrame frame)
{
    const auto syncd = bindFrame(std::move(frame));
    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_frameCallback;
    }
    if (cb) {
        cb(syncd);
    }
}

SynchronizedVideoFrame CameraStreamBinder::bindFrame(RawVideoFrame frame) const
{
    SynchronizedVideoFrame syncd;
    syncd.frame = std::move(frame);
    syncd.cameraTelemetry = findMatchingCameraTelemetry(syncd.frame.timestamp);
    syncd.gimbalTelemetry = findMatchingGimbalTelemetry(syncd.frame.timestamp);
    syncd.streamDescriptor = m_descriptor;

    std::optional<Klv::GeoPoint3D> platformPos;
    double platformHeading { 0.0 };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        platformPos = m_platformPos;
        platformHeading = m_platformHeadingDeg;
    }

    if (platformPos.has_value()) {
        std::shared_ptr<IDemProvider> dem = m_payload ? m_payload->demProvider() : nullptr;
        if (dem) {
            syncd.targetGroundIntersection = GeoreferenceUtils::computeTargetFromDem(
                *dem, *platformPos, platformHeading,
                syncd.gimbalTelemetry.panAngleDeg, syncd.gimbalTelemetry.tiltAngleDeg);
            syncd.frustumCorners = GeoreferenceUtils::computeFrustumCorners(
                *dem, *platformPos, platformHeading,
                syncd.gimbalTelemetry.panAngleDeg, syncd.gimbalTelemetry.tiltAngleDeg,
                syncd.cameraTelemetry.horizontalFovDeg, syncd.cameraTelemetry.verticalFovDeg,
                syncd.gimbalTelemetry.rollAngleDeg);
        } else {
            syncd.targetGroundIntersection = GeoreferenceUtils::computeTargetFromGroundIntersection(
                *platformPos, platformHeading,
                syncd.gimbalTelemetry.panAngleDeg, syncd.gimbalTelemetry.tiltAngleDeg, 0.0);
            syncd.frustumCorners = GeoreferenceUtils::computeFrustumCorners(
                *platformPos, platformHeading,
                syncd.gimbalTelemetry.panAngleDeg, syncd.gimbalTelemetry.tiltAngleDeg,
                syncd.cameraTelemetry.horizontalFovDeg, syncd.cameraTelemetry.verticalFovDeg, 0.0,
                syncd.gimbalTelemetry.rollAngleDeg);
        }
    }

    return syncd;
}

const VideoStreamDescriptor& CameraStreamBinder::descriptor() const noexcept
{
    return m_descriptor;
}

std::shared_ptr<ICameraPayload> CameraStreamBinder::camera() const noexcept
{
    return m_camera;
}

std::shared_ptr<IPanTiltUnit> CameraStreamBinder::panTilt() const noexcept
{
    return m_ptu;
}

std::shared_ptr<IPayload> CameraStreamBinder::payload() const noexcept
{
    return m_payload;
}

void CameraStreamBinder::recordCameraTelemetry(const CameraTelemetry& telem)
{
    if (m_buffer) {
        m_buffer->recordCamera(telem);
    }
}

void CameraStreamBinder::recordGimbalTelemetry(const GimbalTelemetry& telem)
{
    if (m_buffer) {
        m_buffer->recordGimbal(telem);
    }
}

CameraTelemetry CameraStreamBinder::findMatchingCameraTelemetry(
    std::chrono::system_clock::time_point timestamp) const
{
    if (!m_buffer) {
        return m_camera ? m_camera->currentTelemetry() : CameraTelemetry {};
    }

    std::lock_guard<std::mutex> lock(m_buffer->mutex);
    if (m_buffer->cameraHistory.empty()) {
        return m_camera ? m_camera->currentTelemetry() : CameraTelemetry {};
    }

    const auto& hist = m_buffer->cameraHistory;
    auto it = std::lower_bound(hist.begin(), hist.end(), timestamp,
        [](const CameraTelemetry& sample, const std::chrono::system_clock::time_point& tp) {
            return sample.timestamp < tp;
        });

    if (it == hist.begin()) {
        return *it;
    }
    if (it == hist.end()) {
        return hist.back();
    }

    const auto& s2 = *it;
    const auto& s1 = *(it - 1);
    const auto dtTotal = std::chrono::duration<double>(s2.timestamp - s1.timestamp).count();
    if (dtTotal <= 1e-4) {
        return s2;
    }
    const auto dtTarget = std::chrono::duration<double>(timestamp - s1.timestamp).count();
    const double alpha = std::clamp(dtTarget / dtTotal, 0.0, 1.0);

    CameraTelemetry result = s1;
    result.normalizedZoom = s1.normalizedZoom + alpha * (s2.normalizedZoom - s1.normalizedZoom);
    result.opticalZoomFactor = s1.opticalZoomFactor + alpha * (s2.opticalZoomFactor - s1.opticalZoomFactor);
    result.focusDistanceNormalized
        = s1.focusDistanceNormalized + alpha * (s2.focusDistanceNormalized - s1.focusDistanceNormalized);
    result.irisNormalized = s1.irisNormalized + alpha * (s2.irisNormalized - s1.irisNormalized);
    result.horizontalFovDeg = s1.horizontalFovDeg + alpha * (s2.horizontalFovDeg - s1.horizontalFovDeg);
    result.verticalFovDeg = s1.verticalFovDeg + alpha * (s2.verticalFovDeg - s1.verticalFovDeg);
    result.timestamp = timestamp;
    return result;
}

GimbalTelemetry CameraStreamBinder::findMatchingGimbalTelemetry(
    std::chrono::system_clock::time_point timestamp) const
{
    if (!m_buffer) {
        return m_ptu ? m_ptu->currentTelemetry() : GimbalTelemetry {};
    }

    std::lock_guard<std::mutex> lock(m_buffer->mutex);
    if (m_buffer->gimbalHistory.empty()) {
        return m_ptu ? m_ptu->currentTelemetry() : GimbalTelemetry {};
    }

    const auto& hist = m_buffer->gimbalHistory;
    auto it = std::lower_bound(hist.begin(), hist.end(), timestamp,
        [](const GimbalTelemetry& sample, const std::chrono::system_clock::time_point& tp) {
            return sample.timestamp < tp;
        });

    if (it == hist.begin()) {
        return *it;
    }
    if (it == hist.end()) {
        return hist.back();
    }

    const auto& s2 = *it;
    const auto& s1 = *(it - 1);
    const auto dtTotal = std::chrono::duration<double>(s2.timestamp - s1.timestamp).count();
    if (dtTotal <= 1e-4) {
        return s2;
    }
    const auto dtTarget = std::chrono::duration<double>(timestamp - s1.timestamp).count();
    const double alpha = std::clamp(dtTarget / dtTotal, 0.0, 1.0);

    GimbalTelemetry result = s1;
    // Interpolate pan using shortest angular distance
    double panDiff = s2.panAngleDeg - s1.panAngleDeg;
    while (panDiff > 180.0) {
        panDiff -= 360.0;
    }
    while (panDiff < -180.0) {
        panDiff += 360.0;
    }
    double pan = s1.panAngleDeg + alpha * panDiff;
    while (pan >= 360.0) {
        pan -= 360.0;
    }
    while (pan < 0.0) {
        pan += 360.0;
    }
    result.panAngleDeg = pan;

    result.tiltAngleDeg = s1.tiltAngleDeg + alpha * (s2.tiltAngleDeg - s1.tiltAngleDeg);
    result.rollAngleDeg = s1.rollAngleDeg + alpha * (s2.rollAngleDeg - s1.rollAngleDeg);
    result.panRateDegPerSec = s1.panRateDegPerSec + alpha * (s2.panRateDegPerSec - s1.panRateDegPerSec);
    result.tiltRateDegPerSec = s1.tiltRateDegPerSec + alpha * (s2.tiltRateDegPerSec - s1.tiltRateDegPerSec);
    result.rollRateDegPerSec = s1.rollRateDegPerSec + alpha * (s2.rollRateDegPerSec - s1.rollRateDegPerSec);
    result.isHorizonLeveled = (alpha >= 0.5 ? s2.isHorizonLeveled : s1.isHorizonLeveled);
    result.timestamp = timestamp;
    return result;
}

} // namespace PayloadHal
