#include "GeoRegistrationParser.h"
#include "KlvBer.h"
#include "MdArray.h"

#include <algorithm>
#include <cstring>

namespace Klv {

bool GeoRegistrationParser::isGeoRegistration(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < GeoRegistrationUl.size()) {
        return false;
    }
    return std::equal(GeoRegistrationUl.begin(), GeoRegistrationUl.end(), data);
}

KlvStatus GeoRegistrationParser::parsePacket(const std::uint8_t* data,
                                             std::size_t size,
                                             GeoRegistrationLocalSet& outSet) noexcept {
    if (!isGeoRegistration(data, size)) {
        return KlvStatus::InvalidUniversalLabel;
    }

    std::size_t offset { GeoRegistrationUl.size() };
    std::size_t payloadLength { 0U };
    std::size_t lenBytes { 0U };

    if (!KlvBer::decodeLength(data + offset, size - offset, payloadLength, lenBytes)) {
        return KlvStatus::MalformedBerLength;
    }
    offset += lenBytes;

    if (offset + payloadLength > size) {
        return KlvStatus::BufferUnderflow;
    }

    return parse(data + offset, payloadLength, outSet);
}

KlvStatus GeoRegistrationParser::parse(const std::uint8_t* data,
                                       std::size_t size,
                                       GeoRegistrationLocalSet& outSet) noexcept {
    outSet = GeoRegistrationLocalSet{};
    if (data == nullptr || size == 0U) {
        return KlvStatus::BufferUnderflow;
    }

    std::size_t offset { 0U };
    while (offset < size) {
        std::uint32_t tagId { 0U };
        std::size_t tagBytes { 0U };
        if (!KlvBer::decodeTag(data + offset, size - offset, tagId, tagBytes)) {
            return KlvStatus::TagError;
        }
        offset += tagBytes;

        std::size_t length { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, size - offset, length, lenBytes)) {
            return KlvStatus::MalformedBerLength;
        }
        offset += lenBytes;

        if (offset + length > size) {
            return KlvStatus::BufferUnderflow;
        }

        const std::uint8_t* valPtr = data + offset;
        const auto tag = static_cast<GeoRegistrationTag>(tagId);

        switch (tag) {
            case GeoRegistrationTag::DocumentVersion: {
                if (length >= 1U) {
                    outSet.documentVersion = valPtr[0];
                }
                break;
            }
            case GeoRegistrationTag::AlgorithmName: {
                outSet.algorithmName.assign(reinterpret_cast<const char*>(valPtr), length);
                break;
            }
            case GeoRegistrationTag::AlgorithmVersion: {
                outSet.algorithmVersion.assign(reinterpret_cast<const char*>(valPtr), length);
                break;
            }
            case GeoRegistrationTag::RowColPoints: {
                std::vector<std::vector<std::uint32_t>> matrix {};
                if (MdArray::decodeUInt2D(valPtr, length, matrix)) {
                    if (matrix.size() >= 2U && !matrix[0].empty()) {
                        const std::size_t numPoints = matrix[0].size();
                        outSet.pixelPoints.resize(numPoints);
                        const bool hasImg2 = (matrix.size() >= 4U);
                        for (std::size_t i = 0U; i < numPoints; ++i) {
                            outSet.pixelPoints[i].row1 = matrix[0][i];
                            outSet.pixelPoints[i].col1 = matrix[1][i];
                            if (hasImg2) {
                                outSet.pixelPoints[i].row2 = matrix[2][i];
                                outSet.pixelPoints[i].col2 = matrix[3][i];
                            }
                        }
                    }
                }
                break;
            }
            case GeoRegistrationTag::LatLonPoints: {
                std::vector<std::vector<double>> matrix {};
                if (MdArray::decodeFloat2D(valPtr, length, matrix)) {
                    if (matrix.size() >= 2U && !matrix[0].empty()) {
                        const std::size_t numPoints = matrix[0].size();
                        if (outSet.geoPoints.size() < numPoints) {
                            outSet.geoPoints.resize(numPoints);
                        }
                        for (std::size_t i = 0U; i < numPoints; ++i) {
                            outSet.geoPoints[i].lat = matrix[0][i];
                            outSet.geoPoints[i].lon = matrix[1][i];
                        }
                    }
                }
                break;
            }
            case GeoRegistrationTag::SecondImageName: {
                outSet.secondImageName = std::string(reinterpret_cast<const char*>(valPtr), length);
                break;
            }
            case GeoRegistrationTag::AlgorithmConfigId: {
                if (length == 16U) {
                    std::array<std::uint8_t, 16> uuid {};
                    std::memcpy(uuid.data(), valPtr, 16U);
                    outSet.configUuid = uuid;
                }
                break;
            }
            case GeoRegistrationTag::ElevationPoints: {
                std::vector<double> elevVec {};
                if (MdArray::decodeFloat1D(valPtr, length, elevVec)) {
                    const std::size_t numPoints = elevVec.size();
                    if (outSet.geoPoints.size() < numPoints) {
                        outSet.geoPoints.resize(numPoints);
                    }
                    for (std::size_t i = 0U; i < numPoints; ++i) {
                        outSet.geoPoints[i].elevation = elevVec[i];
                    }
                }
                break;
            }
            case GeoRegistrationTag::RowColCovariance: {
                std::vector<std::vector<double>> matrix {};
                if (MdArray::decodeFloat2D(valPtr, length, matrix)) {
                    if (matrix.size() >= 3U && !matrix[0].empty()) {
                        const std::size_t numPoints = matrix[0].size();
                        outSet.pixelCovariances.resize(numPoints);
                        const bool hasImg2 = (matrix.size() >= 6U);
                        for (std::size_t i = 0U; i < numPoints; ++i) {
                            outSet.pixelCovariances[i].sigmaRow1 = matrix[0][i];
                            outSet.pixelCovariances[i].sigmaCol1 = matrix[1][i];
                            outSet.pixelCovariances[i].rho1 = matrix[2][i];
                            if (hasImg2) {
                                outSet.pixelCovariances[i].sigmaRow2 = matrix[3][i];
                                outSet.pixelCovariances[i].sigmaCol2 = matrix[4][i];
                                outSet.pixelCovariances[i].rho2 = matrix[5][i];
                            }
                        }
                    }
                }
                break;
            }
            case GeoRegistrationTag::LatLonElevCovariance: {
                std::vector<std::vector<double>> matrix {};
                if (MdArray::decodeFloat2D(valPtr, length, matrix)) {
                    if (matrix.size() >= 3U && !matrix[0].empty()) {
                        const std::size_t numPoints = matrix[0].size();
                        outSet.geoCovariances.resize(numPoints);
                        const bool hasElev = (matrix.size() >= 6U);
                        for (std::size_t i = 0U; i < numPoints; ++i) {
                            outSet.geoCovariances[i].sigmaLat = matrix[0][i];
                            outSet.geoCovariances[i].sigmaLon = matrix[1][i];
                            outSet.geoCovariances[i].rhoLatLon = matrix[2][i];
                            if (hasElev) {
                                outSet.geoCovariances[i].sigmaElev = matrix[3][i];
                                outSet.geoCovariances[i].rhoLatElev = matrix[4][i];
                                outSet.geoCovariances[i].rhoLonElev = matrix[5][i];
                            }
                        }
                    }
                }
                break;
            }
            default:
                // Ignore unknown tags gracefully per MISB ST 0107 forward-compatibility
                break;
        }

        offset += length;
    }

    return KlvStatus::Success;
}

} // namespace Klv
