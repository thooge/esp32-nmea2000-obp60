// Function lib for history buffer handling, true wind calculation, and other operations on boat data
#pragma once
#include "OBPRingBuffer.h"
#include "obp60task.h"
#include "Pagedata.h"
#include <map>

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
    void handle(bool useSimuData, CommonData& common);
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
        {"COG", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"DBS", {1000, 100, 0.0, 650.0, "formatDepth"}},
        {"DBT", {1000, 100, 0.0, 650.0, "formatDepth"}},
        {"DPT", {1000, 100, 0.0, 650.0, "formatDepth"}},
        {"HDM", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"HDT", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"ROT", {1000, 10000, -M_PI / 180.0 * 99.0, M_PI / 180.0 * 99.0, "formatRot"}}, // min/max is -/+ 99 degrees for rotational angle
        {"SOG", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"STW", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"TWA", {1000, 10000, -M_PI, M_PI, "formatWind"}},
        {"TWD", {1000, 10000, 0.0, M_TWOPI, "formatCourse"}},
        {"TWS", {1000, 1000, 0.0, 65.0, "formatKnots"}},
        {"WTemp", {1000, 100, 0.0, 650.0, "kelvinToC"}}
    };

public:
    HstryBuffers(int size, BoatValueList* boatValues, GwLog* log);
    void addBuffer(const String& name);
    void handleHstryBufs(bool useSimuData, CommonData& common);
    RingBuffer<uint16_t>* getBuffer(const String& name);
};

class WindUtils {
private:
    GwApi::BoatValue *twaBVal, *twsBVal, *twdBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *awdBVal, *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    static constexpr double DBL_MAX = std::numeric_limits<double>::max();
    GwLog* logger;

public:
    WindUtils(BoatValueList* boatValues, GwLog* log)
        : logger(log) {
        twaBVal = boatValues->findValueOrCreate("TWA");
        twsBVal = boatValues->findValueOrCreate("TWS");
        twdBVal = boatValues->findValueOrCreate("TWD");
        awaBVal = boatValues->findValueOrCreate("AWA");
        awsBVal = boatValues->findValueOrCreate("AWS");
        awdBVal = boatValues->findValueOrCreate("AWD");
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
        double* TWD, double* TWS, double* TWA, double* AWD);
    static double calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal);
    bool calcWinds(const double* awaVal, const double* awsVal,
        const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
        const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal, double* awdVal);
    bool addWinds();
};