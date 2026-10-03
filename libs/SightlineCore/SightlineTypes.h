#pragma once

/// @file SightlineTypes.h
/// @brief Fundamental types, enumerations, and coordinate definitions for the Sightline SLA protocol.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Sightline {

/// @class ByteView
/// @brief Non-owning, contiguous read-only view of a sequence of bytes.
/// @details Provides lightweight, zero-allocation slicing and inspection of contiguous byte sequences
///          aliasing raw memory, fixed arrays, or std::vector buffers without memory management overhead.
class ByteView {
public:
    /// @brief Default constructor for an empty ByteView.
    /// @details Invariants: points to nullptr with size 0.
    constexpr ByteView() noexcept = default;

    /// @brief Constructs a view from a raw byte pointer and count.
    /// @details Does not take ownership of the underlying buffer; caller must ensure buffer outlives the view.
    /// @param[in] data Pointer to contiguous byte sequence.
    /// @param[in] size Number of bytes in sequence.
    constexpr ByteView(const std::uint8_t* data, std::size_t size) noexcept
        : m_data { data }
        , m_size { size }
    {
    }

    /// @brief Constructs a view referencing an existing vector's buffer.
    /// @details Non-owning view referencing data() and size() of vector.
    /// @param[in] vec Source byte vector.
    ByteView(const std::vector<std::uint8_t>& vec) noexcept
        : m_data { vec.data() }
        , m_size { vec.size() }
    {
    }

    /// @brief Constructs a view referencing a fixed-size byte array.
    /// @details Compile-time sized array reference constructor.
    /// @tparam N Array element count.
    /// @param[in] arr Source byte array.
    template <std::size_t N>
    constexpr ByteView(const std::uint8_t (&arr)[N]) noexcept
        : m_data { arr }
        , m_size { N }
    {
    }

    /// @brief Returns pointer to contiguous byte memory.
    /// @details Pointer is read-only and non-owning.
    /// @return Pointer to underlying byte buffer.
    [[nodiscard]] constexpr const std::uint8_t* data() const noexcept
    {
        return m_data;
    }

    /// @brief Returns number of bytes in view.
    /// @details Returns count in elements.
    /// @return Number of bytes.
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    /// @brief Checks if view is empty.
    /// @details Evaluates size == 0.
    /// @return True if size is 0, false otherwise.
    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return m_size == 0U;
    }

    /// @brief Unchecked byte access by index.
    /// @details Performs direct array indexing without bounds check for max performance.
    /// @param[in] idx Zero-based byte index.
    /// @return Byte value reference.
    [[nodiscard]] constexpr const std::uint8_t& operator[](std::size_t idx) const noexcept
    {
        return m_data[idx];
    }

    /// @brief Iterator to start of byte view.
    /// @details Points to first byte in view.
    /// @return Pointer iterator.
    [[nodiscard]] constexpr const std::uint8_t* begin() const noexcept
    {
        return m_data;
    }

    /// @brief Iterator to end of byte view.
    /// @details Points one past last byte.
    /// @return Pointer iterator.
    [[nodiscard]] constexpr const std::uint8_t* end() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Extracts a sub-view safely bounded by current length.
    /// @details Safely clamps slice within [0, size). Returns empty ByteView if offset is out of range.
    /// @param[in] offset Starting byte offset.
    /// @param[in] count Number of bytes to include.
    /// @return Sliced ByteView.
    [[nodiscard]] ByteView subspan(std::size_t offset, std::size_t count = static_cast<std::size_t>(-1)) const noexcept
    {
        if (offset >= m_size) {
            return {};
        }
        const std::size_t actualCount
            = (count == static_cast<std::size_t>(-1) || offset + count > m_size) ? (m_size - offset) : count;
        return ByteView { m_data + offset, actualCount };
    }

    /// @brief Converts view to an owning byte vector.
    /// @details Performs a heap allocation and byte copy.
    /// @return Newly allocated byte vector copy.
    [[nodiscard]] std::vector<std::uint8_t> toVector() const
    {
        return { begin(), end() };
    }

    /// @brief Implicit conversion to owning byte vector for backward compatibility.
    /// @details Copies underlying bytes into a new std::vector.
    operator std::vector<std::uint8_t>() const
    {
        return toVector();
    }

    /// @brief Equality comparison with another ByteView.
    /// @details Compares size then uses std::memcmp on underlying bytes.
    /// @param[in] other View to compare against.
    /// @return True if sizes and contents match.
    [[nodiscard]] bool operator==(const ByteView& other) const noexcept
    {
        return m_size == other.m_size && (m_size == 0U || std::memcmp(m_data, other.m_data, m_size) == 0);
    }

    /// @brief Inequality comparison with another ByteView.
    /// @details Negation of operator==.
    /// @param[in] other View to compare against.
    /// @return True if sizes or contents differ.
    [[nodiscard]] bool operator!=(const ByteView& other) const noexcept
    {
        return !(*this == other);
    }

    /// @brief Equality comparison with an owning vector.
    /// @details Compares size then uses std::memcmp on buffer.
    /// @param[in] other Vector to compare against.
    /// @return True if sizes and contents match.
    [[nodiscard]] bool operator==(const std::vector<std::uint8_t>& other) const noexcept
    {
        return m_size == other.size() && (m_size == 0U || std::memcmp(m_data, other.data(), m_size) == 0);
    }

    /// @brief Inequality comparison with an owning vector.
    /// @details Negation of operator==.
    /// @param[in] other Vector to compare against.
    /// @return True if sizes or contents differ.
    [[nodiscard]] bool operator!=(const std::vector<std::uint8_t>& other) const noexcept
    {
        return !(*this == other);
    }

private:
    const std::uint8_t* m_data { nullptr };
    std::size_t m_size { 0U };
};

/// @brief Symmetric equality comparison for std::vector and ByteView.
/// @details Enables vector == ByteView syntax.
/// @param[in] a Owning vector.
/// @param[in] b Non-owning ByteView.
/// @return True if size and contents match.
[[nodiscard]] inline bool operator==(const std::vector<std::uint8_t>& a, const ByteView& b) noexcept
{
    return b == a;
}

/// @brief Symmetric inequality comparison for std::vector and ByteView.
/// @details Enables vector != ByteView syntax.
/// @param[in] a Owning vector.
/// @param[in] b Non-owning ByteView.
/// @return True if size or contents differ.
[[nodiscard]] inline bool operator!=(const std::vector<std::uint8_t>& a, const ByteView& b) noexcept
{
    return b != a;
}

/// @brief Packet signature header byte 1.
inline constexpr std::uint8_t HeaderByte1 { 0x51U };

/// @brief Packet signature header byte 2.
inline constexpr std::uint8_t HeaderByte2 { 0xACU };

/// @brief Default UDP inbound command port on Sightline hardware.
inline constexpr std::uint16_t DefaultHardwareCommandPort { 14001U };

/// @brief Default UDP client reply and unsolicited telemetry port.
inline constexpr std::uint16_t DefaultClientReplyPort { 14002U };

/// @brief Secondary user program UDP command port (symmetrical reply).
inline constexpr std::uint16_t DefaultSecondaryCommandPort { 14003U };

/// @brief Hardware discovery broadcast port.
inline constexpr std::uint16_t DefaultDiscoveryPort { 51000U };

/// @enum MessageId
/// @brief Sightline command and reply message identifiers from IDD-SLA-Protocol_3_11_6.
enum class MessageId : std::uint8_t {
    GetVersionNumber = 0x00U,
    ResetAllParameters = 0x01U,
    SetStabilizationParameters = 0x02U,
    GetStabilizationParameters = 0x03U,
    ResetStabilizationParameters = 0x04U,
    ModifyTracking = 0x05U,
    SetOverlayMode = 0x06U,
    GetOverlayMode = 0x07U,
    StartTracking = 0x08U,
    StopTracking = 0x09U,
    NudgeTrackingCoordinate = 0x0AU,
    CoordinateReportingMode = 0x0BU,
    SetTrackingParameters = 0x0CU,
    GetTrackingParameters = 0x0DU,
    SetRegistrationParameters = 0x0EU,
    GetRegistrationParameters = 0x0FU,
    SetVideoParameters = 0x10U,
    GetVideoParameters = 0x11U,
    SetStabilizationBias = 0x12U,
    SetMetadataValues = 0x13U,
    MetadataStaticValues = 0x14U,
    SetMetadataFrameValues = 0x15U,
    SetDisplayParameters = 0x16U,
    ModifyTrackIndex = 0x17U,
    SetADCParameters = 0x18U,
    GetADCParameters = 0x19U,
    SetEthernetVideoParameters = 0x1AU,
    GetEthernetVideoParameters = 0x1BU,
    SetNetworkParameters = 0x1CU,
    GetNetworkParameters = 0x1DU,
    SetSDRecordingParameters = 0x1EU,
    SetVideoMode = 0x1FU,
    GetVideoMode = 0x20U,
    SetVideoEnhancementParameters = 0x21U,
    GetVideoEnhancementParameters = 0x22U,
    SetH264Parameters = 0x23U,
    GetH264Parameters = 0x24U,
    SaveParameters = 0x25U,
    GetParameters = 0x28U,
    SetEthernetDisplayParameters = 0x29U,
    SetDisplayAdjustments = 0x2AU,
    SetDetectionParameters = 0x2DU,
    GetDetectionParameters = 0x2EU,
    SetBlendParameters = 0x2FU,
    GetBlendParameters = 0x30U,
    GetImageSize = 0x31U,
    DesignateSelectedTrackPrimary = 0x32U,
    ShiftSelectedTrack = 0x33U,
    NucParameters = 0x35U,
    ReadWriteNuc = 0x36U,
    SetAcquisitionParameters = 0x37U,
    GetAcquisitionParameters = 0x38U,
    GetEthernetDisplayParameters = 0x39U,
    GetDisplayParameters = 0x3AU,
    DrawObject = 0x3BU,
    StopSelectedTrack = 0x3CU,
    CommandPassThrough = 0x3DU,
    SetPortConfiguration = 0x3EU,
    GetPortConfiguration = 0x3FU,
    VersionNumber = 0x40U,
    CurrentStabilizationParameters = 0x41U,
    CurrentOverlayMode = 0x42U,
    TrackingPosition = 0x43U,
    CurrentTrackingParameters = 0x44U,
    CurrentRegistrationParameters = 0x45U,
    CurrentVideoParameters = 0x46U,
    CurrentADCParameters = 0x47U,
    CurrentEthernetVideoParameters = 0x48U,
    CurrentNetworkParameters = 0x49U,
    CurrentVideoEnhancementParameters = 0x4AU,
    CurrentVideoModeParameters = 0x4BU,
    CurrentBlendParameters = 0x4DU,
    CurrentImageSize = 0x4EU,
    CurrentAcquisitionParameters = 0x4FU,
    GetHardwareID = 0x50U,
    TrackingPositions = 0x51U,
    CurrentEthernetDisplayParameters = 0x52U,
    CurrentPortConfiguration = 0x53U,
    CurrentDetectionParameters = 0x54U,
    FocusStats = 0x55U,
    CurrentH264Parameters = 0x56U,
    CurrentDisplayParameters = 0x57U,
    CurrentSDCardRecordingStatus = 0x58U,
    CurrentSDCardDirectoryInfo = 0x59U,
    SendTraceStr = 0x5AU,
    CommandCamera = 0x5BU,
    DisplayAngle = 0x5CU,
    CurrentSnapShot = 0x5DU,
    SetSnapShot = 0x5EU,
    GetSnapShot = 0x5FU,
    DoSnapShot = 0x60U,
    SetKlvData = 0x61U,
    SetMetadataRate = 0x62U,
    SetSystemType = 0x63U,
    SetTelemetryDestination = 0x64U,
    CurrentSystemType = 0x65U,
    GetNetworkList = 0x66U,
    CurrentNetworkList = 0x67U,
    CurrentOverlayObjectsIds = 0x68U,
    CurrentOverlayObjectParameters = 0x6BU,
    SetLensMode = 0x6CU,
    CurrentLensStatus = 0x6DU,
    SetLensParameters = 0x6EU,
    CurrentLensParameters = 0x6FU,
    SetDigitalCameraParameters = 0x70U,
    CurrentDigitalCameraParameters = 0x71U,
    SetUserPalette = 0x72U,
    CurrentUserPalette = 0x73U,
    SetMultipleAlignment = 0x74U,
    CurrentMultipleAlignment = 0x75U,
    SetAdvancedDetectionParameters = 0x76U,
    CurrentAdvancedDetectionParameters = 0x77U,
    TrackingBoxPixelStats = 0x78U,
    DirectoryStatisticsReply = 0x79U,
    CurrentStabilizationBias = 0x7AU,
    AdvancedCaptureParameters = 0x7BU,
    SetDetectionRegionOfInterestParameters = 0x7CU,
    CurrentDetectionRegionOfInterestParameters = 0x7DU,
    UserWarningLevel = 0x7FU,
    SystemStatusMode = 0x80U,
    LandingAid = 0x81U,
    CameraSwitch = 0x82U,
    LandingPosition = 0x83U,
    SetVMTI = 0x84U,
    UserWarningMessage = 0x86U,
    SystemStatusMessage = 0x87U,
    DetailedTimingMessage = 0x88U,
    AppendedMetadata = 0x89U,
    FrameIndex = 0x8AU,
    CurrentMetadataValues = 0x8BU,
    CurrentMetadataFrameValues = 0x8CU,
    CurrentMetadataRate = 0x8DU,
    CurrentConfiguration = 0x8EU,
    ExternalProgram = 0x8FU,
    StreamingControl = 0x90U,
    DigitalVideoParserParameters = 0x91U,
    SetSystemValue = 0x92U,
    CurrentSystemValue = 0x93U,
    I2CCommand = 0x94U,
    FourAlignPoints = 0x95U,
    TagData = 0x96U,
    TagDataRate = 0x97U,
    TagSourceSelector = 0x98U,
    DecoderParameters = 0x99U,
    LogoParameters = 0x9BU,
    DrawOverlay = 0x9CU,
    TrackTrails = 0x9DU,
    RegistrationParameters = 0x9EU,
    StabilizationBias = 0x9FU,
    TrackingPositionsExtended = 0xA0U,
    DeadPixelStats = 0xA1U,
    InternalCommand = 0xA2U,
    InternalResponse = 0xA3U,
    VideoDisplay = 0xA4U,
    MultiDisplay = 0xA5U,
    Usb3VisionFeature = 0xA6U,
    CustomClassifier = 0xA7U,
    DeadPixel = 0xA8U,
    ClassifierParameters = 0xA9U,
    SendScript = 0xAAU,
    DoDetectSnapShot = 0xABU,
    AncillaryTextMetadata = 0xACU,
    VMTIChips = 0xADU,
    UserFont = 0xAEU,
    Noise3D = 0xAFU,
    CursorOnTarget = 0xB0U,
    LensParameters = 0xB1U,
    LensCommand = 0xB2U,
    FocusParameters = 0xB3U,
    LensRanges = 0xB4U,
    CustomAutoFocusParameters = 0xB5U,
    GPIO = 0xB6U,
    UsbWebcamFeature = 0xB7U,
    CreateDeviceOverlay = 0xB8U,
    BlendAlign = 0xB9U,
    CustomAIDetect = 0xBAU,
    CameraCapabilities = 0xBBU,
    CustomResponse = 0xBCU,
    TrackingMultiClass = 0xBDU,
    SendToBTS = 0xBEU,
    VMTIFields = 0xBFU,
    VmtiChips = VMTIChips,
    VmtiFields = VMTIFields,
    SetVmti = SetVMTI,
    CameraCalibration = 0xC0U,
    KlvClassFilters = 0xC1U,
    CameraParameterFile = 0xC2U,
    CommandAck = 0xC3U,
    SetFileRecordingParamsV2 = 0xC4U,
    DoSnapShotV2 = 0xC5U,
    FileRecordingEvent = 0xC6U,
    CurrentRecordingStatusV2 = 0xC7U,
    GetDirectoryListing = 0xC8U,
    DirectoryListingReply = 0xC9U,
    FileStorageManagement = 0xCAU,
    Unknown = 0xFFU
};

/// @enum TrackingMode
/// @brief Primary tracking algorithmic modes supported by Sightline SLA firmware.
/// @details Conforms to EAN-Target-Tracking Section 3.
enum class TrackingMode : std::uint8_t {
    Vehicle = 0U,        ///< Any moving or stationary ground vehicle (2.22+)
    Stationary = 1U,     ///< Fixed infrastructure or buildings (2.22+)
    Scene = 2U,          ///< Scene tracking / lock via frame registration (2.22+)
    NoRegistration = 3U, ///< Deprecated in 3.7; blue sky / untextured targets
    Static = 4U,         ///< Fixed camera coordinates (IR radiometric reporting)
    Drone = 5U,          ///< Agile, rapidly resizing aerial targets (2.24+)
    Person = 6U          ///< Human pedestrian tracking (3.01+)
};

/// @enum ModifyMode
/// @brief Target designation, modification, and cueing operations.
/// @details Conforms to EAN-Target-Tracking Appendix C (Tables C1 & C2).
enum class ModifyMode : std::uint8_t {
    ShowCursorOnly = 0U,
    KillAllDesignatePrimary = 1U,
    DesignatePrimaryAtCursor = 2U,
    MoveNearToSecondary = 3U,
    MoveNearOrNewSecondary = 4U,
    DesignateNearAsPrimary = 5U,
    DesignateNearOrNewPrimary = 6U,
    MoveNearOrNewPrimary = 7U,
    MoveNearOrKillAllNewPrimary = 8U,
    KillTrackNear = 9U,
    KillAllExceptPrimary = 10U,
    Reserved11 = 11U,
    DesignateNearAsSecondary = 12U,
    DesignateNearOrNewSecondary = 13U
};

/// @enum ForcedCoastingMode
/// @brief Manual coast override modes configured via ModifyTrackIndex (0x17).
/// @details Conforms to EAN-Target-Tracking Section 7.5.
enum class ForcedCoastingMode : std::uint8_t {
    None = 0U,
    FreezeUpdates = 1U,     ///< Searches frame, updates momentum; does not retrain model (2.24+)
    FreezeSearch = 2U,      ///< Skips search; maintains current velocity projection (3.00+)
    FreezePropagation = 3U  ///< Freezes search and velocity propagation entirely (3.4+)
};

/// @enum TrackIndexAction
/// @brief Operational action codes for ModifyTrackIndex (Message ID 0x17).
/// @details Conforms to EAN-Target-Tracking Section 7.5 and SLA IDD.
enum class TrackIndexAction : std::uint8_t {
    Stop = 0U,                        ///< Stop the specified track
    MakePrimary = 1U,                 ///< Designate the specified track as primary
    Reinitialize = 2U,                ///< Re-acquire and retrain model at current target position
    CoastNone = 3U,                   ///< Clear forced coast / resume normal tracking
    CoastFreezeUpdates = 4U,          ///< Forced coast: search without retraining model
    CoastFreezeSearch = 5U,           ///< Forced coast: skip search, maintain velocity projection
    CoastFreezePropagation = 6U,     ///< Forced coast: freeze velocity propagation
    ResizeNoAcquisitionAssist = 8U,   ///< Resize track box without acquisition assist
    ResizeWithAcquisitionAssist = 9U  ///< Resize track box with acquisition assist
};

/// @enum NudgeCoordinateMode
/// @brief Coordinate frame selection for NudgeTrackingCoordinate (0x0A).
/// @details Conforms to EAN-Target-Tracking Section 7.4.
enum class NudgeCoordinateMode : std::uint8_t {
    CameraCoordinates = 0U,  ///< Standard camera frame coordinates
    DisplayCoordinates = 1U  ///< Rotated display coordinates (handles inverted/gimbal displays)
};

/// @namespace TrackingFlags
/// @brief Feature bitmasks for SetTrackingParameters (0x0C).
namespace TrackingFlags {
    inline constexpr std::uint8_t AcquisitionAssist { 0x01U }; ///< Enables initial search area optimization
    inline constexpr std::uint8_t IntelligentAssist { 0x02U }; ///< Enables adaptive reinitialization
    inline constexpr std::uint8_t ColorTracking     { 0x04U }; ///< Enables color contrast tracking (3.11+)
    inline constexpr std::uint8_t ZoomScaling       { 0x08U }; ///< Scale target gate with zoom (3.6+)
    inline constexpr std::uint8_t UniqueTracks      { 0x10U }; ///< Independent per-track modes (3.11+)
    inline constexpr std::uint8_t AutoTrackMode     { 0x20U }; ///< Automatic AI classification mode (3.11+)
}

/// @enum CoordinateReportingFlags
/// @brief Reporting mode bitmask flags for tracking telemetry.
enum class CoordinateReportingFlags : std::uint8_t {
    Disabled = 0x00U,
    PrimaryTrackOnly = 0x01U,
    AllTracks = 0x02U,
    IncludePixelStats = 0x04U,
    IncludeVelocity = 0x08U
};

/// @struct TrackCoordinate
/// @brief Target tracking bounding box and centroid position in sub-pixel coordinates.
struct TrackCoordinate {
    std::uint8_t trackId { 0U };
    double centerCol { 0.0 };
    double centerRow { 0.0 };
    double width { 0.0 };
    double height { 0.0 };
    double velocityCol { 0.0 };
    double velocityRow { 0.0 };
    std::uint8_t confidence { 0U };
    bool isPrimary { false };
    bool isCoasting { false };

    /// @brief Computes normalized boresight error X [-1.0, 1.0] relative to frame width.
    /// @param[in] frameWidth Total horizontal pixel resolution (e.g. 1920.0).
    /// @return Normalized horizontal error where -1.0 is left edge, 0.0 is center, +1.0 is right edge.
    [[nodiscard]] constexpr double normalizedErrorX(double frameWidth) const noexcept
    {
        return (frameWidth > 0.0) ? ((centerCol - (frameWidth * 0.5)) / (frameWidth * 0.5)) : 0.0;
    }

    /// @brief Computes normalized boresight error Y [-1.0, 1.0] relative to frame height.
    /// @param[in] frameHeight Total vertical pixel resolution (e.g. 1080.0).
    /// @return Normalized vertical error where -1.0 is top edge, 0.0 is center, +1.0 is bottom edge.
    [[nodiscard]] constexpr double normalizedErrorY(double frameHeight) const noexcept
    {
        return (frameHeight > 0.0) ? ((centerRow - (frameHeight * 0.5)) / (frameHeight * 0.5)) : 0.0;
    }

    /// @brief Computes normalized velocity X (units / sec) given sensor width and frame rate.
    /// @param[in] frameWidth Total horizontal pixel resolution.
    /// @param[in] frameRateHz Video sensor frame rate in Hertz (e.g. 30.0).
    /// @return Normalized horizontal target velocity in normalized viewport units per second.
    [[nodiscard]] constexpr double normalizedVelocityX(double frameWidth, double frameRateHz = 30.0) const noexcept
    {
        return (frameWidth > 0.0) ? ((velocityCol / (frameWidth * 0.5)) * frameRateHz) : 0.0;
    }

    /// @brief Computes normalized velocity Y (units / sec) given sensor height and frame rate.
    /// @param[in] frameHeight Total vertical pixel resolution.
    /// @param[in] frameRateHz Video sensor frame rate in Hertz (e.g. 30.0).
    /// @return Normalized vertical target velocity in normalized viewport units per second.
    [[nodiscard]] constexpr double normalizedVelocityY(double frameHeight, double frameRateHz = 30.0) const noexcept
    {
        return (frameHeight > 0.0) ? ((velocityRow / (frameHeight * 0.5)) * frameRateHz) : 0.0;
    }

    /// @brief Computes normalized bounding height [0.0, 1.0] relative to frame height.
    /// @param[in] frameHeight Total vertical pixel resolution.
    /// @return Normalized target height ratio.
    [[nodiscard]] constexpr double normalizedHeight(double frameHeight) const noexcept
    {
        return (frameHeight > 0.0) ? (height / frameHeight) : 0.0;
    }
};

/// @struct TrackingTelemetrySnapshot
/// @brief Aggregated tracking coordinates emitted for a specific video frame.
struct TrackingTelemetrySnapshot {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t frameTimestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
};

/// @brief Maps a Sightline MessageId to a human-readable string.
/// @param[in] id MessageId enum value.
/// @return String representation of the message.
[[nodiscard]] inline constexpr std::string_view messageIdToString(MessageId id) noexcept
{
    switch (id) {
    case MessageId::GetVersionNumber:
        return "GetVersionNumber (0x00)";
    case MessageId::ResetAllParameters:
        return "ResetAllParameters (0x01)";
    case MessageId::SetStabilizationParameters:
        return "SetStabilization (0x02)";
    case MessageId::ResetStabilizationParameters:
        return "ResetStabilization (0x04)";
    case MessageId::ModifyTracking:
        return "ModifyTracking (0x05)";
    case MessageId::SetOverlayMode:
        return "SetOverlayMode (0x06)";
    case MessageId::StartTracking:
        return "StartTracking (0x08)";
    case MessageId::StopTracking:
        return "StopTracking (0x09)";
    case MessageId::NudgeTrackingCoordinate:
        return "NudgeTracking (0x0A)";
    case MessageId::CoordinateReportingMode:
        return "CoordinateReportingMode (0x0B)";
    case MessageId::SetTrackingParameters:
        return "SetTrackingParameters (0x0C)";
    case MessageId::GetTrackingParameters:
        return "GetTrackingParameters (0x0D)";
    case MessageId::SetRegistrationParameters:
        return "SetRegistration (0x0E)";
    case MessageId::GetRegistrationParameters:
        return "GetRegistration (0x0F)";
    case MessageId::SetVideoParameters:
        return "SetVideoParameters (0x10)";
    case MessageId::GetVideoParameters:
        return "GetVideoParameters (0x11)";
    case MessageId::StabilizationBias:
        return "StabilizationBias (0x12)";
    case MessageId::SetMetadataValues:
        return "SetMetadataValues (0x13)";
    case MessageId::MetadataStaticValues:
        return "MetadataStaticValues (0x14)";
    case MessageId::SetDisplayParameters:
        return "SetDisplayParameters (0x16)";
    case MessageId::ModifyTrackIndex:
        return "ModifyTrackIndex (0x17)";
    case MessageId::SetEthernetVideoParameters:
        return "SetEthernetVideo (0x1A)";
    case MessageId::GetEthernetVideoParameters:
        return "GetEthernetVideo (0x1B)";
    case MessageId::SetNetworkParameters:
        return "SetNetworkParameters (0x1C)";
    case MessageId::GetNetworkParameters:
        return "GetNetworkParameters (0x1D)";
    case MessageId::SetSDRecordingParameters:
        return "SetSDRecording (0x1E)";
    case MessageId::SetVideoMode:
        return "SetVideoMode (0x1F)";
    case MessageId::GetVideoMode:
        return "GetVideoMode (0x20)";
    case MessageId::SetVideoEnhancementParameters:
        return "SetVideoEnhancement (0x21)";
    case MessageId::GetVideoEnhancementParameters:
        return "GetVideoEnhancement (0x22)";
    case MessageId::SetH264Parameters:
        return "SetH264Parameters (0x23)";
    case MessageId::GetH264Parameters:
        return "GetH264Parameters (0x24)";
    case MessageId::SaveParameters:
        return "SaveParameters (0x25)";
    case MessageId::GetParameters:
        return "GetParameters (0x28)";
    case MessageId::SetDetectionParameters:
        return "SetDetectionParameters (0x2D)";
    case MessageId::GetDetectionParameters:
        return "GetDetectionParameters (0x2E)";
    case MessageId::SetBlendParameters:
        return "SetBlendParameters (0x2F)";
    case MessageId::GetBlendParameters:
        return "GetBlendParameters (0x30)";
    case MessageId::DesignateSelectedTrackPrimary:
        return "DesignatePrimary (0x32)";
    case MessageId::ShiftSelectedTrack:
        return "ShiftSelectedTrack (0x33)";
    case MessageId::DrawObject:
        return "DrawObject (0x3B)";
    case MessageId::StopSelectedTrack:
        return "StopSelectedTrack (0x3C)";
    case MessageId::CommandPassThrough:
        return "CommandPassThrough (0x3D)";
    case MessageId::SetPortConfiguration:
        return "SetPortConfiguration (0x3E)";
    case MessageId::GetPortConfiguration:
        return "GetPortConfiguration (0x3F)";
    case MessageId::VersionNumber:
        return "VersionNumber (0x40)";
    case MessageId::CurrentStabilizationParameters:
        return "CurrentStabilization (0x41)";
    case MessageId::TrackingPosition:
        return "TrackingPosition (0x43)";
    case MessageId::CurrentVideoParameters:
        return "CurrentVideoParameters (0x46)";
    case MessageId::CurrentNetworkParameters:
        return "CurrentNetworkParameters (0x49)";
    case MessageId::TrackingPositions:
        return "TrackingPositions (0x51)";
    case MessageId::FocusStats:
        return "FocusStats (0x55)";
    case MessageId::CurrentH264Parameters:
        return "CurrentH264Parameters (0x56)";
    case MessageId::SetMetadataRate:
        return "SetMetadataRate (0x62)";
    case MessageId::SetTelemetryDestination:
        return "SetTelemetryDestination (0x64)";
    case MessageId::SetLensParameters:
        return "SetLensParameters (0x6E)";
    case MessageId::UserWarningMessage:
        return "UserWarningMessage (0x86)";
    case MessageId::SystemStatusMessage:
        return "SystemStatusMessage (0x87)";
    case MessageId::CurrentMetadataValues:
        return "CurrentMetadataValues (0x8B)";
    case MessageId::CurrentConfiguration:
        return "CurrentConfiguration (0x8E)";
    case MessageId::StreamingControl:
        return "StreamingControl (0x90)";
    case MessageId::DrawOverlay:
        return "DrawOverlay (0x9C)";
    case MessageId::TrackTrails:
        return "TrackTrails (0x9D)";
    case MessageId::TrackingPositionsExtended:
        return "TrackingPositionsExtended (0xA0)";
    case MessageId::Noise3D:
        return "Noise3D (0xAF)";
    case MessageId::CursorOnTarget:
        return "CursorOnTarget (0xB0)";
    case MessageId::LensCommand:
        return "LensCommand (0xB2)";
    case MessageId::FocusParameters:
        return "FocusParameters (0xB3)";
    case MessageId::GPIO:
        return "GPIO (0xB6)";
    case MessageId::CustomAIDetect:
        return "CustomAIDetect (0xBA)";
    case MessageId::CommandAck:
        return "CommandAck (0xC3)";
    case MessageId::SetFileRecordingParamsV2:
        return "SetFileRecordingParamsV2 (0xC4)";
    case MessageId::DoSnapShotV2:
        return "DoSnapShotV2 (0xC5)";
    case MessageId::FileRecordingEvent:
        return "FileRecordingEvent (0xC6)";
    case MessageId::CurrentRecordingStatusV2:
        return "CurrentRecordingStatusV2 (0xC7)";
    case MessageId::GetDirectoryListing:
        return "GetDirectoryListing (0xC8)";
    case MessageId::DirectoryListingReply:
        return "DirectoryListingReply (0xC9)";
    case MessageId::FileStorageManagement:
        return "FileStorageManagement (0xCA)";
    default:
        return "Unknown Message";
    }
}

} // namespace Sightline
