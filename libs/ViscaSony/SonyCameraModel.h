#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Visca::Sony {

/// @brief Identification of supported Sony camera models.
enum class SonyCameraModelType : uint8_t {
    Unknown, ///< Unrecognized device
    GenericSony, ///< Generic Sony VISCA camera
    FCB_EV9520L, ///< Sony FCB-EV9520L (1/2.8" STARVIS 2 FHD, LVDS)
    FCB_EW9500H ///< Sony FCB-EW9500H (1/1.8" STARVIS 4K, HDMI/TMDS)
};

/// @struct CameraCapabilities
/// @brief Static capabilities and hardware properties of a Sony camera model.
struct CameraCapabilities {
    SonyCameraModelType modelType { SonyCameraModelType::Unknown }; ///< Model classification
    std::string modelName { "Unknown" }; ///< Human readable model name
    std::string sensorDescription { "" }; ///< Sensor technology and format
    bool supports4K { false }; ///< Supports 4K UHD video resolutions (2160p)
    bool supportsDistortionCompensation { false }; ///< Supports register 0x57 distortion comp
    bool supportsOpticalAxisGapCompensation { false }; ///< Supports register 0x47 optical axis gap comp
    bool supportsLvdsMode { false }; ///< Uses LVDS serial interface (register 0x74)
    bool supportsTmdsOutput { false }; ///< Uses TMDS digital video output (register 0x60)
    double minFocalLengthMm { 0.0 }; ///< Wide end focal length in mm
    double maxFocalLengthMm { 0.0 }; ///< Optical tele end focal length in mm
    double maxOpticalZoomRatio { 30.0 }; ///< Maximum optical zoom multiplier (e.g. 30x)
    double maxDigitalZoomRatio { 12.0 }; ///< Maximum digital zoom multiplier (e.g. 12x)
    uint16_t minWorkingDistanceWideMm { 10 }; ///< Minimum working distance at wide end in mm
    uint16_t minWorkingDistanceTeleMm { 1200 }; ///< Minimum working distance at tele end in mm

    /// @brief Calculates optical focal length in mm for a given raw optical zoom position.
    /// @param[in] zoomPos Raw zoom position (0x0000 wide to 0x4000 tele).
    /// @return Focal length in millimeters.
    [[nodiscard]] double calculateFocalLength(uint16_t zoomPos) const noexcept
    {
        const double clamped = static_cast<double>(zoomPos > 0x4000 ? 0x4000 : zoomPos);
        const double ratio = clamped / static_cast<double>(0x4000);
        return minFocalLengthMm + ratio * (maxFocalLengthMm - minFocalLengthMm);
    }
};

/// @class SonyCameraModel
/// @brief Model descriptor and capability query helper for Sony FCB cameras.
class SonyCameraModel {
public:
    /// @brief Vendor ID for Sony Corporation devices.
    static constexpr uint16_t kSonyVendorId { 0x0020 };

    /// @brief Model ID for Sony FCB-EV9520L.
    static constexpr uint16_t kModelIdEV9520L { 0x0711 };

    /// @brief Model ID for Sony FCB-EW9500H.
    static constexpr uint16_t kModelIdEW9500H { 0x070F };

    /// @brief Identifies model type from vendor and model IDs.
    /// @param[in] vendorId 16-bit Vendor ID.
    /// @param[in] modelId 16-bit Model ID.
    /// @return The identified @ref SonyCameraModelType.
    [[nodiscard]] static SonyCameraModelType identify(uint16_t vendorId, uint16_t modelId) noexcept
    {
        if (vendorId == kSonyVendorId) {
            if (modelId == kModelIdEV9520L) {
                return SonyCameraModelType::FCB_EV9520L;
            }
            if (modelId == kModelIdEW9500H) {
                return SonyCameraModelType::FCB_EW9500H;
            }
            return SonyCameraModelType::GenericSony;
        }
        return SonyCameraModelType::Unknown;
    }

    /// @brief Retrieves the capabilities definition for a specific model type.
    /// @param[in] type The camera model enum.
    /// @return Populated @ref CameraCapabilities structure.
    [[nodiscard]] static CameraCapabilities getCapabilities(SonyCameraModelType type)
    {
        CameraCapabilities caps;
        caps.modelType = type;

        switch (type) {
        case SonyCameraModelType::FCB_EV9520L:
            caps.modelName = "Sony FCB-EV9520L";
            caps.sensorDescription = "1/2.8 STARVIS 2 CMOS (2.13 MP, Full HD)";
            caps.supports4K = false;
            caps.supportsDistortionCompensation = true;
            caps.supportsOpticalAxisGapCompensation = true;
            caps.supportsLvdsMode = true;
            caps.supportsTmdsOutput = false;
            caps.minFocalLengthMm = 4.3;
            caps.maxFocalLengthMm = 129.0;
            caps.maxOpticalZoomRatio = 30.0;
            caps.maxDigitalZoomRatio = 12.0;
            caps.minWorkingDistanceWideMm = 10;
            caps.minWorkingDistanceTeleMm = 1200;
            break;

        case SonyCameraModelType::FCB_EW9500H:
            caps.modelName = "Sony FCB-EW9500H";
            caps.sensorDescription = "1/1.8 STARVIS CMOS (4.17 MP, 4K UHD)";
            caps.supports4K = true;
            caps.supportsDistortionCompensation = false;
            caps.supportsOpticalAxisGapCompensation = false;
            caps.supportsLvdsMode = false;
            caps.supportsTmdsOutput = true;
            caps.minFocalLengthMm = 6.5;
            caps.maxFocalLengthMm = 162.5;
            caps.maxOpticalZoomRatio = 30.0;
            caps.maxDigitalZoomRatio = 12.0;
            caps.minWorkingDistanceWideMm = 100;
            caps.minWorkingDistanceTeleMm = 1200;
            break;

        case SonyCameraModelType::GenericSony:
        case SonyCameraModelType::Unknown:
        default:
            caps.modelName = "Generic Sony Camera";
            caps.sensorDescription = "Standard Sony VISCA Device";
            caps.supports4K = false;
            caps.supportsDistortionCompensation = false;
            caps.supportsOpticalAxisGapCompensation = false;
            caps.supportsLvdsMode = false;
            caps.supportsTmdsOutput = false;
            caps.minFocalLengthMm = 4.0;
            caps.maxFocalLengthMm = 120.0;
            caps.maxOpticalZoomRatio = 30.0;
            caps.maxDigitalZoomRatio = 12.0;
            caps.minWorkingDistanceWideMm = 100;
            caps.minWorkingDistanceTeleMm = 1200;
            break;
        }

        return caps;
    }
};

} // namespace Visca::Sony
