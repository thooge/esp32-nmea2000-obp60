#include "OBPDataOperations.h"
#include "BoatDataCalibration.h"        // Functions lib for data instance calibration
#include <math.h>
#include <vector>
#include <map>
#include <memory>

// --- Class HstryBuf ---------------

HstryBuf::HstryBuf(const String& name, int size, GwLog* log, BoatValueList* boatValues)
    : logger(log), boatDataName(name) {
    hstry.resize(size);
    boatValue = boatValues->findValueOrCreate(name);
}

void HstryBuf::init(const String& format, int updFreq, int mltplr, double minVal, double maxVal) {
    hstry.setMetaData(boatDataName, format, updFreq, mltplr, minVal, maxVal);
    hstryMin = minVal;
    hstryMax = maxVal;
    if (!boatValue->valid) {
        boatValue->setFormat(format);
        boatValue->value = std::numeric_limits<double>::max();
    }
}

void HstryBuf::add(double value) {
    if (value >= hstryMin && value <= hstryMax) {
        hstry.add(value);
    }
}

void HstryBuf::handle(bool useSimuData) {
    GwApi::BoatValue *calBVal;

    if (boatValue->valid) {
        calBVal = new GwApi::BoatValue(boatDataName.c_str());
        calBVal->setFormat(boatValue->getFormat());
        calBVal->value = boatValue->value;
        calBVal->valid = boatValue->valid;
        calibrationData.calibrateInstance(calBVal, logger);
        add(calBVal->value);
        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
        double simValue = hstry.getLast();
        if (boatDataName == "TWD" || boatDataName == "AWD") {
            simValue += static_cast<double>(random(-349, 349) / 1000.0);
            simValue = WindUtils::to2PI(simValue);
        } else if (boatDataName == "TWS" || boatDataName == "AWS") {
            simValue += static_cast<double>(random(-5000, 5000) / 1000.0);
            simValue = constrain(simValue, 0, 40);
        }
        add(simValue);
    }
}

// --- Class HstryManager ---------------
HstryManager::HstryManager(int size, GwLog* log, BoatValueList* boatValues) {
    // Create history buffers for each boat data type
    hstryBufs["TWD"] = std::unique_ptr<HstryBuf>(new HstryBuf("TWD", size, log, boatValues));
    hstryBufs["TWS"] = std::unique_ptr<HstryBuf>(new HstryBuf("TWS", size, log, boatValues));
    hstryBufs["AWD"] = std::unique_ptr<HstryBuf>(new HstryBuf("AWD", size, log, boatValues));
    hstryBufs["AWS"] = std::unique_ptr<HstryBuf>(new HstryBuf("AWS", size, log, boatValues));

    // Initialize metadata for each buffer
    int hstryUpdFreq = 1000; // Update frequency for history buffers in ms
    int mltplr = 1000; // Multiplier which transforms original <double> value into buffer type format
    double hstryMinVal = 0; // Minimum value for these history buffers
    double courseMax = 2 * M_PI;
    double speedMax = 65;

    mltplr = 10000; // Store 4 decimals for course data
    hstryBufs["TWD"]->init("formatCourse", hstryUpdFreq, mltplr, hstryMinVal, courseMax);
    hstryBufs["AWD"]->init("formatCourse", hstryUpdFreq, mltplr, hstryMinVal, courseMax);

    mltplr = 1000; // Store 3 decimals for windspeed data
    hstryBufs["TWS"]->init("formatKnots", hstryUpdFreq, mltplr, hstryMinVal, speedMax);
    hstryBufs["AWS"]->init("formatKnots", hstryUpdFreq, mltplr, hstryMinVal, speedMax);

    // collect boat values for true wind calculation
    awaBVal = boatValues->findValueOrCreate("AWA");
    hdtBVal = boatValues->findValueOrCreate("HDT");
    hdmBVal = boatValues->findValueOrCreate("HDM");
    varBVal = boatValues->findValueOrCreate("VAR");
    cogBVal = boatValues->findValueOrCreate("COG");
    sogBVal = boatValues->findValueOrCreate("SOG");
    awdBVal = boatValues->findValueOrCreate("AWD");
}

// Handle history buffers for TWD, TWS, AWD, AWS
void HstryManager::handleHstryBufs(bool useSimuData) {

    static double hdt = 20; //initial value only relevant if we use simulation data
    GwApi::BoatValue *calBVal; // temp variable just for data calibration -> we don't want to calibrate the original data here

    // Handle all registered history buffers
    for (auto& pair : hstryBufs) {
        auto& buf = pair.second;
        buf->handle(useSimuData);
    }

    // Special handling for AWD which is calculated
    if (awaBVal->valid) {
        if (hdtBVal->valid) {
            hdt = hdtBVal->value; // Use HDT if available
        } else {
            hdt = WindUtils::calcHDT(&hdmBVal->value, &varBVal->value, &cogBVal->value, &sogBVal->value);
        }
        double awd;
        awd = awaBVal->value + hdt;
        awd = WindUtils::to2PI(awd);
        calBVal = new GwApi::BoatValue("AWD"); // temporary solution for calibration of history buffer values
        calBVal->value = awd;
        calBVal->setFormat(awdBVal->getFormat());
        calBVal->valid = true;
        // We don't have a logger here, so we pass nullptr. This should be improved.
        calibrationData.calibrateInstance(calBVal, nullptr); // Check if boat data value is to be calibrated
        awdBVal->value = calBVal->value;
        awdBVal->valid = true;
        // Find the AWD buffer and add the value.
        auto it = hstryBufs.find("AWD");
        if (it != hstryBufs.end()) {
            it->second->add(calBVal->value);
        }

        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
        // Simulation for AWD is handled inside HstryBuf::handle
    }
}

RingBuffer<uint16_t>* HstryManager::getBuffer(const String& name) {
    auto it = hstryBufs.find(name);
    if (it != hstryBufs.end()) {
        return &it->second->hstry;
    }
    return nullptr;
}
// --- Class HstryBuf ---------------

// --- Class WindUtils --------------
double WindUtils::to2PI(double a)
{
    a = fmod(a, M_TWOPI);
    if (a < 0.0) {
        a += M_TWOPI;
    }
    return a;
}

double WindUtils::toPI(double a)
{
    a += M_PI;
    a = to2PI(a);
    a -= M_PI;

    return a;
}

double WindUtils::to360(double a)
{
    a = fmod(a, 360.0);
    if (a < 0.0) {
        a += 360.0;
    }
    return a;
}

double WindUtils::to180(double a)
{
    a += 180.0;
    a = to360(a);
    a -= 180.0;

    return a;
}

void WindUtils::toCart(const double* phi, const double* r, double* x, double* y)
{
    *x = *r * sin(*phi);
    *y = *r * cos(*phi);
}

void WindUtils::toPol(const double* x, const double* y, double* phi, double* r)
{
    *phi = (M_PI / 2) - atan2(*y, *x);
    *phi = to2PI(*phi);
    *r = sqrt(*x * *x + *y * *y);
}

void WindUtils::addPolar(const double* phi1, const double* r1,
    const double* phi2, const double* r2,
    double* phi, double* r)
{
    double x1, y1, x2, y2;
    toCart(phi1, r1, &x1, &y1);
    toCart(phi2, r2, &x2, &y2);
    x1 += x2;
    y1 += y2;
    toPol(&x1, &y1, phi, r);
}

void WindUtils::calcTwdSA(const double* AWA, const double* AWS,
    const double* CTW, const double* STW, const double* HDT,
    double* TWD, double* TWS, double* TWA)
{
    double awd = *AWA + *HDT;
    awd = to2PI(awd);
    double stw = -*STW;
    addPolar(&awd, AWS, CTW, &stw, TWD, TWS);

    // Normalize TWD and TWA to 0-360°
    *TWD = to2PI(*TWD);
    *TWA = toPI(*TWD - *HDT);
}

double WindUtils::calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal)
{
    double hdt;
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor

    if (*hdmVal != DBL_MAX) {
        hdt = *hdmVal + (*varVal != DBL_MAX ? *varVal : 0.0); // Use corrected HDM if HDT is not available (or just HDM if VAR is not available)
        hdt = to2PI(hdt);
    } else if (*cogVal != DBL_MAX && *sogVal >= minSogVal) {
        hdt = *cogVal; // Use COG as fallback if HDT and HDM are not available, and SOG is not data noise
    } else {
        hdt = DBL_MAX; // Cannot calculate HDT without valid HDM or HDM+VAR or COG
    }

    return hdt;
}

bool WindUtils::calcTrueWind(const double* awaVal, const double* awsVal,
    const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
    const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal)
{
    double stw, hdt, ctw;
    double twd, tws, twa;
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor

    if (*hdtVal != DBL_MAX) {
        hdt = *hdtVal; // Use HDT if available
    } else {
        hdt = calcHDT(hdmVal, varVal, cogVal, sogVal);
    }

    if (*cogVal != DBL_MAX && *sogVal >= minSogVal) { // if SOG is data noise, we don't trust COG

        ctw = *cogVal; // Use COG for CTW if available
    } else {
        ctw = hdt; // 2nd approximation for CTW; hdt must exist if we reach this part of the code
    }

    if (*stwVal != DBL_MAX) {
        stw = *stwVal; // Use STW if available
    } else if (*sogVal != DBL_MAX) {
        stw = *sogVal;
    } else {
        // If STW and SOG are not available, we cannot calculate true wind
        return false;
    }
//    Serial.println("\ncalcTrueWind: HDT: " + String(hdt) + ", CTW: " + String(ctw) + ", STW: " + String(stw));

    if ((*awaVal == DBL_MAX) || (*awsVal == DBL_MAX)) {
        // Cannot calculate true wind without valid AWA, AWS; other checks are done earlier
        return false;
    } else {
        calcTwdSA(awaVal, awsVal, &ctw, &stw, &hdt, &twd, &tws, &twa);
        *twdVal = twd;
        *twsVal = tws;
        *twaVal = twa;

        return true;
    }
}

// Calculate true wind data and add to obp60task boat data list
bool WindUtils::addTrueWind(GwApi* api, BoatValueList* boatValues, GwLog* log) {

    GwLog* logger = log;

    double awaVal, awsVal, cogVal, stwVal, sogVal, hdtVal, hdmVal, varVal;
    double twd, tws, twa;
    bool isCalculated = false;

    awaVal = awaBVal->valid ? awaBVal->value : DBL_MAX;
    awsVal = awsBVal->valid ? awsBVal->value : DBL_MAX;
    cogVal = cogBVal->valid ? cogBVal->value : DBL_MAX;
    stwVal = stwBVal->valid ? stwBVal->value : DBL_MAX;
    sogVal = sogBVal->valid ? sogBVal->value : DBL_MAX;
    hdtVal = hdtBVal->valid ? hdtBVal->value : DBL_MAX;
    hdmVal = hdmBVal->valid ? hdmBVal->value : DBL_MAX;
    varVal = varBVal->valid ? varBVal->value : DBL_MAX;
    LOG_DEBUG(GwLog::DEBUG,"obp60task addTrueWind: AWA %.1f, AWS %.1f, COG %.1f, STW %.1f, SOG %.2f, HDT %.1f, HDM %.1f, VAR %.1f", awaBVal->value * RAD_TO_DEG, awsBVal->value * 3.6 / 1.852,
            cogBVal->value * RAD_TO_DEG, stwBVal->value * 3.6 / 1.852, sogBVal->value * 3.6 / 1.852, hdtBVal->value * RAD_TO_DEG, hdmBVal->value * RAD_TO_DEG, varBVal->value * RAD_TO_DEG);

    isCalculated = calcTrueWind(&awaVal, &awsVal, &cogVal, &stwVal, &sogVal, &hdtVal, &hdmVal, &varVal, &twd, &tws, &twa);

    if (isCalculated) { // Replace values only, if successfully calculated and not already available
        if (!twdBVal->valid) {
            twdBVal->value = twd;
            twdBVal->valid = true;
        }
        if (!twsBVal->valid) {
            twsBVal->value = tws;
            twsBVal->valid = true;
        }
        if (!twaBVal->valid) {
            twaBVal->value = twa;
            twaBVal->valid = true;
        }
    }
    LOG_DEBUG(GwLog::DEBUG,"obp60task addTrueWind: isCalculated %d, TWD %.1f, TWA %.1f, TWS %.1f", isCalculated, twdBVal->value * RAD_TO_DEG,
        twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852);

    return isCalculated;
}
// --- Class WindUtils --------------
