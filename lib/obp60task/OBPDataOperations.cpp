#include "OBPDataOperations.h"
#include "BoatDataCalibration.h"        // Functions lib for data instance calibration
#include <math.h>

// --- Class HstryBuf ---------------
HstryBuf::HstryBuf(const String& name, int size, BoatValueList* boatValues, GwLog* log)
    : logger(log), boatDataName(name) {
    hstryBuf.resize(size);
    boatValue = boatValues->findValueOrCreate(name);
}

void HstryBuf::init(const String& format, int updFreq, int mltplr, double minVal, double maxVal) {
    hstryBuf.setMetaData(boatDataName, format, updFreq, mltplr, minVal, maxVal);
    hstryMin = minVal;
    hstryMax = maxVal;
    if (!boatValue->valid) {
        boatValue->setFormat(format);
        boatValue->value = std::numeric_limits<double>::max(); // mark current value invalid
    }
}

void HstryBuf::add(double value) {
    if (value >= hstryMin && value <= hstryMax) {
        hstryBuf.add(value);
    }
}

void HstryBuf::handle(bool useSimuData) {
    GwApi::BoatValue *calBVal;

    if (boatValue->valid) { // add calibrated boat value to history buffer
        calBVal = new GwApi::BoatValue(boatDataName.c_str());
        calBVal->setFormat(boatValue->getFormat());
        calBVal->value = boatValue->value;
        calBVal->valid = boatValue->valid;
        calibrationData.calibrateInstance(calBVal, logger);
        add(calBVal->value);
        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) { // add simulated value to history buffer
        double simValue = hstryBuf.getLast();
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
// --- End Class HstryBuf ---------------

// --- Class HstryBuffers ---------------
HstryBuffers::HstryBuffers(int size, BoatValueList* boatValues, GwLog* log)
    : size(size), boatValueList(boatValues), logger(log) {

    // collect boat values for true wind calculation
    // should have been already all created at true wind object initialization
    // potentially to be moved to history buffer handling
    awaBVal = boatValueList->findValueOrCreate("AWA");
    hdtBVal = boatValueList->findValueOrCreate("HDT");
    hdmBVal = boatValueList->findValueOrCreate("HDM");
    varBVal = boatValueList->findValueOrCreate("VAR");
    cogBVal = boatValueList->findValueOrCreate("COG");
    sogBVal = boatValueList->findValueOrCreate("SOG");
    awdBVal = boatValueList->findValueOrCreate("AWD");
}

void HstryBuffers::addBuffer(const String& name) {
    // Create history buffer for boat data type

    if (HstryBuffers::getBuffer(name) != nullptr) { // buffer for this data type already exists
        return;
    }

    hstryBuffers[name] = std::unique_ptr<HstryBuf>(new HstryBuf(name, size, boatValueList, logger));

    // Initialize metadata for buffer
    // String valueFormat = boatValueList->findValueOrCreate(name)->getFormat().c_str(); // Unfortunately, format is not yet available during system initialization
    String valueFormat = bufferParams[name].format; // Data format of boat data type
    int hstryUpdFreq = bufferParams[name].hstryUpdFreq; // Update frequency for history buffers in ms
    int mltplr = bufferParams[name].mltplr; // default multiplier which transforms original <double> value into buffer type format
    double bufferMinVal = bufferParams[name].bufferMinVal; // Min value for this history buffer
    double bufferMaxVal = bufferParams[name].bufferMaxVal; // Max value for this history buffer

    hstryBuffers[name]->init(valueFormat, hstryUpdFreq, mltplr, bufferMinVal, bufferMaxVal);
    LOG_DEBUG(GwLog::DEBUG,"HstryBuffers-new buffer added: name: %s, format: %s, multiplier: %d, min value: %.2f, max value: %.2f", name, valueFormat, mltplr, bufferMinVal, bufferMaxVal);
}

// Handle history buffers for TWD, TWS, AWD, AWS
void HstryBuffers::handleHstryBufs(bool useSimuData) {

    static double hdt = 20; //initial value only relevant if we use simulation data
    GwApi::BoatValue *calBVal; // temp variable just for data calibration -> we don't want to calibrate the original data here

    // Handle all registered history buffers
    for (auto& pair : hstryBuffers) {
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
        auto it = hstryBuffers.find("AWD");
        if (it != hstryBuffers.end()) {
            it->second->add(calBVal->value);
        }

        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
        // Simulation for AWD is handled inside HstryBuf::handle
    }
}

RingBuffer<uint16_t>* HstryBuffers::getBuffer(const String& name) {
    auto it = hstryBuffers.find(name);
    if (it != hstryBuffers.end()) {
        return &it->second->hstryBuf;
    }
    return nullptr;
}
// --- End Class HstryBuffers ---------------

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
//bool WindUtils::addTrueWind(GwApi* api, GwLog* log) {
bool WindUtils::addTrueWind() {

//    GwLog* logger = log;

//    double awaVal, awsVal, cogVal, stwVal, sogVal, hdtVal, hdmVal, varVal;
    double twd, tws, twa;
    bool isCalculated = false;

    double awaVal = awaBVal->valid ? awaBVal->value : DBL_MAX;
    double awsVal = awsBVal->valid ? awsBVal->value : DBL_MAX;
    double cogVal = cogBVal->valid ? cogBVal->value : DBL_MAX;
    double stwVal = stwBVal->valid ? stwBVal->value : DBL_MAX;
    double sogVal = sogBVal->valid ? sogBVal->value : DBL_MAX;
    double hdtVal = hdtBVal->valid ? hdtBVal->value : DBL_MAX;
    double hdmVal = hdmBVal->valid ? hdmBVal->value : DBL_MAX;
    double varVal = varBVal->valid ? varBVal->value : DBL_MAX;
    LOG_DEBUG(GwLog::DEBUG,"WindUtils:addTrueWind: AWA %.1f, AWS %.1f, COG %.1f, STW %.1f, SOG %.2f, HDT %.1f, HDM %.1f, VAR %.1f", awaBVal->value * RAD_TO_DEG, awsBVal->value * 3.6 / 1.852,
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
    LOG_DEBUG(GwLog::DEBUG,"WindUtils:addTrueWind: isCalculated %d, TWD %.1f, TWA %.1f, TWS %.1f", isCalculated, twdBVal->value * RAD_TO_DEG,
        twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852);

    return isCalculated;
}
// --- End Class WindUtils --------------
