// Function lib for display of boat data in various graphical chart formats
#pragma once
#include "Pagedata.h"

struct Pos {
    int x;
    int y;
};
template <typename T> class RingBuffer;
class GwLog;

template <typename T>
class Chart {
protected:
    CommonData *commonData;
    GwLog *logger;

    RingBuffer<T> &dataBuf; // Buffer to display
    int8_t chrtDir; // Chart timeline direction: [0] = horizontal, [1] = vertical
    int8_t chrtSz; // Chart size: [0] = full size, [1] = half size left/top, [2] half size right/bottom
    double dfltRng; // Default range of chart, e.g. 30 = [0..30]
    uint16_t fgColor; // color code for any screen writing
    uint16_t bgColor; // color code for screen background
    bool useSimuData; // flag to indicate if simulation data is active

    int top = 48; // display top header lines
    int bottom = 22; // display bottom lines
    int hGap = 11; // gap between 2 horizontal charts; actual gap is 2x <gap>
    int vGap = 20; // gap between 2 vertical charts; actual gap is 2x <gap>
    int xOffset = 33; // offset for horizontal axis (time/value), because of space for left vertical axis labeling
    int yOffset = 10; // offset for vertical axis (time/value), because of space for top horizontal axis labeling
    int dWidth; // Display width
    int dHeight; // Display height
    int timAxis, valAxis; // size of time and value chart axis
    Pos cStart; // start point of chart area
    double chrtRng; // Range of buffer values from min to max value
    double chrtMin; // Range low end value
    double chrtMax; // Range high end value
    double chrtMid; // Range mid value
    double rngStep; // Defines the step of adjustment (e.g. 10 m/s) for value axis range
    bool recalcRngCntr = false; // Flag for re-calculation of mid value of chart for wind data types

    String dbName, dbFormat; // Name and format of data buffer
    int chrtDataFmt; // Data format of chart: [0] size values; [1] degree of course or wind; [2] rotational degrees
    double dbMIN_VAL; // Lowest possible value of buffer of type <T>
    double dbMAX_VAL; // Highest possible value of buffer of type <T>; indicates invalid value in buffer
    size_t bufSize; // History buffer size: 1.920 values for 32 min. history chart
    int count; // current size of buffer
    int numBufVals; // number of wind values available for current interval selection
    int bufStart; // 1st data value in buffer to show
    int numAddedBufVals; // Number of values added to buffer since last display
    size_t currIdx; // Current index in TWD history buffer
    size_t lastIdx; // Last index of TWD history buffer
    size_t lastAddedIdx = 0; // Last index of TWD history buffer when new data was added
    bool bufDataValid = false; // Flag to indicate if buffer data is valid
    int oldChrtIntv = 0; // remember recent user selection of data interval

    void drawChrt(int8_t chrtIntv, GwApi::BoatValue& currValue); // Draw chart line
    double getRng(double center, size_t amount); // Calculate range between chart center and edges
    void calcChrtBorders(double& rngMid, double& rngMin, double& rngMax, double& rng); // Calculate chart points for value axis and return range between <min> and <max>
    void drawChrtTimeAxis(int8_t chrtIntv); // Draw time axis of chart, value and lines
    void drawChrtValAxis(); // Draw value axis of chart, value and lines
    void prntCurrValue(GwApi::BoatValue& currValue); // Add current boat data value to chart 

public:
    Chart(RingBuffer<T>& dataBuf, int8_t chrtDir, int8_t chrtSz, double dfltRng, CommonData& common, bool useSimuData); // Chart object of data chart
    ~Chart();
    void showChrt(int8_t chrtIntv, GwApi::BoatValue currValue); // Perform all actions to draw chart

};