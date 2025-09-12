// Function lib for display of boat data in various chart formats
#pragma once
#include "OBP60Extensions.h"
#include "Pagedata.h"
// #include "OBPDataOperations.h"
// #include "OBPRingBuffer.h"

struct Point {
    int x;
    int y;
};

class Chart {
protected:
    RingBuffer<uint16_t>* dataBuf; // Buffer to display
    GwApi::BoatValue* bValue; // Present value to display additionally to chart
    int8_t chrtDir; // Chart direction: [0] = vertical, [1] = horizontal
    int8_t chrtSze; // Chart size: [0] = full size, [1] = half size left/top, [2] half size right/bottom
    int8_t chrtIntv; // Chart time interval: [4] 4 min., [8] 8 min., [12] 12 min., [16] 16 min., [32] 32 min.
    int dfltRng; // Default range of chart, e.g. 30 = [0..30]

    int top = 48; // display top header lines
    int bottom = 22; // display bottom lines
    int gap = 4; // gap between 2 charts; actual gap is 2x <gap>
    int cWidth;
    int cHeight;
    Point cStart; // start point for chart area
    int cLines; // number of chart lines
    int xCenter; // x center point of chart

    String dbName, dbFormat;
    int16_t dbMAX_VAL;
    size_t bufSize;
    GwApi::BoatValue* bValue;

public:
    Chart(RingBuffer<uint16_t>* dataBuf, GwApi::BoatValue* bValue, int8_t chrtDir, int8_t chrtSz, int8_t chrtIntv, int dfltRng, GwLog* logger)
        : dataBuf(dataBuf)
        , bValue(bValue)
        , chrtDir(chrtDir)
        , chrtSze(chrtSze)
        , chrtIntv(chrtIntv)
        , dfltRng(dfltRng)
    {
        cWidth = getdisplay().width();
        cHeight = getdisplay().height();
        cHeight = cHeight - top - bottom;
        if (chrtDir == 0) {
            // vertical chart
            switch (chrtSze) {
            case 0:
                // default is already set
                break;
            case 1:
                cWidth = cWidth;
                cHeight = cHeight / 2 - gap;
                cStart = { 30, cHeight + top };
                break;
            case 2:
                cWidth = cWidth;
                cHeight = cHeight / 2 - gap;
                cStart = { cWidth + gap, top };
                break;
            default:
                LOG_DEBUG(GwLog::DEBUG, "displayChart: wrong parameter");
                return;
            }
        } else if (chrtDir == 1) {
            // horizontal chart
            switch (chrtSze) {
            case 0:
                cStart = { 0, cHeight - bottom };
                break;
            case 1:
                cWidth = cWidth / 2 - gap;
                cHeight = cHeight;
                cStart = { 0, cHeight - bottom };
                break;
            case 2:
                cWidth = cWidth / 2 - gap;
                cHeight = cHeight;
                cStart = { cWidth + gap, cHeight - bottom };
                break;
            default:
                LOG_DEBUG(GwLog::DEBUG, "displayChart: wrong parameter");
                return;
            }
        } else {
            LOG_DEBUG(GwLog::DEBUG, "displayChart: wrong parameter");
            return;
        }
        xCenter = cWidth / 2;
        cLines = cHeight - 22;

        dataBuf->getMetaData(dbName, dbFormat);
        dbMAX_VAL = dataBuf->getMaxVal();
        bufSize = dataBuf->getCapacity();
        bValue->setFormat(dataBuf->getFormat());
    };
    void drawChrtHdr();
    void drawChrtGrd(const int chrtRng);
    bool drawChrt(int8_t chrtIntv, int dfltRng);
};