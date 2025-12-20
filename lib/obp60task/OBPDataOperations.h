// Function lib for history buffer handling, true wind calculation, and other operations on boat data
#pragma once
#include "OBPRingBuffer.h"
#include "obp60task.h"
#include <map>
#include <memory>
#include <limits>
#include <cmath>

class HstryBuf {
private:
    RingBuffer<uint16_t> hstryBuf; // Circular buffer to store history values
    String boatDataName;
    double hstryMin;
    double hstryMax;
    GwApi::BoatValue *boatValue;
    GwLog *logger;

    friend class HstryBuffers;

public:
    HstryBuf(const String& name, int size, BoatValueList* boatValues, GwLog* log);
    void init(const String& format, int updFreq, int mltplr, double minVal, double maxVal);
    void add(double value);
    void handle(bool useSimuData);
};

class HstryBuffers {
private:
    std::map<String, std::unique_ptr<HstryBuf>> hstryBuffers;
    int size; // size of all history buffers
    BoatValueList* boatValueList;
    GwLog* logger;
    GwApi::BoatValue *awaBVal, *hdtBVal, *hdmBVal, *varBVal, *cogBVal, *sogBVal, *awdBVal; // boat values for true wind calculation

    struct HistoryParams {
        int hstryUpdFreq;
        int mltplr;
        double bufferMinVal;
        double bufferMaxVal;
        String format;
    };

    // Define buffer parameters for each boat data type
    std::map<String, HistoryParams> bufferParams = {
        {"AWA", {1000, 10000, -M_PI, M_PI, "formatWind"}},
        {"AWD", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"AWS", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"DBS", {1000, 100, 0.0, 650, "formatDepth"}},
        {"DBT", {1000, 100, 0.0, 650, "formatDepth"}},
        {"DPT", {1000, 100, 0.0, 650, "formatDepth"}},
        {"HDT", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"HDM", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"TWA", {1000, 10000, -M_PI, M_PI, "formatWind"}},
        {"TWD", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"TWS", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"SOG", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"STW", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"WTemp", {1000, 100, 0.0, 650.0, "kelvinToC"}}
    };

public:
    HstryBuffers(int size, BoatValueList* boatValues, GwLog* log);
    void addBuffer(const String& name);
    void handleHstryBufs(bool useSimuData);
    RingBuffer<uint16_t>* getBuffer(const String& name);
};

class WindUtils {
private:
    GwApi::BoatValue *twdBVal, *twsBVal, *twaBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    static constexpr double DBL_MAX = std::numeric_limits<double>::max();
    GwLog* logger;

public:
    WindUtils(BoatValueList* boatValues, GwLog* log)
        : logger(log) {
        twdBVal = boatValues->findValueOrCreate("TWD");
        twsBVal = boatValues->findValueOrCreate("TWS");
        twaBVal = boatValues->findValueOrCreate("TWA");
        awaBVal = boatValues->findValueOrCreate("AWA");
        awsBVal = boatValues->findValueOrCreate("AWS");
        cogBVal = boatValues->findValueOrCreate("COG");
        stwBVal = boatValues->findValueOrCreate("STW");
        sogBVal = boatValues->findValueOrCreate("SOG");
        hdtBVal = boatValues->findValueOrCreate("HDT");
        hdmBVal = boatValues->findValueOrCreate("HDM");
        varBVal = boatValues->findValueOrCreate("VAR");
    };

    static double to2PI(double a);
    static double toPI(double a);
    static double to360(double a);
    static double to180(double a);
    void toCart(const double* phi, const double* r, double* x, double* y);
    void toPol(const double* x, const double* y, double* phi, double* r);
    void addPolar(const double* phi1, const double* r1,
        const double* phi2, const double* r2,
        double* phi, double* r);
    void calcTwdSA(const double* AWA, const double* AWS,
        const double* CTW, const double* STW, const double* HDT,
        double* TWD, double* TWS, double* TWA);
    static double calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal);
    bool calcTrueWind(const double* awaVal, const double* awsVal,
        const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
        const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal);
//    bool addTrueWind(GwApi* api, BoatValueList* boatValues, GwLog *log);
    bool addTrueWind();
};