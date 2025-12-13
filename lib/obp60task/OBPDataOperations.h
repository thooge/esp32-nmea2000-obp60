// Function lib for history buffer handling, true wind calculation, and other operations on boat data
#pragma once
#include "OBPRingBuffer.h"
#include "obp60task.h"
#include <vector>
#include <memory>
#include <map>

class HstryBuf {
private:
    GwLog *logger;
    RingBuffer<uint16_t> hstry; // Circular buffer to store history values
    String boatDataName;
    double hstryMin;
    double hstryMax;
    GwApi::BoatValue *boatValue;

    friend class HstryManager;
    void handleHistory(bool useSimuData);

public:
    HstryBuf(const String& name, int size, GwLog* log, BoatValueList* boatValues);
    void init(const String& format, int updFreq, int mltplr, double minVal, double maxVal);
    void add(double value);
    void handle(bool useSimuData);
};

class HstryManager {
private:
    std::map<String, std::unique_ptr<HstryBuf>> hstryBufs;
    // boat values for true wind calculation
    GwApi::BoatValue *awaBVal, *hdtBVal, *hdmBVal, *varBVal, *cogBVal, *sogBVal, *awdBVal;
public:
    HstryManager(int size, GwLog* log, BoatValueList* boatValues);
    void handleHstryBufs(bool useSimuData);
    RingBuffer<uint16_t>* getBuffer(const String& name);
};

class WindUtils {
private:
    GwApi::BoatValue *twdBVal, *twsBVal, *twaBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    static constexpr double DBL_MAX = std::numeric_limits<double>::max();

public:
    WindUtils(BoatValueList* boatValues){
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
    bool addTrueWind(GwApi* api, BoatValueList* boatValues, GwLog *log);
};