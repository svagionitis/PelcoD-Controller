#pragma once

/// @file SightlineProtocolParser.h
/// @brief Parser deserializing raw Sightline SLA frames into typed POD structures.

#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Sightline {

/// @class SightlineProtocolParser
/// @brief Zero-copy inspection and deserialization of framed SLA packets.
class SightlineProtocolParser {
public:
    /// @brief Identifies the SLA Message ID from a framed binary packet.
    /// @details Extracts the Message ID byte directly following the framing header bytes.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @return Extracted MessageId enum or MessageId::Unknown.
    [[nodiscard]] static MessageId identifyMessage(ByteView packet) noexcept;

    /// @brief Extracts payload bytes excluding headers, ID, and checksum without heap allocation.
    /// @details Returns a ByteView slicing the underlying packet buffer between Message ID and CRC.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @return ByteView of payload contents.
    [[nodiscard]] static ByteView extractPayload(ByteView packet) noexcept;

    // --- System & Telemetry Deserializers ---

    /// @brief Parses system version information (Message ID 0x40 / 0x00).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized version structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVersionNumber(const std::vector<std::uint8_t>& packet, MsgVersionNumber& out);

    /// @brief Parses unsolicited diagnostic warning notification (Message ID 0x86).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized warning structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseUserWarning(const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out);

    /// @brief Parses system health status, CPU load and core temp (Message ID 0x87).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized system status structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseSystemStatus(const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out);

    /// @brief Parses hardware input/output configuration (Message ID 0x8E).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized current configuration structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCurrentConfiguration(
        const std::vector<std::uint8_t>& packet, MsgCurrentConfiguration& out);

    // --- Tracking Deserializers ---

    /// @brief Parses single primary track coordinate and scene motion (Message ID 0x43).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized tracking position structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingPosition(const std::vector<std::uint8_t>& packet, MsgTrackingPosition& out);

    /// @brief Parses multi-target position and velocity telemetry (Message ID 0x51).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized tracking positions structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingPositions(
        const std::vector<std::uint8_t>& packet, MsgTrackingPositions& out);

    /// @brief Parses multi-target report with AI classifier labels (Message ID 0xA0).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized extended tracking positions structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parsePositionsExtended(
        const std::vector<std::uint8_t>& packet, MsgTrackingPositionsExtended& out);

    /// @brief Parses target history trails (Message ID 0x9D).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized track trails structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackTrails(const std::vector<std::uint8_t>& packet, MsgTrackTrails& out);

    // --- Configuration State Deserializers ---

    /// @brief Parses active stabilization settings (Message ID 0x41).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized stabilization parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseStabilizationParams(
        const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out);

    /// @brief Parses active stabilization bias settings (Message ID 0x9F / 0x7A).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized stabilization bias.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseStabilizationBias(
        const std::vector<std::uint8_t>& packet, MsgSetStabilizationBias& out);

    /// @brief Parses active frame registration parameters (Message ID 0x9E / 0x45 / 0x0E).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized registration parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseRegistration(
        const std::vector<std::uint8_t>& packet, MsgSetRegistrationParameters& out);

    /// @brief Parses active video capture parameters (Message ID 0x46).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized video parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoParameters(const std::vector<std::uint8_t>& packet, MsgSetVideoParameters& out);

    /// @brief Parses active video enhancement parameters (Message ID 0x4A / 0x21).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized enhancement structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoEnhance(const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancement& out);

    /// @brief Parses complete SLA video enhancement parameters (Message ID 0x4A / 0x21).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized full enhancement structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoEnhanceFull(
        const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancementFull& out);

    /// @brief Parses active H.264 compression parameters (Message ID 0x56).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized H.264 parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseH264Parameters(const std::vector<std::uint8_t>& packet, MsgSetH264Parameters& out);

    /// @brief Parses active auto-focus statistics (Message ID 0x55).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized focus parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseFocusStats(const std::vector<std::uint8_t>& packet, MsgFocusParameters& out);

    /// @brief Parses active platform telemetry metadata (Message ID 0x8B).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized metadata values.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseMetadataValues(const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out);

    /// @brief Parses tracking algorithm parameters (Message ID 0x44 / 0x0C).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized tracking parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingParameters(
        const std::vector<std::uint8_t>& packet, MsgSetTrackingParameters& out);

    /// @brief Parses lens optical calibration parameters (Message ID 0x6E / 0x6F / 0xB1).
    [[nodiscard]] static bool parseLensParameters(const std::vector<std::uint8_t>& packet, MsgSetLensParameters& out);

    /// @brief Parses system status mode settings (Message ID 0x80).
    [[nodiscard]] static bool parseSystemStatusMode(const std::vector<std::uint8_t>& packet, MsgSystemStatusMode& out);

    /// @brief Parses static metadata values (Message ID 0x14).
    [[nodiscard]] static bool parseMetadataStaticValues(
        const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out);

    /// @brief Parses metadata rate reply (Message ID 0x62 / 0x8D).
    [[nodiscard]] static bool parseMetadataRate(const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out);

    /// @brief Parses current metadata rate query reply (Message ID 0x8D).
    [[nodiscard]] static bool parseCurrentRate(const std::vector<std::uint8_t>& packet, MsgCurrentMetadataRate& out);

    /// @brief Parses current metadata values (Message ID 0x8B).
    [[nodiscard]] static bool parseCurrentValues(
        const std::vector<std::uint8_t>& packet, MsgCurrentMetadataValues& out);

    /// @brief Parses frame metadata values (Message ID 0x15).
    [[nodiscard]] static bool parseFrameValues(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataFrameValues& out);

    /// @brief Parses current frame metadata values (Message ID 0x8C).
    [[nodiscard]] static bool parseCurrentFrameValues(
        const std::vector<std::uint8_t>& packet, MsgCurrentMetadataFrameValues& out);

    /// @brief Parses TagData message (Message ID 0x96).
    [[nodiscard]] static bool parseTagData(const std::vector<std::uint8_t>& packet, MsgTagData& out);

    /// @brief Parses TagDataRate message (Message ID 0x97).
    [[nodiscard]] static bool parseTagDataRate(const std::vector<std::uint8_t>& packet, MsgTagDataRate& out);

    /// @brief Parses TagSourceSelector message (Message ID 0x98).
    [[nodiscard]] static bool parseTagSourceSelector(
        const std::vector<std::uint8_t>& packet, MsgTagSourceSelector& out);

    /// @brief Parses Cursor-on-Target XML configuration (Message ID 0xB0).
    [[nodiscard]] static bool parseCursorOnTarget(
        const std::vector<std::uint8_t>& packet, MsgCursorOnTarget& out);

    /// @brief Parses VMTI chips configuration (Message ID 0xAD).
    [[nodiscard]] static bool parseVmtiChips(const std::vector<std::uint8_t>& packet, MsgVmtiChips& out);

    /// @brief Parses VMTI fields configuration (Message ID 0xBF).
    [[nodiscard]] static bool parseVmtiFields(const std::vector<std::uint8_t>& packet, MsgVmtiFields& out);

    /// @brief Parses dynamic ancillary text metadata insertion (Message ID 0xAC).
    [[nodiscard]] static bool parseAncillaryText(
        const std::vector<std::uint8_t>& packet, MsgAncillaryTextMetadata& out);

    /// @brief Parses Ethernet display streaming parameters (Message ID 0x29 / 0x52).
    [[nodiscard]] static bool parseEthernetDisplay(
        const std::vector<std::uint8_t>& packet, MsgSetEthernetDisplayParameters& out);

    /// @brief Parses Ethernet video parameters (Message ID 0x1A / 0x48).
    [[nodiscard]] static bool parseEthernetVideo(
        const std::vector<std::uint8_t>& packet, MsgSetEthernetVideoParameters& out);

    /// @brief Parses network parameters (Message ID 0x1C / 0x49).
    [[nodiscard]] static bool parseNetworkParameters(
        const std::vector<std::uint8_t>& packet, MsgSetNetworkParameters& out);

    /// @brief Parses network interfaces list (Message ID 0x67).
    [[nodiscard]] static bool parseNetworkList(const std::vector<std::uint8_t>& packet, MsgCurrentNetworkList& out);

    /// @brief Parses snapshot status and path (Message ID 0x5D / 0x5F).
    [[nodiscard]] static bool parseSnapShot(const std::vector<std::uint8_t>& packet, MsgCurrentSnapShot& out);

    /// @brief Parses command acknowledgment message (Message ID 0xC3).
    [[nodiscard]] static bool parseCmdAck(const std::vector<std::uint8_t>& packet, MsgCommandAck& out);

    /// @brief Parses hardened video recording parameters (Message ID 0xC4).
    [[nodiscard]] static bool parseSetFileRecordingV2(const std::vector<std::uint8_t>& packet, MsgSetFileRecordingParamsV2& out);

    /// @brief Parses hardened snapshot request (Message ID 0xC5).
    [[nodiscard]] static bool parseDoSnapShotV2(const std::vector<std::uint8_t>& packet, MsgDoSnapShotV2& out);

    /// @brief Parses push-based file recording event notification (Message ID 0xC6).
    [[nodiscard]] static bool parseRecordingEvent(const std::vector<std::uint8_t>& packet, MsgFileRecordingEvent& out);

    /// @brief Parses comprehensive recording health telemetry (Message ID 0xC7).
    [[nodiscard]] static bool parseRecordingStatusV2(const std::vector<std::uint8_t>& packet, MsgCurrentRecordingStatusV2& out);

    /// @brief Parses directory catalog query (Message ID 0xC8).
    [[nodiscard]] static bool parseGetDirListing(const std::vector<std::uint8_t>& packet, MsgGetDirectoryListing& out);

    /// @brief Parses directory catalog response batch (Message ID 0xC9).
    [[nodiscard]] static bool parseDirListingReply(const std::vector<std::uint8_t>& packet, MsgDirectoryListingReply& out);

    /// @brief Parses in-band file storage management command (Message ID 0xCA).
    [[nodiscard]] static bool parseFileStorageMgmt(const std::vector<std::uint8_t>& packet, MsgFileStorageManagement& out);

    // --- Thermal NUC & Sensor Calibration Deserializers ---

    /// @brief Parses NUC calibration parameters (Message ID 0x35).
    [[nodiscard]] static bool parseNucParameters(ByteView packet, MsgNucParameters& out);

    /// @brief Parses dead pixel replacement configuration (Message ID 0xA8).
    [[nodiscard]] static bool parseDeadPixel(ByteView packet, MsgDeadPixel& out);

    /// @brief Parses NUC read/write flash response (Message ID 0x36).
    [[nodiscard]] static bool parseReadWriteNuc(ByteView packet, MsgReadWriteNuc& out);

    /// @brief Parses custom thermal pseudo-color palette (Message ID 0x72 / 0x73).
    [[nodiscard]] static bool parseUserPalette(ByteView packet, MsgUserPalette& out);

    /// @brief Parses dead pixel metrics and defect statistics (Message ID 0xA1).
    [[nodiscard]] static bool parseDeadPixelStats(ByteView packet, MsgDeadPixelStats& out);

    /// @brief Parses camera intrinsic geometric calibration (Message ID 0xC0).
    [[nodiscard]] static bool parseCameraCalibration(ByteView packet, MsgCameraCalibration& out);

    /// @brief Parses camera parameter file status or reply (Message ID 0xC2).
    [[nodiscard]] static bool parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out);

    // --- VMTI & Tactical Video Analytics Deserializers (Phase 2) ---

    /// @brief Parses active detection parameters (Message ID 0x54 / 0x2D).
    [[nodiscard]] static bool parseDetectionParams(ByteView packet, MsgSetDetectionParameters& out);

    /// @brief Parses Video Moving Target Indication (VMTI) parameters (Message ID 0x84).
    [[nodiscard]] static bool parseVMTI(ByteView packet, MsgSetVMTI& out);

    /// @brief Parses detection region of interest (ROI) parameters (Message ID 0x7C / 0x7D).
    [[nodiscard]] static bool parseDetectionROI(ByteView packet, MsgDetectionROI& out);

    /// @brief Parses advanced detection parameters (Message ID 0x76 / 0x77).
    [[nodiscard]] static bool parseAdvDetectionParams(ByteView packet, MsgAdvancedDetectionParameters& out);

    /// @brief Parses tracking gate luminance pixel statistics (Message ID 0x78).
    [[nodiscard]] static bool parseTrackingPixelStats(ByteView packet, MsgTrackingBoxPixelStats& out);

    /// @brief Parses AI detection configuration (Message ID 0xBA).
    [[nodiscard]] static bool parseCustomAIDetect(ByteView packet, MsgCustomAIDetect& out);

    /// @brief Parses target thumbnail chip image payload (Message ID 0xAD).
    [[nodiscard]] static bool parseVMTIChips(ByteView packet, MsgVMTIChips& out);

    /// @brief Parses MISB ST 0903 VMTI field insertion mask (Message ID 0xBF).
    [[nodiscard]] static bool parseVMTIFields(ByteView packet, MsgVMTIFields& out);

    /// @brief Parses STANAG 4609 KLV target class filtering rules (Message ID 0xC1).
    [[nodiscard]] static bool parseKlvClassFilters(ByteView packet, MsgKlvClassFilters& out);

    /// @brief Parses multi-class deep learning categorization report (Message ID 0xBD).
    [[nodiscard]] static bool parseTrackingMultiClass(ByteView packet, MsgTrackingMultiClass& out);

    /// @brief Parses custom neural network classifier pipeline configuration (Message ID 0xA7).
    [[nodiscard]] static bool parseCustomClassifier(ByteView packet, MsgCustomClassifier& out);

    /// @brief Parses classifier execution bounds and threshold parameters (Message ID 0xA9).
    [[nodiscard]] static bool parseClassifierParams(ByteView packet, MsgClassifierParameters& out);

    /// @brief Parses KLV spatial and metric dimension filtering rules (Message ID 0xC1).
    [[nodiscard]] static bool parseKlvMetricFilters(ByteView packet, MsgKlvMetricFilters& out);

    /// @brief Parses classifier configuration and compute settings (Message ID 0xA9).
    [[nodiscard]] static bool parseClassifierConfig(ByteView packet, MsgClassifierConfig& out);

    // --- Multi-Sensor Alignment & Digital Video Pipeline (Phase 3) ---

    /// @brief Parses active blending parameters (Message ID 0x4D / 0x2F).
    [[nodiscard]] static bool parseBlendParameters(ByteView packet, MsgSetBlendParameters& out);

    /// @brief Parses 4-point projective homography points (Message ID 0x95).
    [[nodiscard]] static bool parseFourAlignPoints(ByteView packet, MsgFourAlignPoints& out);

    /// @brief Parses blend alignment offsets and registration mode (Message ID 0xB9).
    [[nodiscard]] static bool parseBlendAlign(ByteView packet, MsgBlendAlign& out);

    /// @brief Parses camera switch command / status (Message ID 0x82).
    [[nodiscard]] static bool parseCameraSwitch(ByteView packet, MsgCameraSwitch& out);

    /// @brief Parses advanced capture deserializer hardware parameters (Message ID 0x7B).
    [[nodiscard]] static bool parseAdvCaptureParams(ByteView packet, MsgAdvancedCaptureParameters& out);

    /// @brief Parses digital video framing decoder parameters (Message ID 0x91).
    [[nodiscard]] static bool parseDigiVideoParser(ByteView packet, MsgDigitalVideoParserParameters& out);

    /// @brief Parses camera hardware capabilities and limits (Message ID 0xBB).
    [[nodiscard]] static bool parseCameraCapabilities(ByteView packet, MsgCameraCapabilities& out);

    /// @brief Parses multi-channel video display routing and aspect ratio (Message ID 0xA4).
    [[nodiscard]] static bool parseVideoDisplay(ByteView packet, MsgVideoDisplay& out);

    /// @brief Parses multi-display split screen and PiP window routing (Message ID 0xA5).
    [[nodiscard]] static bool parseMultiDisplay(ByteView packet, MsgMultiDisplay& out);

    // --- Low-Level Bus, I2C, & Telemetry Tags (Phase 4) ---

    /// @brief Parses master I2C response / telemetry packet (Message ID 0x94).
    [[nodiscard]] static bool parseI2CCommand(ByteView packet, MsgI2CCommand& out);

    /// @brief Parses system value register reply (Message ID 0x92 / 0x93).
    [[nodiscard]] static bool parseSystemValue(ByteView packet, MsgSystemValue& out);

    /// @brief Parses custom binary tag data frame (Message ID 0x96).
    [[nodiscard]] static bool parseTagData(ByteView packet, MsgTagData& out);

    /// @brief Parses tag data rate reply (Message ID 0x97).
    [[nodiscard]] static bool parseTagDataRate(ByteView packet, MsgTagDataRate& out);

    /// @brief Parses tag source selector reply (Message ID 0x98).
    [[nodiscard]] static bool parseTagSourceSelector(ByteView packet, MsgTagSourceSelector& out);

    /// @brief Parses detailed latency timing telemetry (Message ID 0x88).
    [[nodiscard]] static bool parseDetailedTiming(ByteView packet, MsgDetailedTiming& out);

    /// @brief Parses appended metadata configuration reply (Message ID 0x89).
    [[nodiscard]] static bool parseAppendedMetadata(ByteView packet, MsgAppendedMetadata& out);

    /// @brief Parses frame index and timestamp telemetry (Message ID 0x8A).
    [[nodiscard]] static bool parseFrameIndex(ByteView packet, MsgFrameIndex& out);

    // --- Autonomous Landing Aid & Graphics (Phase 5) ---

    /// @brief Parses landing aid configuration parameters (Message ID 0x81).
    [[nodiscard]] static bool parseLandingAid(ByteView packet, MsgLandingAid& out);

    /// @brief Parses landing target relative coordinates and orientation (Message ID 0x83).
    [[nodiscard]] static bool parseLandingPosition(ByteView packet, MsgLandingPosition& out);

    /// @brief Parses current overlay mode reply (Message ID 0x42 / 0x06).
    [[nodiscard]] static bool parseOverlayMode(ByteView packet, MsgSetOverlayMode& out);

    /// @brief Parses single custom graphic object command (Message ID 0x3B).
    [[nodiscard]] static bool parseDrawObject(ByteView packet, MsgDrawObject& out);

    /// @brief Parses batch overlay graphic primitives (Message ID 0x9C).
    [[nodiscard]] static bool parseDrawOverlay(ByteView packet, MsgDrawOverlay& out);

    /// @brief Parses logo watermark parameters reply (Message ID 0x9B).
    [[nodiscard]] static bool parseLogoParameters(ByteView packet, MsgLogoParameters& out);

    /// @brief Parses dynamic ancillary text metadata overlay (Message ID 0xAC).
    [[nodiscard]] static bool parseAncillaryTextMetadata(ByteView packet, MsgAncillaryTextMetadata& out);

    /// @brief Parses TrueType user font assignment reply (Message ID 0xAE).
    [[nodiscard]] static bool parseUserFont(ByteView packet, MsgUserFont& out);

    /// @brief Parses active overlay object IDs bitmask reply (Message ID 0x68).
    [[nodiscard]] static bool parseOverlayObjectsIds(ByteView packet, MsgCurrentOverlayObjectsIds& out);

    /// @brief Parses graphic overlay object parameters reply (Message ID 0x6B).
    [[nodiscard]] static bool parseOverlayObjectParams(ByteView packet, MsgCurrentOverlayObjectParameters& out);

    /// @brief Parses hardware video decoder parameters (Message ID 0x99).
    [[nodiscard]] static bool parseDecoderParameters(ByteView packet, MsgDecoderParameters& out);

    /// @brief Parses transparent packet forwarding to BTS / RF modem (Message ID 0xBE).
    [[nodiscard]] static bool parseSendToBTS(ByteView packet, MsgSendToBTS& out);

private:
    [[nodiscard]] static std::size_t getHeaderLength(ByteView packet) noexcept;
};

} // namespace Sightline
