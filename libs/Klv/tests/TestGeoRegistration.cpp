#include <gtest/gtest.h>

#include "GeoRegistrationEncoder.h"
#include "GeoRegistrationParser.h"
#include "GeoRegistrationTypes.h"
#include "MdArray.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Klv;

TEST(TestGeoRegistration, UniversalLabelIdentification) {
    EXPECT_TRUE(GeoRegistrationParser::isGeoRegistration(GeoRegistrationUl.data(), GeoRegistrationUl.size()));

    std::array<std::uint8_t, 16> invalidUl = GeoRegistrationUl;
    invalidUl[0] = 0xFFU;
    EXPECT_FALSE(GeoRegistrationParser::isGeoRegistration(invalidUl.data(), invalidUl.size()));
    EXPECT_FALSE(GeoRegistrationParser::isGeoRegistration(nullptr, 0U));
    EXPECT_FALSE(GeoRegistrationParser::isGeoRegistration(GeoRegistrationUl.data(), 15U));
}

TEST(TestGeoRegistration, MdArrayUInt2DNaturalFormat) {
    const std::vector<std::vector<std::uint32_t>> original = {
        { 133U, 128U, 97U, 69U },
        { 31U, 91U, 122U, 129U },
        { 89U, 82U, 52U, 27U },
        { 125U, 176U, 204U, 210U }
    };

    std::vector<std::uint8_t> encoded {};
    ASSERT_TRUE(MdArray::encodeUInt2D(original, 2U, encoded));
    EXPECT_FALSE(encoded.empty());

    std::vector<std::vector<std::uint32_t>> decoded {};
    ASSERT_TRUE(MdArray::decodeUInt2D(encoded.data(), encoded.size(), decoded));

    ASSERT_EQ(decoded.size(), original.size());
    for (std::size_t r = 0U; r < original.size(); ++r) {
        ASSERT_EQ(decoded[r].size(), original[r].size());
        for (std::size_t c = 0U; c < original[r].size(); ++c) {
            EXPECT_EQ(decoded[r][c], original[r][c]);
        }
    }
}

TEST(TestGeoRegistration, MdArrayFloat2DST1201) {
    const std::vector<std::vector<double>> original = {
        { 32.98416, 32.98417, 32.98418, 32.98419 },
        { 48.08388, 48.08389, 48.08390, 48.08391 }
    };

    std::vector<std::uint8_t> encoded {};
    ASSERT_TRUE(MdArray::encodeFloat2D(original, -180.0, 180.0, 4U, encoded));

    std::vector<std::vector<double>> decoded {};
    ASSERT_TRUE(MdArray::decodeFloat2D(encoded.data(), encoded.size(), decoded));

    ASSERT_EQ(decoded.size(), 2U);
    ASSERT_EQ(decoded[0].size(), 4U);
    ASSERT_EQ(decoded[1].size(), 4U);

    for (std::size_t r = 0U; r < 2U; ++r) {
        for (std::size_t c = 0U; c < 4U; ++c) {
            EXPECT_NEAR(decoded[r][c], original[r][c], 1e-5);
        }
    }
}

TEST(TestGeoRegistration, MdArrayFloat1DST1201) {
    const std::vector<double> original = { 1500.0, 1501.0, 1500.0, 1499.0 };

    std::vector<std::uint8_t> encoded {};
    ASSERT_TRUE(MdArray::encodeFloat1D(original, 0.0, 3000.0, 4U, encoded));

    std::vector<double> decoded {};
    ASSERT_TRUE(MdArray::decodeFloat1D(encoded.data(), encoded.size(), decoded));

    ASSERT_EQ(decoded.size(), original.size());
    for (std::size_t i = 0U; i < original.size(); ++i) {
        EXPECT_NEAR(decoded[i], original[i], 0.01);
    }
}

TEST(TestGeoRegistration, MandatoryTagsOnly) {
    GeoRegistrationLocalSet setIn {};
    setIn.documentVersion = 2U;
    setIn.algorithmName = "SURF-Homography";
    setIn.algorithmVersion = "1.4.2";

    EXPECT_TRUE(setIn.validate());

    std::vector<std::uint8_t> payload {};
    ASSERT_EQ(GeoRegistrationEncoder::encode(setIn, payload), KlvStatus::Success);

    GeoRegistrationLocalSet setOut {};
    ASSERT_EQ(GeoRegistrationParser::parse(payload.data(), payload.size(), setOut), KlvStatus::Success);

    EXPECT_EQ(setOut.documentVersion, 2U);
    EXPECT_EQ(setOut.algorithmName, "SURF-Homography");
    EXPECT_EQ(setOut.algorithmVersion, "1.4.2");
    EXPECT_TRUE(setOut.pixelPoints.empty());
    EXPECT_TRUE(setOut.geoPoints.empty());
}

TEST(TestGeoRegistration, TwoImageTiePoints) {
    // ST 1601.2 Table 2: 4 tie points between Image 1 and Image 2
    GeoRegistrationLocalSet setIn {};
    setIn.documentVersion = 2U;
    setIn.algorithmName = "SIFT-RANSAC";
    setIn.algorithmVersion = "2.0";
    setIn.secondImageName = "Reference_EO_Image_20230302.tif";

    const std::vector<std::uint32_t> img1Rows = { 133U, 128U, 97U, 69U };
    const std::vector<std::uint32_t> img1Cols = { 31U, 91U, 122U, 129U };
    const std::vector<std::uint32_t> img2Rows = { 89U, 82U, 52U, 27U };
    const std::vector<std::uint32_t> img2Cols = { 125U, 176U, 204U, 210U };

    for (std::size_t i = 0U; i < 4U; ++i) {
        TiePointPixel pt {};
        pt.row1 = img1Rows[i];
        pt.col1 = img1Cols[i];
        pt.row2 = img2Rows[i];
        pt.col2 = img2Cols[i];
        setIn.pixelPoints.push_back(pt);
    }

    EXPECT_TRUE(setIn.validate());
    EXPECT_EQ(setIn.tiePointCount(), 4U);

    std::vector<std::uint8_t> packet {};
    ASSERT_EQ(GeoRegistrationEncoder::encodePacket(setIn, packet), KlvStatus::Success);

    GeoRegistrationLocalSet setOut {};
    ASSERT_EQ(GeoRegistrationParser::parsePacket(packet.data(), packet.size(), setOut), KlvStatus::Success);

    EXPECT_EQ(setOut.documentVersion, 2U);
    EXPECT_EQ(setOut.algorithmName, "SIFT-RANSAC");
    EXPECT_EQ(setOut.algorithmVersion, "2.0");
    ASSERT_TRUE(setOut.secondImageName.has_value());
    EXPECT_EQ(*setOut.secondImageName, "Reference_EO_Image_20230302.tif");

    ASSERT_EQ(setOut.pixelPoints.size(), 4U);
    for (std::size_t i = 0U; i < 4U; ++i) {
        EXPECT_EQ(setOut.pixelPoints[i].row1, img1Rows[i]);
        EXPECT_EQ(setOut.pixelPoints[i].col1, img1Cols[i]);
        ASSERT_TRUE(setOut.pixelPoints[i].row2.has_value());
        ASSERT_TRUE(setOut.pixelPoints[i].col2.has_value());
        EXPECT_EQ(*setOut.pixelPoints[i].row2, img2Rows[i]);
        EXPECT_EQ(*setOut.pixelPoints[i].col2, img2Cols[i]);
    }
}

TEST(TestGeoRegistration, OneImageWithGeoPointsAndElevation) {
    // ST 1601.2 Table 3: One image with 4 ground geographic tie points
    GeoRegistrationLocalSet setIn {};
    setIn.documentVersion = 2U;
    setIn.algorithmName = "DEM-Correlator";
    setIn.algorithmVersion = "3.1";

    const std::vector<std::uint32_t> rows = { 133U, 128U, 97U, 69U };
    const std::vector<std::uint32_t> cols = { 31U, 91U, 122U, 129U };
    const std::vector<double> lats = { 32.98416, 32.98417, 32.98418, 32.98419 };
    const std::vector<double> lons = { 48.08388, 48.08389, 48.08390, 48.08391 };
    const std::vector<double> haes = { 1500.0, 1501.0, 1500.0, 1499.0 };

    for (std::size_t i = 0U; i < 4U; ++i) {
        TiePointPixel pix {};
        pix.row1 = rows[i];
        pix.col1 = cols[i];
        setIn.pixelPoints.push_back(pix);

        TiePointGeo geo {};
        geo.lat = lats[i];
        geo.lon = lons[i];
        geo.elevation = haes[i];
        setIn.geoPoints.push_back(geo);
    }

    EXPECT_TRUE(setIn.validate());
    EXPECT_EQ(setIn.tiePointCount(), 4U);

    std::vector<std::uint8_t> payload {};
    ASSERT_EQ(GeoRegistrationEncoder::encode(setIn, payload), KlvStatus::Success);

    GeoRegistrationLocalSet setOut {};
    ASSERT_EQ(GeoRegistrationParser::parse(payload.data(), payload.size(), setOut), KlvStatus::Success);

    ASSERT_EQ(setOut.pixelPoints.size(), 4U);
    ASSERT_EQ(setOut.geoPoints.size(), 4U);

    for (std::size_t i = 0U; i < 4U; ++i) {
        EXPECT_EQ(setOut.pixelPoints[i].row1, rows[i]);
        EXPECT_EQ(setOut.pixelPoints[i].col1, cols[i]);
        EXPECT_FALSE(setOut.pixelPoints[i].row2.has_value());

        EXPECT_NEAR(setOut.geoPoints[i].lat, lats[i], 1e-5);
        EXPECT_NEAR(setOut.geoPoints[i].lon, lons[i], 1e-5);
        ASSERT_TRUE(setOut.geoPoints[i].elevation.has_value());
        EXPECT_NEAR(*setOut.geoPoints[i].elevation, haes[i], 0.1);
    }
}

TEST(TestGeoRegistration, FullSetWithCovariancesAndUuid) {
    GeoRegistrationLocalSet setIn {};
    setIn.documentVersion = 2U;
    setIn.algorithmName = "Combined-Bundle-Adjustment";
    setIn.algorithmVersion = "4.0.0";
    setIn.secondImageName = "Image_Pair_B";
    setIn.configUuid = std::array<std::uint8_t, 16>{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };

    // 2 tie points
    for (std::size_t i = 0U; i < 2U; ++i) {
        TiePointPixel pix {};
        pix.row1 = 100U + static_cast<std::uint32_t>(i * 50U);
        pix.col1 = 200U + static_cast<std::uint32_t>(i * 50U);
        pix.row2 = 110U + static_cast<std::uint32_t>(i * 50U);
        pix.col2 = 210U + static_cast<std::uint32_t>(i * 50U);
        setIn.pixelPoints.push_back(pix);

        TiePointGeo geo {};
        geo.lat = 35.0 + static_cast<double>(i) * 0.1;
        geo.lon = -115.0 + static_cast<double>(i) * 0.1;
        geo.elevation = 800.0 + static_cast<double>(i) * 10.0;
        setIn.geoPoints.push_back(geo);

        TiePointPixelCovariance pixCov {};
        pixCov.sigmaRow1 = 1.5;
        pixCov.sigmaCol1 = 1.2;
        pixCov.rho1 = 0.45;
        pixCov.sigmaRow2 = 2.0;
        pixCov.sigmaCol2 = 1.8;
        pixCov.rho2 = 0.35;
        setIn.pixelCovariances.push_back(pixCov);

        TiePointGeoCovariance geoCov {};
        geoCov.sigmaLat = 15.0;
        geoCov.sigmaLon = 12.0;
        geoCov.rhoLatLon = 0.10;
        geoCov.sigmaElev = 25.0;
        geoCov.rhoLatElev = -0.05;
        geoCov.rhoLonElev = 0.08;
        setIn.geoCovariances.push_back(geoCov);
    }

    EXPECT_TRUE(setIn.validate());

    std::vector<std::uint8_t> packet {};
    ASSERT_EQ(GeoRegistrationEncoder::encodePacket(setIn, packet), KlvStatus::Success);

    GeoRegistrationLocalSet setOut {};
    ASSERT_EQ(GeoRegistrationParser::parsePacket(packet.data(), packet.size(), setOut), KlvStatus::Success);

    EXPECT_EQ(setOut.documentVersion, 2U);
    EXPECT_EQ(setOut.algorithmName, "Combined-Bundle-Adjustment");
    EXPECT_EQ(setOut.algorithmVersion, "4.0.0");
    ASSERT_TRUE(setOut.secondImageName.has_value());
    EXPECT_EQ(*setOut.secondImageName, "Image_Pair_B");
    ASSERT_TRUE(setOut.configUuid.has_value());
    EXPECT_EQ(*setOut.configUuid, (*setIn.configUuid));

    ASSERT_EQ(setOut.pixelPoints.size(), 2U);
    ASSERT_EQ(setOut.geoPoints.size(), 2U);
    ASSERT_EQ(setOut.pixelCovariances.size(), 2U);
    ASSERT_EQ(setOut.geoCovariances.size(), 2U);

    for (std::size_t i = 0U; i < 2U; ++i) {
        EXPECT_NEAR(setOut.pixelCovariances[i].sigmaRow1, setIn.pixelCovariances[i].sigmaRow1, 0.05);
        EXPECT_NEAR(setOut.pixelCovariances[i].sigmaCol1, setIn.pixelCovariances[i].sigmaCol1, 0.05);
        EXPECT_NEAR(setOut.pixelCovariances[i].rho1, setIn.pixelCovariances[i].rho1, 0.01);
        ASSERT_TRUE(setOut.pixelCovariances[i].sigmaRow2.has_value());
        EXPECT_NEAR(*setOut.pixelCovariances[i].sigmaRow2, *setIn.pixelCovariances[i].sigmaRow2, 0.05);

        EXPECT_NEAR(setOut.geoCovariances[i].sigmaLat, setIn.geoCovariances[i].sigmaLat, 0.1);
        EXPECT_NEAR(setOut.geoCovariances[i].sigmaLon, setIn.geoCovariances[i].sigmaLon, 0.1);
        EXPECT_NEAR(setOut.geoCovariances[i].rhoLatLon, setIn.geoCovariances[i].rhoLatLon, 0.01);
        ASSERT_TRUE(setOut.geoCovariances[i].sigmaElev.has_value());
        EXPECT_NEAR(*setOut.geoCovariances[i].sigmaElev, *setIn.geoCovariances[i].sigmaElev, 0.1);
    }
}

TEST(TestGeoRegistration, St1601Rule03ParityValidation) {
    // ST 1601.1-03: All included array items shall use the same number of tie points
    GeoRegistrationLocalSet set {};
    set.documentVersion = 2U;
    set.algorithmName = "Test";
    set.algorithmVersion = "1.0";

    TiePointPixel pix {};
    pix.row1 = 10U;
    pix.col1 = 20U;
    set.pixelPoints.push_back(pix);
    set.pixelPoints.push_back(pix); // 2 points

    TiePointGeo geo {};
    geo.lat = 30.0;
    geo.lon = 40.0;
    set.geoPoints.push_back(geo); // 1 point -> Mismatch!

    EXPECT_FALSE(set.validate());

    std::vector<std::uint8_t> out {};
    EXPECT_EQ(GeoRegistrationEncoder::encode(set, out), KlvStatus::TagError);

    // Fix parity: add second geo point
    set.geoPoints.push_back(geo);
    EXPECT_TRUE(set.validate());
    EXPECT_EQ(GeoRegistrationEncoder::encode(set, out), KlvStatus::Success);
}

TEST(TestGeoRegistration, MalformedPackets) {
    GeoRegistrationLocalSet outSet {};

    // Null or empty
    EXPECT_EQ(GeoRegistrationParser::parse(nullptr, 0U, outSet), KlvStatus::BufferUnderflow);
    EXPECT_EQ(GeoRegistrationParser::parsePacket(nullptr, 0U, outSet), KlvStatus::InvalidUniversalLabel);

    // Truncated UL
    const std::vector<std::uint8_t> truncatedUl = { 0x06, 0x0E, 0x2B };
    EXPECT_EQ(GeoRegistrationParser::parsePacket(truncatedUl.data(), truncatedUl.size(), outSet),
              KlvStatus::InvalidUniversalLabel);

    // Valid UL but truncated payload length
    std::vector<std::uint8_t> truncatedPayload = {
        0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01,
        0x0E, 0x01, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00,
        0x82, 0x01 // Declares 2-byte length but only 1 byte follows
    };
    EXPECT_EQ(GeoRegistrationParser::parsePacket(truncatedPayload.data(), truncatedPayload.size(), outSet),
              KlvStatus::MalformedBerLength);
}
