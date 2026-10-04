#include "St1607Types.h"
#include "KlvTypes.h"

namespace Klv {

bool AmendLocalSet::validate() const noexcept {
    // ST 0601.19-46 & ST 0601.19-47: MSID must have localId > 0 or universalId set
    if (msid.localId == 0U && !msid.universalId.has_value()) {
        return false;
    }

    // Recursively validate child Amend Local Sets (ST 1607.2-08)
    for (const auto& child : childAmends) {
        if (!child.validate()) {
            return false;
        }
    }

    return true;
}

void AmendLocalSet::applyTo(UasDatalinkMessage& target) const noexcept {
    if (platformHeadingDeg)     target.platformHeadingDeg = platformHeadingDeg;
    if (platformPitchDeg)       target.platformPitchDeg = platformPitchDeg;
    if (platformRollDeg)        target.platformRollDeg = platformRollDeg;
    if (imageSourceSensor)      target.imageSourceSensor = imageSourceSensor;
    if (imageCoordinateSystem)  target.imageCoordinateSystem = imageCoordinateSystem;
    if (sensorLatitudeDeg)      target.sensorLatitudeDeg = sensorLatitudeDeg;
    if (sensorLongitudeDeg)     target.sensorLongitudeDeg = sensorLongitudeDeg;
    if (sensorTrueAltitudeM)    target.sensorTrueAltitudeM = sensorTrueAltitudeM;
    if (sensorHfovDeg)          target.sensorHfovDeg = sensorHfovDeg;
    if (sensorVfovDeg)          target.sensorVfovDeg = sensorVfovDeg;
    if (sensorRelAzimuthDeg)    target.sensorRelAzimuthDeg = sensorRelAzimuthDeg;
    if (sensorRelElevationDeg)  target.sensorRelElevationDeg = sensorRelElevationDeg;
    if (sensorRelRollDeg)       target.sensorRelRollDeg = sensorRelRollDeg;
    if (slantRangeM)            target.slantRangeM = slantRangeM;
    if (targetWidthM)           target.targetWidthM = targetWidthM;
    if (frameCenterLatDeg)      target.frameCenterLatDeg = frameCenterLatDeg;
    if (frameCenterLonDeg)      target.frameCenterLonDeg = frameCenterLonDeg;
    if (frameCenterElevM)       target.frameCenterElevM = frameCenterElevM;
    if (cornerCoordinates)      target.cornerCoordinates = cornerCoordinates;
    if (targetErrorCe90M)       target.targetErrorCe90M = targetErrorCe90M;
    if (targetErrorLe90M)       target.targetErrorLe90M = targetErrorLe90M;
    if (sensorAltitudeHaeM)     target.sensorAltitudeHaeM = sensorAltitudeHaeM;
    if (frameCenterElevHaeM)    target.frameCenterElevHaeM = frameCenterElevHaeM;
    if (sensorRollAngleDeg)     target.sensorRollAngleDeg = sensorRollAngleDeg;

    if (securityCountryCodingMethod || securityObjectCountryCodes) {
        if (!target.security) {
            target.security.emplace();
        }
        if (securityCountryCodingMethod) {
            target.security->objectCountryCodingMethod = *securityCountryCodingMethod;
        }
        if (securityObjectCountryCodes) {
            target.security->objectCountryCodes = *securityObjectCountryCodes;
        }
    }
}

bool SegmentLocalSet::validate() const noexcept {
    // ST 0601.19-46 & ST 0601.19-47: MSID must have localId > 0 or universalId set
    if (msid.localId == 0U && !msid.universalId.has_value()) {
        return false;
    }

    for (const auto& child : childSegments) {
        if (!child.validate()) {
            return false;
        }
    }

    for (const auto& child : childAmends) {
        if (!child.validate()) {
            return false;
        }
    }

    return true;
}

void SegmentLocalSet::applyTo(UasDatalinkMessage& target) const noexcept {
    if (imageSourceSensor)      target.imageSourceSensor = imageSourceSensor;
    if (imageCoordinateSystem)  target.imageCoordinateSystem = imageCoordinateSystem;
    if (sensorLatitudeDeg)      target.sensorLatitudeDeg = sensorLatitudeDeg;
    if (sensorLongitudeDeg)     target.sensorLongitudeDeg = sensorLongitudeDeg;
    if (sensorTrueAltitudeM)    target.sensorTrueAltitudeM = sensorTrueAltitudeM;
    if (sensorHfovDeg)          target.sensorHfovDeg = sensorHfovDeg;
    if (sensorVfovDeg)          target.sensorVfovDeg = sensorVfovDeg;
    if (sensorRelAzimuthDeg)    target.sensorRelAzimuthDeg = sensorRelAzimuthDeg;
    if (sensorRelElevationDeg)  target.sensorRelElevationDeg = sensorRelElevationDeg;
    if (sensorRelRollDeg)       target.sensorRelRollDeg = sensorRelRollDeg;
    if (slantRangeM)            target.slantRangeM = slantRangeM;
    if (targetWidthM)           target.targetWidthM = targetWidthM;
    if (frameCenterLatDeg)      target.frameCenterLatDeg = frameCenterLatDeg;
    if (frameCenterLonDeg)      target.frameCenterLonDeg = frameCenterLonDeg;
    if (frameCenterElevM)       target.frameCenterElevM = frameCenterElevM;
    if (cornerCoordinates)      target.cornerCoordinates = cornerCoordinates;
    if (sensorAltitudeHaeM)     target.sensorAltitudeHaeM = sensorAltitudeHaeM;
    if (frameCenterElevHaeM)    target.frameCenterElevHaeM = frameCenterElevHaeM;
    if (miisCoreId)             target.miisCoreId = miisCoreId;
    if (sensorRollAngleDeg)     target.sensorRollAngleDeg = sensorRollAngleDeg;

    if (securityCountryCodingMethod || securityObjectCountryCodes) {
        if (!target.security) {
            target.security.emplace();
        }
        if (securityCountryCodingMethod) {
            target.security->objectCountryCodingMethod = *securityCountryCodingMethod;
        }
        if (securityObjectCountryCodes) {
            target.security->objectCountryCodes = *securityObjectCountryCodes;
        }
    }
}

} // namespace Klv
