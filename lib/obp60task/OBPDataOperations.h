#pragma once
// #include <Python.h>
#include "GwApi.h"
#include <Arduino.h>
#include <math.h>

// #define radians(a) (a*0.017453292519943295)
// #define degrees(a) (a*57.29577951308232)

class WindUtils {

public:
    static void to360(double* a);
    static void to180(double* a);
    static void to2PI(double* a);
    static void toPI(double* a);
    static void toCart(const double* phi, const double* r, double* x, double* y);
    static void toPol(const double* x, const double* y, double* phi, double* r);
    static void addPolar(const double* phi1, const double* r1,
        const double* phi2, const double* r2,
        double* phi, double* r);
    static bool calcTrueWind(const double* awaVal, const double* awsVal,
        const double* cogVal, const double* stwVal, const double* hdtVal,
        const double* hdmVal, double* twdVal, double* twsVal);
    static void calcTwdSA(const double* AWA, const double* AWS,
        const double* CTW, const double* STW, const double* HDT,
        double* TWD, double* TWS);
};

/*
// make function available in Python for testing
static PyObject* true_wind(PyObject* self, PyObject* args);
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
} */