#include "OBPDataOperations.h"

double WindUtils::to2PI(double a)
{
    a = fmod(a, 2 * M_PI);
    if (a < 0.0) {
        a += 2 * M_PI;
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
    a = fmod(a, 360);
    if (a < 0.0) {
        a += 360;
    }
    return a;
}

double WindUtils::to180(double a)
{
    a += 180;
    a = to360(a);
    a -= 180;

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
    //    Serial.println("\ncalcTwdSA: AWA: " + String(*AWA) + ", AWS: " + String(*AWS) + ", CTW: " + String(*CTW) + ", STW: " + String(*STW) + ", HDT: " + String(*HDT));
    addPolar(&awd, AWS, CTW, &stw, TWD, TWS);

    // Normalize TWD and TWA to 0-360°
    *TWD = to2PI(*TWD);
    *TWA = toPI(*TWD - *HDT);
    //    Serial.println("calcTwdSA: TWD: " + String(*TWD) + ", TWS: " + String(*TWS));
}

bool WindUtils::calcTrueWind(const double* awaVal, const double* awsVal,
    const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
    const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal)
{
    double stw, hdt, ctw;
    double twd, tws, twa;
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor
    static const double DBL_MIN = std::numeric_limits<double>::lowest();

    // Serial.println("\ncalcTrueWind: HDT: " + String(*hdtVal) + ", HDM: " + String(*hdmVal) + ", VAR: " + String(*varVal) + ", SOG: " + String(*sogVal) + ", COG: " + String(*cogVal));
    if (*hdtVal != DBL_MIN) {
        hdt = *hdtVal; // Use HDT if available
    } else {
        if (*hdmVal != DBL_MIN) {
            hdt = *hdmVal + (*varVal != DBL_MIN ? *varVal : 0.0); // Use corrected HDM if HDT is not available (or just HDM if VAR is not available)
            hdt = to2PI(hdt);
        } else if (*cogVal != DBL_MIN && *sogVal >= minSogVal) {
            hdt = *cogVal; // Use COG as fallback if HDT and HDM are not available, and SOG is not data noise
        } else {
            return false; // Cannot calculate without valid HDT or HDM+VAR or COG
        }
    }

    if (*cogVal != DBL_MIN && *sogVal >= minSogVal) { // if SOG is data noise, we don't trust COG

        ctw = *cogVal; // Use COG for CTW if available
    } else {
        ctw = hdt; // 2nd approximation for CTW; hdt must exist if we reach this part of the code
    }

    if (*stwVal != DBL_MIN) {
        stw = *stwVal; // Use STW if available
    } else if (*sogVal != DBL_MIN) {
        stw = *sogVal;
    } else {
        // If STW and SOG are not available, we cannot calculate true wind
        return false;
    }
    // Serial.println("\ncalcTrueWind: HDT: " + String(hdt) + ", CTW: " + String(ctw) + ", STW: " + String(stw));

    if ((*awaVal == DBL_MIN) || (*awsVal == DBL_MIN)) {
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