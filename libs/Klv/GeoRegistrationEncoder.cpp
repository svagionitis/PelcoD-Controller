#include "GeoRegistrationEncoder.h"
#include "KlvBer.h"
#include "MdArray.h"

#include <algorithm>

namespace Klv {

KlvStatus GeoRegistrationEncoder::encodePacket(const GeoRegistrationLocalSet& set,
                                               std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> payload {};
    const auto status = encode(set, payload);
    if (status != KlvStatus::Success) {
        return status;
    }

    out.insert(out.end(), GeoRegistrationUl.begin(), GeoRegistrationUl.end());
    KlvBer::encodeLength(payload.size(), out);
    out.insert(out.end(), payload.begin(), payload.end());
    return KlvStatus::Success;
}

KlvStatus GeoRegistrationEncoder::encode(const GeoRegistrationLocalSet& set,
                                         std::vector<std::uint8_t>& out) {
    if (!set.validate()) {
        return KlvStatus::TagError;
    }

    // Tag 1: Document Version (Mandatory)
    KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::DocumentVersion), out);
    KlvBer::encodeLength(1U, out);
    out.push_back(set.documentVersion);

    // Tag 2: Algorithm Name (Mandatory)
    KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::AlgorithmName), out);
    KlvBer::encodeLength(set.algorithmName.size(), out);
    for (const char ch : set.algorithmName) {
        out.push_back(static_cast<std::uint8_t>(ch));
    }

    // Tag 3: Algorithm Version (Mandatory)
    KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::AlgorithmVersion), out);
    KlvBer::encodeLength(set.algorithmVersion.size(), out);
    for (const char ch : set.algorithmVersion) {
        out.push_back(static_cast<std::uint8_t>(ch));
    }

    // Tag 4: Row / Column Points (Optional)
    if (!set.pixelPoints.empty()) {
        const std::size_t numPoints = set.pixelPoints.size();
        const bool hasImg2 = set.pixelPoints[0].row2.has_value();
        const std::size_t numRows = hasImg2 ? 4U : 2U;

        std::uint32_t maxCoord { 0U };
        for (const auto& pt : set.pixelPoints) {
            maxCoord = std::max({ maxCoord, pt.row1, pt.col1 });
            if (hasImg2) {
                maxCoord = std::max({ maxCoord, pt.row2.value_or(0U), pt.col2.value_or(0U) });
            }
        }
        const std::size_t ebytes = (maxCoord > 65535U) ? 4U : 2U;

        std::vector<std::vector<std::uint32_t>> matrix(numRows, std::vector<std::uint32_t>(numPoints));
        for (std::size_t i = 0U; i < numPoints; ++i) {
            matrix[0][i] = set.pixelPoints[i].row1;
            matrix[1][i] = set.pixelPoints[i].col1;
            if (hasImg2) {
                matrix[2][i] = set.pixelPoints[i].row2.value_or(0U);
                matrix[3][i] = set.pixelPoints[i].col2.value_or(0U);
            }
        }

        std::vector<std::uint8_t> tag4Buf {};
        if (MdArray::encodeUInt2D(matrix, ebytes, tag4Buf)) {
            KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::RowColPoints), out);
            KlvBer::encodeLength(tag4Buf.size(), out);
            out.insert(out.end(), tag4Buf.begin(), tag4Buf.end());
        }
    }

    // Tag 5: Lat / Lon Points (Optional)
    if (!set.geoPoints.empty()) {
        const std::size_t numPoints = set.geoPoints.size();
        std::vector<std::vector<double>> matrix(2U, std::vector<double>(numPoints));
        for (std::size_t i = 0U; i < numPoints; ++i) {
            matrix[0][i] = set.geoPoints[i].lat;
            matrix[1][i] = set.geoPoints[i].lon;
        }

        std::vector<std::uint8_t> tag5Buf {};
        if (MdArray::encodeFloat2D(matrix, -180.0, 180.0, 4U, tag5Buf)) {
            KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::LatLonPoints), out);
            KlvBer::encodeLength(tag5Buf.size(), out);
            out.insert(out.end(), tag5Buf.begin(), tag5Buf.end());
        }
    }

    // Tag 6: Second Image Name (Optional)
    if (set.secondImageName.has_value() && !set.secondImageName->empty()) {
        KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::SecondImageName), out);
        KlvBer::encodeLength(set.secondImageName->size(), out);
        for (const char ch : *set.secondImageName) {
            out.push_back(static_cast<std::uint8_t>(ch));
        }
    }

    // Tag 7: Algorithm Configuration Identifier (Optional)
    if (set.configUuid.has_value()) {
        KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::AlgorithmConfigId), out);
        KlvBer::encodeLength(16U, out);
        out.insert(out.end(), set.configUuid->begin(), set.configUuid->end());
    }

    // Tag 8: Elevation Points (Optional)
    if (!set.geoPoints.empty() && set.geoPoints[0].elevation.has_value()) {
        std::vector<double> elevVec {};
        elevVec.reserve(set.geoPoints.size());
        double minElev { -900.0 };
        double maxElev { 19000.0 };
        for (const auto& pt : set.geoPoints) {
            const double val = pt.elevation.value_or(0.0);
            elevVec.push_back(val);
            if (val < minElev) {
                minElev = val - 100.0;
            }
            if (val > maxElev) {
                maxElev = val + 100.0;
            }
        }

        std::vector<std::uint8_t> tag8Buf {};
        if (MdArray::encodeFloat1D(elevVec, minElev, maxElev, 4U, tag8Buf)) {
            KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::ElevationPoints), out);
            KlvBer::encodeLength(tag8Buf.size(), out);
            out.insert(out.end(), tag8Buf.begin(), tag8Buf.end());
        }
    }

    // Tag 9: Row / Column Covariance (Optional)
    if (!set.pixelCovariances.empty()) {
        const std::size_t numPoints = set.pixelCovariances.size();
        const bool hasImg2 = set.pixelCovariances[0].sigmaRow2.has_value();
        const std::size_t numRows = hasImg2 ? 6U : 3U;

        std::vector<std::vector<double>> matrix(numRows, std::vector<double>(numPoints));
        for (std::size_t i = 0U; i < numPoints; ++i) {
            matrix[0][i] = set.pixelCovariances[i].sigmaRow1;
            matrix[1][i] = set.pixelCovariances[i].sigmaCol1;
            matrix[2][i] = set.pixelCovariances[i].rho1;
            if (hasImg2) {
                matrix[3][i] = set.pixelCovariances[i].sigmaRow2.value_or(0.0);
                matrix[4][i] = set.pixelCovariances[i].sigmaCol2.value_or(0.0);
                matrix[5][i] = set.pixelCovariances[i].rho2.value_or(0.0);
            }
        }

        std::vector<std::uint8_t> tag9Buf {};
        if (MdArray::encodeFloat2D(matrix, -1.0, 100.0, 2U, tag9Buf)) {
            KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::RowColCovariance), out);
            KlvBer::encodeLength(tag9Buf.size(), out);
            out.insert(out.end(), tag9Buf.begin(), tag9Buf.end());
        }
    }

    // Tag 10: Lat / Lon / Elev Covariance (Optional)
    if (!set.geoCovariances.empty()) {
        const std::size_t numPoints = set.geoCovariances.size();
        const bool hasElev = set.geoCovariances[0].sigmaElev.has_value();
        const std::size_t numRows = hasElev ? 6U : 3U;
        const double maxBound = hasElev ? 1000.0 : 650.0;

        std::vector<std::vector<double>> matrix(numRows, std::vector<double>(numPoints));
        for (std::size_t i = 0U; i < numPoints; ++i) {
            matrix[0][i] = set.geoCovariances[i].sigmaLat;
            matrix[1][i] = set.geoCovariances[i].sigmaLon;
            matrix[2][i] = set.geoCovariances[i].rhoLatLon;
            if (hasElev) {
                matrix[3][i] = set.geoCovariances[i].sigmaElev.value_or(0.0);
                matrix[4][i] = set.geoCovariances[i].rhoLatElev.value_or(0.0);
                matrix[5][i] = set.geoCovariances[i].rhoLonElev.value_or(0.0);
            }
        }

        std::vector<std::uint8_t> tag10Buf {};
        if (MdArray::encodeFloat2D(matrix, -1.0, maxBound, 2U, tag10Buf)) {
            KlvBer::encodeTag(static_cast<std::uint32_t>(GeoRegistrationTag::LatLonElevCovariance), out);
            KlvBer::encodeLength(tag10Buf.size(), out);
            out.insert(out.end(), tag10Buf.begin(), tag10Buf.end());
        }
    }

    return KlvStatus::Success;
}

} // namespace Klv
