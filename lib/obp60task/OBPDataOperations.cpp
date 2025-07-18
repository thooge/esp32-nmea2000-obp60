#include "OBPDataOperations.h"

void WindUtils::to2PI(double* a)
{
    while (*a < 0) {
        *a += 2 * M_PI;
    }
    *a = fmod(*a, 2 * M_PI);
}

void WindUtils::toPI(double* a)
{
    *a += M_PI;
    to2PI(a);
    *a -= M_PI;
}

void WindUtils::to360(double* a)
{
    while (*a < 0) {
        *a += 360;
    }
    *a = fmod(*a, 360);
}

void WindUtils::to180(double* a)
{
    *a += 180;
    to360(a);
    *a -= 180;
}

void WindUtils::toCart(const double* phi, const double* r, double* x, double* y)
{
    *x = *r * sin(radians(*phi));
    *y = *r * cos(radians(*phi));
}

void WindUtils::toPol(const double* x, const double* y, double* phi, double* r)
{
    *phi = 90 - degrees(atan2(*y, *x));
    to360(phi);
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
    double* TWD, double* TWS)
{
    double AWD = *AWA + *HDT;
    double stw = -*STW;
    Serial.println("calcTwdSA: AWA: " + String(*AWA) + ", AWS: " + String(*AWS) + ", CTW: " + String(*CTW) + ", STW: " + String(*STW) + ", HDT: " + String(*HDT));
    addPolar(&AWD, AWS, CTW, &stw, TWD, TWS);

    // Normalize TWD to 0-360°
    while (*TWD < 0)
        *TWD += 360;
    while (*TWD >= 360)
        *TWD -= 360;

    Serial.println("calcTwdSA: TWD: " + String(*TWD) + ", TWS: " + String(*TWS));
}

bool WindUtils::calcTrueWind(const double* awaVal, const double* awsVal,
    const double* cogVal, const double* stwVal, const double* hdtVal,
    const double* hdmVal, double* twdVal, double* twsVal)
{
    double hdt, ctw;
    double hdmVar = 3.0; // Magnetic declination, can be set from config if needed
    double twd, tws;

    if (*hdtVal == __DBL_MIN__) {
        if (*hdmVal != __DBL_MIN__) {
            hdt = *hdmVal + hdmVar; // Use corrected HDM if HDT is not available
        } else {
            return false; // Cannot calculate without valid HDT or HDM
        }
    }
    ctw = *hdtVal + ((*cogVal - *hdtVal) / 2); // Estimate CTW from COG

    if ((*awaVal == __DBL_MIN__) || (*awsVal == __DBL_MIN__) || (*cogVal == __DBL_MIN__) || (*stwVal == __DBL_MIN__)) {
        // Cannot calculate true wind without valid AWA, AWS, COG, or STW
        return false;
    } else {
        calcTwdSA(awaVal, awsVal, cogVal, stwVal, hdtVal, &twd, &tws);
        *twdVal = twd;
        *twsVal = tws;

        return true;
    }
}

/*
// make function available in Python for testing
static PyObject* true_wind(PyObject* self, PyObject* args) {
    double AWA,AWS,CTW,STW,HDT,TWS,TWD;
    if (!PyArg_ParseTuple(args, "ddddd", &AWA, &AWS, &CTW, &STW, &HDT)) {
        return NULL;
    }

    calc_true_wind(&AWA, &AWS, &CTW, &STW, &HDT, &TWD, &TWS);

    PyObject* twd = PyFloat_FromDouble(TWD);
    PyObject* tws = PyFloat_FromDouble(TWS);
    PyObject* tw = PyTuple_Pack(2,twd,tws);
    return tw;
}

static PyMethodDef methods[] = {
    {"true_wind", true_wind, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "truewind",    // Module name
    NULL,          // Optional docstring
    -1,
    methods
};

PyMODINIT_FUNC PyInit_truewind(void) {
    return PyModule_Create(&module);
}*/