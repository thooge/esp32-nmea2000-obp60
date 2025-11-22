#include "OBPDataOperations.h"
#include "BoatDataCalibration.h"        // Functions lib for data instance calibration
#include <math.h>

// --- Class HstryBuf ---------------

// Init history buffers for selected boat data
void HstryBuf::init(BoatValueList* boatValues, GwLog *log) {

    logger = log;

    int hstryUpdFreq = 1000; // Update frequency for history buffers in ms
    int mltplr = 1000; // Multiplier which transforms original <double> value into buffer type format
    double hstryMinVal = 0; // Minimum value for these history buffers
    twdHstryMax = 2 * M_PI; // Max value for wind direction (TWD, AWD) in rad [0...2*PI]
    twsHstryMax = 65; // Max value for wind speed (TWS, AWS) in m/s [0..65] (limit due to type capacity of buffer - shifted by <mltplr>)
    awdHstryMax = twdHstryMax;
    awsHstryMax = twsHstryMax;
    twdHstryMin = hstryMinVal;
    twsHstryMin = hstryMinVal;
    awdHstryMin = hstryMinVal;
    awsHstryMin = hstryMinVal;
    const double DBL_MAX = std::numeric_limits<double>::max();

    // Initialize history buffers with meta data
    mltplr = 10000; // Store 4 decimals for course data
    hstryBufList.twdHstry->setMetaData("TWD", "formatCourse", hstryUpdFreq, mltplr, hstryMinVal, twdHstryMax);
    hstryBufList.awdHstry->setMetaData("AWD", "formatCourse", hstryUpdFreq, mltplr, hstryMinVal, twdHstryMax);
    mltplr = 1000; // Store 3 decimals for windspeed data
    hstryBufList.twsHstry->setMetaData("TWS", "formatKnots", hstryUpdFreq, mltplr, hstryMinVal, twsHstryMax);
    hstryBufList.awsHstry->setMetaData("AWS", "formatKnots", hstryUpdFreq, mltplr, hstryMinVal, twsHstryMax);

    // create boat values for history data types, if they don't exist yet
    twdBVal = boatValues->findValueOrCreate(hstryBufList.twdHstry->getName());
    twsBVal = boatValues->findValueOrCreate(hstryBufList.twsHstry->getName());
    twaBVal = boatValues->findValueOrCreate("TWA");
    awdBVal = boatValues->findValueOrCreate(hstryBufList.awdHstry->getName());
    awsBVal = boatValues->findValueOrCreate(hstryBufList.awsHstry->getName());

    if (!awdBVal->valid) { // AWD usually does not exist
        awdBVal->setFormat(hstryBufList.awdHstry->getFormat());
        awdBVal->value = DBL_MAX;
    }

    // collect boat values for true wind calculation
    awaBVal = boatValues->findValueOrCreate("AWA");
    hdtBVal = boatValues->findValueOrCreate("HDT");
    hdmBVal = boatValues->findValueOrCreate("HDM");
    varBVal = boatValues->findValueOrCreate("VAR");
    cogBVal = boatValues->findValueOrCreate("COG");
    sogBVal = boatValues->findValueOrCreate("SOG");
}

// Handle history buffers for TWD, TWS, AWD, AWS
//void HstryBuf::handleHstryBuf(GwApi* api, BoatValueList* boatValues, bool useSimuData) {
void HstryBuf::handleHstryBuf(bool useSimuData) {

    static double twd, tws, awd, aws, hdt = 20; //initial value only relevant if we use simulation data
    GwApi::BoatValue *calBVal; // temp variable just for data calibration -> we don't want to calibrate the original data here

    LOG_DEBUG(GwLog::DEBUG,"obp60task handleHstryBuf: TWD_isValid? %d, twdBVal: %.1f, twaBVal: %.1f, twsBVal: %.1f", twdBVal->valid, twdBVal->value * RAD_TO_DEG,
        twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852);

    if (twdBVal->valid) {
//    if (!useSimuData) {
        calBVal = new GwApi::BoatValue("TWD"); // temporary solution for calibration of history buffer values
        calBVal->setFormat(twdBVal->getFormat());
        calBVal->value = twdBVal->value;
        calBVal->valid = twdBVal->valid;
        calibrationData.calibrateInstance(calBVal, logger); // Check if boat data value is to be calibrated
        twd = calBVal->value;
        if (twd >= twdHstryMin && twd <= twdHstryMax) {
            hstryBufList.twdHstry->add(twd);
            LOG_DEBUG(GwLog::DEBUG,"obp60task handleHstryBuf: calBVal.value %.2f, twd: %.2f, twdHstryMin: %.1f, twdHstryMax: %.2f", calBVal->value, twd, twdHstryMin, twdHstryMax);
        }
        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
//    } else {
        twd += random(-20, 20);
        twd += static_cast<double>(random(-349, 349) / 1000.0); // add up to +/- 20 degree in RAD
        twd = WindUtils::to2PI(twd);
        hstryBufList.twdHstry->add(twd);
    }

    if (twsBVal->valid) {
        calBVal = new GwApi::BoatValue("TWS"); // temporary solution for calibration of history buffer values
        calBVal->setFormat(twsBVal->getFormat());
        calBVal->value = twsBVal->value;
        calBVal->valid = twsBVal->valid;
        calibrationData.calibrateInstance(calBVal, logger); // Check if boat data value is to be calibrated
        tws = calBVal->value;
        if (tws >= twsHstryMin && tws <= twsHstryMax) {
            hstryBufList.twsHstry->add(tws);
        }
        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
        // tws += random(-5000, 5000); // TWS value in m/s; expands to 3 decimals
        tws += static_cast<double>(random(-5000, 5000) / 1000.0); // add up to +/- 5 m/s TWS speed
        tws = constrain(tws, 0, 40); // Limit TWS to [0..40] m/s
        hstryBufList.twsHstry->add(tws);
    }

    if (awaBVal->valid) {
        if (hdtBVal->valid) {
            hdt = hdtBVal->value; // Use HDT if available
        } else {
            hdt = WindUtils::calcHDT(&hdmBVal->value, &varBVal->value, &cogBVal->value, &sogBVal->value);
        }

        awd = awaBVal->value + hdt;
        awd = WindUtils::to2PI(awd);
        calBVal = new GwApi::BoatValue("AWD"); // temporary solution for calibration of history buffer values
        calBVal->value = awd;
        calBVal->setFormat(awdBVal->getFormat());
        calBVal->valid = true;
        calibrationData.calibrateInstance(calBVal, logger); // Check if boat data value is to be calibrated
        awdBVal->value = calBVal->value;
        awdBVal->valid = true;
        awd = calBVal->value;
        if (awd >= awdHstryMin && awd <= awdHstryMax) {
            hstryBufList.awdHstry->add(awd);
        }
        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
        awd += static_cast<double>(random(-349, 349) / 1000.0); // add up to +/- 20 degree in RAD
        awd = WindUtils::to2PI(awd);
        hstryBufList.awdHstry->add(awd);
    }
    
    if (awsBVal->valid) {
        calBVal = new GwApi::BoatValue("AWS"); // temporary solution for calibration of history buffer values
        calBVal->setFormat(awsBVal->getFormat());
        calBVal->value = awsBVal->value;
        calBVal->valid = awsBVal->valid;
        calibrationData.calibrateInstance(calBVal, logger); // Check if boat data value is to be calibrated
        aws = calBVal->value;
        if (aws >= awsHstryMin && aws <= awsHstryMax) {
            hstryBufList.awsHstry->add(aws);
        }
        delete calBVal;
        calBVal = nullptr;
    } else if (useSimuData) {
        aws += static_cast<double>(random(-5000, 5000) / 1000.0); // add up to +/- 5 m/s TWS speed
        aws = constrain(aws, 0, 40); // Limit TWS to [0..40] m/s
        hstryBufList.awsHstry->add(aws);
    }
    LOG_DEBUG(GwLog::DEBUG,"obp60task handleHstryBuf-End: Buffer twdHstry: %.3f, twsHstry: %.3f, awdHstry: %.3f, awsHstry: %.3f", hstryBufList.twdHstry->getLast(), hstryBufList.twsHstry->getLast(),
        hstryBufList.awdHstry->getLast(),hstryBufList.awsHstry->getLast());
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
