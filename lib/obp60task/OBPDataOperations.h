#pragma once
#include "GwApi.h"
#include "OBPRingBuffer.h"
// #include <Arduino.h>
#include <math.h>

typedef struct {
    RingBuffer<int16_t>* twdHstry;
    RingBuffer<int16_t>* twsHstry;
    RingBuffer<int16_t>* awdHstry;
    RingBuffer<int16_t>* awsHstry;
} tBoatHstryData; // Holds pointers to all history buffers for boat data

class HstryBuf {

public:

};

class WindUtils {

public:
    static double to2PI(double a);
    static double toPI(double a);
    static double to360(double a);
    static double to180(double a);
    static void toCart(const double* phi, const double* r, double* x, double* y);
    static void toPol(const double* x, const double* y, double* phi, double* r);
    static void addPolar(const double* phi1, const double* r1,
        const double* phi2, const double* r2,
        double* phi, double* r);
    static void calcTwdSA(const double* AWA, const double* AWS,
        const double* CTW, const double* STW, const double* HDT,
        double* TWD, double* TWS, double* TWA);
    static double calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal);
    static bool calcTrueWind(const double* awaVal, const double* awsVal,
        const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
        const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal);
};