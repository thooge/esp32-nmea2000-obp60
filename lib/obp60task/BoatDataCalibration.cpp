#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include <cmath>
#include <math.h>
#include <unordered_map>

CalibrationDataList calibrationData;

void CalibrationDataList::readConfig(GwConfigHandler* config, GwLog* logger)
// Initial load of calibration data into internal list
// This method is called once at init phase of <obp60task> to read the configuration values
{
    String instance;
    double offset;
    double slope;
    double smooth;

    // Approximate mid-range values in m/s for Beaufort scale 0–12
    // hier geht's weiter mit den Bft-Werten: was muss ich bei welcher Windstärke addieren bzw. wie ist der Multiplikator?
    /*    static const std::array<std::pair<double, double>, 12> mps = {{
                {0.2, 1.3}, {1.5, 1.8}, {3.3, 2.1}, {5.4, 2.5},
                {7.9, 2.8}, {10.7, 3.1}, {13.8, 3.3}, {17.1, 3.6},
                {20.7, 3.7}, {24.4, 4.0}, {28.4, 4.2}, {32.6, 4.2}
            }}; */

    String calInstance = "";
    String calOffset = "";
    String calSlope = "";
    String calSmooth = "";

    // Load user format configuration values
    String lengthFormat = config->getString(config->lengthFormat); // [m|ft]
    String distanceFormat = config->getString(config->distanceFormat); // [m|km|nm]
    String speedFormat = config->getString(config->speedFormat); // [m/s|km/h|kn]
    String windspeedFormat = config->getString(config->windspeedFormat); // [m/s|km/h|kn|bft]
    String tempFormat = config->getString(config->tempFormat); // [K|C|F]

    // Read calibration settings for data instances
    for (int i = 0; i < maxCalibrationData; i++) {
        calInstance = "calInstance" + String(i + 1);
        calOffset = "calOffset" + String(i + 1);
        calSlope = "calSlope" + String(i + 1);
        calSmooth = "calSmooth" + String(i + 1);
        calibrationData.list[i] = { "---", 0.0f, 1.0f, 1, 0.0f, false };

        instance = config->getString(calInstance, "---");
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "no calibration data for instance no. %d", i + 1);
            continue;
        }
        offset = (config->getString(calOffset, "")).toFloat();
        slope = (config->getString(calSlope, "")).toFloat();
        smooth = (config->getString(calSmooth, "")).toInt(); // user input is int; further math is done with double

        // Convert calibration values to internal standard formats
        if (instance == "AWS" || instance == "TWS") {
            if (windspeedFormat == "m/s") {
                // No conversion needed
            } else if (windspeedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (windspeedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            } else if (windspeedFormat == "bft") {
                offset *= 2 + (offset / 2); // Convert Bft to m/s (approx) -> to be improved
            }

        } else if (instance == "AWA" || instance == "TWA" || instance == "TWD" || instance == "HDM" || instance == "PRPOS" || instance == "RPOS") {
            offset *= M_PI / 180; // Convert deg to rad

        } else if (instance == "DBT") {
            if (lengthFormat == "m") {
                // No conversion needed
            } else if (lengthFormat == "ft") {
                offset /= 3.28084; // Convert ft to m
            }

        } else if (instance == "STW") {
            if (speedFormat == "m/s") {
                // No conversion needed
            } else if (speedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (speedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            }

        } else if (instance == "WTemp") {
            if (tempFormat == "K" || tempFormat == "C") {
                // No conversion needed
            } else if (tempFormat == "F") {
                offset *= 9.0 / 5.0; // Convert °F to K
                slope *= 9.0 / 5.0; // Convert °F to K
            }
        }

        // transform smoothing factor from {0.01..10} to {0.3..0.95} and invert for exponential smoothing formula
        if (smooth <= 0) {
            smooth = 0;
        } else {
            if (smooth > 10) {
                smooth = 10;
            }
            smooth = 0.3 + ((smooth - 0.01) * (0.95 - 0.3) / (10 - 0.01));
        }
        smooth = 1 - smooth;

        calibrationData.list[i].instance = instance;
        calibrationData.list[i].offset = offset;
        calibrationData.list[i].slope = slope;
        calibrationData.list[i].smooth = smooth;
        calibrationData.list[i].isCalibrated = false;
        LOG_DEBUG(GwLog::LOG, "stored calibration data: %s, offset: %f, slope: %f, smoothing: %f", calibrationData.list[i].instance.c_str(),
            calibrationData.list[i].offset, calibrationData.list[i].slope, calibrationData.list[i].smooth);
    }
    LOG_DEBUG(GwLog::LOG, "all calibration data read");
}

int CalibrationDataList::getInstanceListNo(String instance)
// Method to get the index of the requested instance in the list
{
    // Check if instance is in the list
    for (int i = 0; i < maxCalibrationData; i++) {
        if (calibrationData.list[i].instance == instance) {
            return i;
        }
    }
    return -1; // instance not found
}

void CalibrationDataList::calibrateInstance(String instance, GwApi::BoatValue* boatDataValue, GwLog* logger)
// Method to calibrate the boat data value
{
    double offset = 0;
    double slope = 1.0;
    double dataValue = 0;

    int listNo = getInstanceListNo(instance);
    if (listNo < 0) {
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not found in calibration data list", instance.c_str());
        return;
    } else {
        offset = calibrationData.list[listNo].offset;
        slope = calibrationData.list[listNo].slope;

        if (!boatDataValue->valid) { // no valid boat data value, so we don't want to apply calibration data
            calibrationData.list[listNo].isCalibrated = false;
            return;
        } else {
            dataValue = boatDataValue->value;
            LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: value: %f, format: %s", boatDataValue->getName().c_str(), boatDataValue->value, boatDataValue->getFormat().c_str());

            if (boatDataValue->getFormat() == "formatWind") { // instance is of type angle
                dataValue = (dataValue * slope) + offset;
                dataValue = fmod(dataValue, 2 * M_PI);
                if (dataValue > (M_PI)) {
                    dataValue -= (2 * M_PI);
                } else if (dataValue < (M_PI * -1)) {
                    dataValue += (2 * M_PI);
                }
            } else if (boatDataValue->getFormat() == "formatCourse") { // instance is of type direction
                dataValue = (dataValue * slope) + offset;
                dataValue = fmod(dataValue, 2 * M_PI);
                if (dataValue < 0) {
                    dataValue += (2 * M_PI);
                }
            } else if (boatDataValue->getFormat() == "kelvinToC") { // instance is of type temperature
                dataValue = ((dataValue - 273.15) * slope) + offset + 273.15;
            } else {
                dataValue = (dataValue * slope) + offset;
            }

            calibrationData.list[listNo].isCalibrated = true;
            boatDataValue->value = dataValue;

            calibrationData.smoothInstance(instance, boatDataValue, logger); // smooth the boat data value
            calibrationData.list[listNo].value = boatDataValue->value; // store the calibrated + smoothed value in the list
            
            LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Offset: %f, Slope: %f, Result: %f", instance.c_str(), offset, slope, boatDataValue->value);
        }
    }
}

void CalibrationDataList::smoothInstance(String instance, GwApi::BoatValue* boatDataValue, GwLog* logger)
// Method to smoothen the boat data value
{
    // array for last values of smoothed boat data values
    static std::unordered_map<std::string, double> lastValue;

    double oldValue = 0;
    double dataValue = boatDataValue->value;

    if (!boatDataValue->valid) { // no valid boat data value, so we don't want to smoothen value
        return;
    } else {

        double smoothFactor = calibrationData.list[getInstanceListNo(instance)].smooth;

        if (lastValue.find(instance.c_str()) != lastValue.end()) {
            oldValue = lastValue[instance.c_str()];

            dataValue = oldValue + (smoothFactor * (dataValue - oldValue)); // exponential smoothing algorithm
        }
        lastValue[instance.c_str()] = dataValue; // store the new value for next cycle; first time, store only the current value and return
        boatDataValue->value = dataValue; // set the smoothed value to the boat data value

        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Smoothing factor: %f, Smoothed value: %f", instance.c_str(), smoothFactor, dataValue);
    }
}

#endif