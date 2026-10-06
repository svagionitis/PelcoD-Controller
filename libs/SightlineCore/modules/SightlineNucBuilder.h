#pragma once

/// @file SightlineNucBuilder.h
/// @brief Serializer for Sightline non-uniformity correction (NUC) and dead pixel replacement commands.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"
#include "SightlineNucCaps.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNucBuilder
/// @brief Encodes NUC / DPR commands per Sightline IDD v3.11.
/// @details All encoders are stateless and thread-safe. Encoders that can receive structurally
///          invalid input return an empty vector instead of a malformed packet; callers must check
///          `empty()` before transmitting.
class SightlineNucBuilder {
public:
    /// @brief Encodes NUC/DPR parameters and calibration action (Message ID 0x35).
    /// @details Always serialises the full IDD v3.11 layout (35 fixed bytes + nucName).
    /// @param[in] msg NUC parameters.
    /// @return Framed binary packet.
    /// @retval empty `msg.nucName` exceeds 255 characters.
    [[nodiscard]] static std::vector<std::uint8_t> buildNucParameters(const MsgNucParameters& msg);

    /// @brief Encodes 0x35 truncated after the field group selected by @p tail.
    /// @details Firmware accepts shorter payloads (EAN Appendix A4 sends 28 bytes). Use the
    ///          shortest tail that carries the fields you intend to change, capped by
    ///          SightlineNucCaps::maxNucTail(). Fields beyond the tail keep their board value.
    /// @param[in] msg NUC parameters.
    /// @param[in] tail Last field group to serialise.
    /// @return Framed binary packet (28, 33, 35, or 36 + name bytes of payload).
    /// @retval empty `msg.nucName` is non-empty but @p tail is not NucTail::Named (it would be
    ///         silently dropped), or the name exceeds 255 characters.
    [[nodiscard]] static std::vector<std::uint8_t> buildNucParameters(
        const MsgNucParameters& msg, NucTail tail);

    /// @brief Validates a 0x35 command against the IDD ranges and firmware capabilities.
    /// @details Checks, in order: run mode support, Calc2Point numFrames, DPR fields only with
    ///          CalcDead (and their ranges), numReplace, deadFilterThresh, and nucName rules
    ///          (FW 3.10+, fewer than 64 characters, [A-Za-z0-9_-], EAN section 4.4).
    /// @param[in] msg NUC parameters.
    /// @param[in] fw Reported firmware version (0.0 if unknown).
    /// @return NucError::Ok or the first violation found.
    [[nodiscard]] static NucError checkNucParams(const MsgNucParameters& msg, FwVersion fw);

    /// @brief Validates a 0x36 command against the IDD rules and firmware capabilities.
    /// @details fileName is required unless the command is a pure ClearNuc / ClearDead.
    ///          secondaryFileName is required for LoadInterpolated / LoadShutterFlatten and must
    ///          be empty otherwise; interpolationRatio must be 0 unless LoadInterpolated.
    /// @param[in] msg Read/write NUC command.
    /// @param[in] fw Reported firmware version (0.0 if unknown).
    /// @return NucError::Ok or the first violation found.
    [[nodiscard]] static NucError checkReadWriteNuc(const MsgReadWriteNuc& msg, FwVersion fw);

    /// @brief Validates a 0xA8 command against the IDD rules and firmware capabilities.
    /// @details Requires FW 3.3; `c` must be 0 or 1 for Add / Remove; `d` is reserved (0).
    /// @param[in] msg Dead pixel command.
    /// @param[in] fw Reported firmware version (0.0 if unknown).
    /// @return NucError::Ok or the first violation found.
    [[nodiscard]] static NucError checkDeadPixel(const MsgDeadPixel& msg, FwVersion fw);

    /// @brief Encodes a raw dead pixel list operation (Message ID 0xA8).
    /// @details Prefer buildAddDeadPixel / buildRemoveDeadPixel / buildDynamicDead.
    /// @param[in] msg Dead pixel operation.
    /// @return Framed binary packet (8-byte payload).
    [[nodiscard]] static std::vector<std::uint8_t> buildDeadPixel(const MsgDeadPixel& msg);

    /// @brief Encodes "add pixel to dead list" (Message ID 0xA8, mode 0).
    /// @details See EAN-NUC-and-DPR Appendix A1. When sending a list, set @p deferUpdate on every
    ///          pixel except the last so the dead map is rebuilt once.
    /// @param[in] cameraIndex Camera index.
    /// @param[in] column Pixel column (x), 0 = left.
    /// @param[in] row Pixel row (y), 0 = top.
    /// @param[in] deferUpdate True if more pixels follow (c = 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildAddDeadPixel(
        std::uint8_t cameraIndex, std::uint16_t column, std::uint16_t row, bool deferUpdate);

    /// @brief Encodes "remove pixel from dead list" (Message ID 0xA8, mode 1).
    /// @details Only remove pixels that were added manually (EAN-NUC-and-DPR section 5.3).
    /// @param[in] cameraIndex Camera index.
    /// @param[in] column Pixel column (x).
    /// @param[in] row Pixel row (y).
    /// @param[in] deferUpdate True if more pixels follow (c = 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildRemoveDeadPixel(
        std::uint8_t cameraIndex, std::uint16_t column, std::uint16_t row, bool deferUpdate);

    /// @brief Encodes dynamic dead pixel detection (Message ID 0xA8, mode 2).
    /// @param[in] cameraIndex Camera index.
    /// @param[in] kernelSize Neighbourhood width/height; 0 selects the firmware default (15).
    /// @param[in] maxPixelDiff Dead threshold; 0 selects an automatic per-neighbourhood value.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDynamicDead(
        std::uint8_t cameraIndex, std::uint8_t kernelSize, std::uint8_t maxPixelDiff);

    /// @brief Encodes query for active NUC parameters (Message ID 0x28 query 0x35).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNucParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes NUC / dead table save, load, interpolate, and default command (Message ID 0x36).
    /// @details mode byte = (defaultOp << 4) | fileOp. secondaryFileName and interpolationRatio are
    ///          always appended (empty / 0 when unused).
    /// @param[in] msg Read/write NUC parameters.
    /// @return Framed binary packet.
    /// @retval empty fileName is blank and the command is not ClearNuc / ClearDead with
    ///         fileOp None (IDD: blank names only accepted for mode 0x30 / 0x40), or a name
    ///         exceeds 255 characters.
    [[nodiscard]] static std::vector<std::uint8_t> buildReadWriteNuc(const MsgReadWriteNuc& msg);

    /// @brief Encodes query for loaded / default table names (Message ID 0x28 query 0x36).
    /// @param[in] query Table selector (payload0).
    /// @param[in] cameraIndex Target camera index (payload1).
    /// @return Framed binary packet; the reply is a MsgReadWriteNuc (0x36).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetReadWriteNuc(
        NucTableQuery query, std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for dead pixel detection statistics (Message ID 0x28 query 0xA1).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet; the reply is a MsgDeadPixelStats (0xA1).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDeadPixelStats(std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
