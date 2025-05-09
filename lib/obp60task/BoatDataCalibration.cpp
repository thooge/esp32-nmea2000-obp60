#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"

CalibrationDataList calibrationData;

void CalibrationDataList::readConfig(GwConfigHandler* config, GwLog* logger)
// Initial load of calibration data into internal list
// This function is called once at init phase of <OBP60task> to read the configuration values
{
    // Load user configuration values
    String lengthFormat = config->getString(config->lengthFormat); // [m|ft]
    String distanceFormat = config->getString(config->distanceFormat); // [m|km|nm]
    String speedFormat = config->getString(config->speedFormat); // [m/s|km/h|kn]
    String windspeedFormat = config->getString(config->windspeedFormat); // [m/s|km/h|kn|bft]
    String tempFormat = config->getString(config->tempFormat); // [K|°C|°F]

    // Read calibration settings for DBT
    calibrationData.list[0].instance = "DBT";
    calibrationData.list[0].offset = (config->getString(config->calOffsetDBT)).toFloat();
    if (lengthFormat == "ft") { // Convert DBT to SI standard meters
        calibrationData.list[0].offset *= 0.3048;
    }
    calibrationData.list[0].slope = 1.0; // No slope for DBT
    calibrationData.list[0].isCalibrated = false;

    // Read calibration settings for other data instances
    for (int i = 1; i < maxCalibrationData; i++) {
        String instance = "calInstance" + String(i);
        String offset = "calOffset" + String(i);
        String slope = "calSlope" + String(i);

//        calibrationData = new CalibrationDataList;
        calibrationData.list[i].instance = config->getString(instance, "");
        calibrationData.list[i].offset = (config->getString(offset, "")).toFloat();
        calibrationData.list[i].slope = (config->getString(slope, "")).toFloat();
        if (calibrationData.list[i].instance == "AWA" || calibrationData.list[i].instance == "AWS") {
            if (windspeedFormat == "m/s") { // Convert calibration values to SI standard m/s
                // No conversion needed
            } else if (windspeedFormat == "km/h") {
                calibrationData.list[i].offset /= 3.6; // Convert km/h to m/s
                calibrationData.list[i].slope /= 3.6; // Convert km/h to m/s
            } else if (windspeedFormat == "kn") {
                calibrationData.list[i].offset /= 1.94384; // Convert kn to m/s
                calibrationData.list[i].slope /= 1.94384; // Convert kn to m/s
            } else if (windspeedFormat == "bft") {
                calibrationData.list[i].offset *= 0.5; // Convert Bft to m/s (approx) -> to be improved
                calibrationData.list[i].slope *= 0.5; // Convert km/h to m/s
            }
        } else if (calibrationData.list[i].instance == "STW") {
            if (speedFormat == "m/s") {
                // No conversion needed
            } else if (speedFormat == "km/h") {
                calibrationData.list[i].offset /= 3.6; // Convert km/h to m/s
                calibrationData.list[i].slope /= 3.6; // Convert km/h to m/s
            } else if (speedFormat == "kn") {
                calibrationData.list[i].offset /= 1.94384; // Convert kn to m/s
                calibrationData.list[i].slope /= 1.94384; // Convert kn to m/s
            }
        } else if (calibrationData.list[i].instance == "WTemp") {
            if (tempFormat == "K" || tempFormat == "C") {
                // No conversion needed
            } else if (tempFormat == "F") {
                calibrationData.list[i].offset *= 5.0 / 9.0; // Convert °F to K
                calibrationData.list[i].slope *= 5.0 / 9.0;  // Convert °F to K
            }
        }
        calibrationData.list[i].isCalibrated = false;

        LOG_DEBUG(GwLog::LOG, "stored calibration data: %s, offset: %f, slope: %f", calibrationData.list[i].instance.c_str(), calibrationData.list[i].offset, calibrationData.list[i].slope);
    }
}

int CalibrationDataList::getInstanceListNo(String instance)
// Function to get the index of the requested instance in the list
{

    // Check if instance is in the list
    for (int i = 0; i < maxCalibrationData; i++) {
        if (calibrationData.list[i].instance == instance) {
            return i;
        }
    }
    return -1; // instance not found
}

/* void CalibrationDataList::updateBoatDataValidity(String instance)
{
    for (int i = 0; i < maxCalibrationData; i++) {
        if (calibrationData.list[i].instance == instance) {
            // test for boat data value validity - to be implemented
            calibrationData.list[i].isValid = true;
            return;
        }
    }
} */

void CalibrationDataList::calibrateInstance(String instance, GwApi::BoatValue* boatDataValue, GwLog* logger)
// Function to calibrate the boat data value
{
    double offset = 0;
    double slope = 1.0;

    int listNo = getInstanceListNo(instance);
    if (listNo >= 0) {
        offset = calibrationData.list[listNo].offset;
        slope = calibrationData.list[listNo].slope;

        if (!boatDataValue->valid) { // no valid boat data value, so we don't want to apply calibration data
            return;
        } else {
            boatDataValue->value = (boatDataValue->value * slope) + offset;
            calibrationData.list[listNo].value = boatDataValue->value;
            calibrationData.list[listNo].isCalibrated = true;
            LOG_DEBUG(GwLog::LOG, "BoatDataCalibration: %s: Offset: %f Slope: %f Result: %f", instance.c_str(), offset, slope, boatDataValue->value);

        }
    } else {
        LOG_DEBUG(GwLog::LOG, "BoatDataCalibration: %s not found in calibration data list", instance.c_str());
    }
}

#endif