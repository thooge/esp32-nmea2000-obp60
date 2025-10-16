// Function lib for display of boat data in various chart formats
#include "OBPcharts.h"
#include "OBP60Extensions.h"
#include "OBPRingBuffer.h"

// --- Class Chart ---------------
template <typename T>
Chart<T>::Chart(RingBuffer<T>& dataBuf, int8_t chrtDir, int8_t chrtSz, int dfltRng, CommonData& common, bool useSimuData)
    : dataBuf(dataBuf)
    , chrtDir(chrtDir)
    , chrtSz(chrtSz)
    , dfltRng(dfltRng)
    , commonData(&common)
    , useSimuData(useSimuData)
{
    logger = commonData->logger;
    fgColor = commonData->fgcolor;
    bgColor = commonData->bgcolor;

    LOG_DEBUG(GwLog::DEBUG, "Chart create: dataBuf: %p", (void*)&dataBuf);
    dWidth = getdisplay().width();
    dHeight = getdisplay().height();

    if (chrtDir == 0) {
        // horizontal chart timeline direction
        timAxis = dWidth - xOffset;
        switch (chrtSz) {
        case 0:
            valAxis = dHeight - top - bottom;
            cStart = { xOffset, top };
            break;
        case 1:
            valAxis = (dHeight - top - bottom) / 2 - gap;
            cStart = { xOffset, top };
            break;
        case 2:
            valAxis = (dHeight - top - bottom) / 2 - gap;
            cStart = { xOffset, top + (valAxis + gap) + gap };
            break;
        default:
            LOG_DEBUG(GwLog::DEBUG, "displayChart: wrong parameter");
            return;
        }
    } else if (chrtDir == 1) {
        // vertical chart timeline direction
        timAxis = dHeight - top - bottom;
        switch (chrtSz) {
        case 0:
            valAxis = dWidth - xOffset;
            cStart = { xOffset, top };
            break;
        case 1:
            valAxis = dWidth / 2 - gap;
            cStart = { 0, top };
            break;
        case 2:
            valAxis = dWidth / 2 - gap;
            cStart = { dWidth / 2 + gap, top };
            break;
        default:
            LOG_DEBUG(GwLog::DEBUG, "displayChart: wrong parameter");
            return;
        }
    } else {
        LOG_DEBUG(GwLog::DEBUG, "displayChart: wrong parameter");
        return;
    }
    //    xCenter = timAxis / 2;

    dataBuf.getMetaData(dbName, dbFormat);
    dbMAX_VAL = dataBuf.getMaxVal();
    bufSize = dataBuf.getCapacity();
    LOG_DEBUG(GwLog::DEBUG, "Chart create: dWidth: %d, dHeight: %d, timAxis: %d, valAxis: %d, cStart {x,y}: %d, %d, dbname: %s", dWidth, dHeight, timAxis, valAxis, cStart.x, cStart.y, dbName);
};

template <typename T>
Chart<T>::~Chart()
{
}

// chart time axis label + lines
template <typename T>
void Chart<T>::drawChrtTimeAxis(int8_t chrtIntv)
{
    int timeRng;
    float slots, intv, i;
    char sTime[6];

    getdisplay().setTextColor(fgColor);
    if (chrtDir == 0) { // horizontal chart
        getdisplay().fillRect(0, top, dWidth, 2, fgColor);
        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        timeRng = chrtIntv * 4; // Chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [8] 32 min.
        slots = (timAxis - xOffset) / 75.0; // number of axis labels
        intv = timeRng / slots; // minutes per chart axis interval
        i = timeRng; // Chart axis label start at -32, -16, -12, ... minutes

        for (int j = 0; j < timAxis - 30; j += 75) {
            LOG_DEBUG(GwLog::DEBUG, "ChartHdr: timAxis: %d, {x,y}: {%d,%d}, i: %.1f, j: %d, chrtIntv: %d, intv: %.1f, slots: %.1f", timAxis, cStart.x, cStart.y, i, j, chrtIntv, intv, slots);
            if (chrtIntv < 3) {
                snprintf(sTime, size_t(sTime), "-%.1f", i);
                drawTextCenter(cStart.x + j - 8, cStart.y - 8, sTime); // time interval
            } else {
                snprintf(sTime, size_t(sTime), "-%.0f", std::round(i));
                drawTextCenter(cStart.x + j - 4, cStart.y - 8, sTime); // time interval
            }
            getdisplay().drawLine(cStart.x + j, cStart.y, cStart.x + j, cStart.y + 5, fgColor);
            i -= intv;
        }
        /*        getdisplay().setFont(&Ubuntu_Bold8pt8b);
                getdisplay().setCursor(timAxis - 8, cStart.y - 2);
                getdisplay().print("min"); */

    } else { // chrtDir == 1; vertical chart
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        timeRng = chrtIntv * 4; // Chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [8] 32 min.
        slots = timAxis / 75.0; // number of axis labels
        intv = timeRng / slots; // minutes per chart axis interval
        i = 0; // Chart axis label start at -32, -16, -12, ... minutes

        for (int j = 0; j < (timAxis - 75); j += 75) { // don't print time label at lower end
            LOG_DEBUG(GwLog::DEBUG, "ChartHdr: timAxis: %d, {x,y}: {%d,%d}, i: %.1f, j: %d, chrtIntv: %d, intv: %.1f, slots: %.1f", timAxis, cStart.x, cStart.y, i, j, chrtIntv, intv, slots);
            if (chrtIntv < 3) { // print 1 decimal if time range is single digit (4 or 8 minutes)
                snprintf(sTime, size_t(sTime), "%.1f", i * -1);
            } else {
                snprintf(sTime, size_t(sTime), "%.0f", std::round(i) * -1);
            }
            drawTextCenter(dWidth / 2, cStart.y + j, sTime); // time value
            getdisplay().drawLine(cStart.x, cStart.y + j, cStart.x + valAxis, cStart.y + j, fgColor); // Grid line
            i += intv;
        }
    }
}

// chart value axis labels + lines
template <typename T>
void Chart<T>::drawChrtValAxis()
{
    float slots;
    int i, intv;
    char sVal[6];

    getdisplay().setFont(&Ubuntu_Bold10pt8b);
    if (chrtDir == 0) { // horizontal chart
        slots = valAxis / 60.0; // number of axis labels
        intv = static_cast<int>(round(chrtRng / slots));
        i = intv;
        for (int j = 60; j < valAxis - 30; j += 60) {
            LOG_DEBUG(GwLog::DEBUG, "ChartGrd: chrtRng: %d, intv: %d, slots: %.1f, valAxis: %d, i: %d, j: %d", chrtRng, intv, slots, valAxis, i, j);
            getdisplay().fillRect(cStart.x - xOffset, cStart.y + j - 9, cStart.x - xOffset + 28, 12, bgColor); // Clear small area to remove potential chart lines
            String sVal = String(static_cast<int>(round(i)));
            getdisplay().setCursor((3 - sVal.length()) * 9, cStart.y + j + 4); // value right-formated
            getdisplay().printf("%s", sVal); // Range value
            i += intv;
            getdisplay().drawLine(cStart.x + 2, cStart.y + j, cStart.x + timAxis, cStart.y + j, fgColor);
        }
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        drawTextRalign(cStart.x + timAxis, cStart.y - 3, dataBuf.getName()); // buffer data name

    } else { // chrtDir == 1; vertical chart
        getdisplay().fillRect(cStart.x, top, valAxis, 2, fgColor); // top chart line
        getdisplay().setCursor(cStart.x, cStart.y - 2);
        snprintf(sVal, sizeof(sVal), "%d", dataBuf.getMin(numBufVals) / 1000);
        getdisplay().printf("%s", sVal); // Range low end
        snprintf(sVal, sizeof(sVal), "%.0f", round(chrtRng / 2));
        drawTextCenter(cStart.x + (valAxis / 2), cStart.y - 10, sVal); // Range mid end
        snprintf(sVal, sizeof(sVal), "%.0f", round(chrtRng));
        drawTextRalign(cStart.x + valAxis - 1, cStart.y - 2, sVal); // Range high end
        for (int j = 0; j <= valAxis; j += (valAxis / 2)) {
            getdisplay().drawLine(cStart.x + j - 1, cStart.y, cStart.x + j - 1, cStart.y + timAxis, fgColor);
        }
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        drawTextCenter(cStart.x + (valAxis / 4) + 4, cStart.y - 11, dataBuf.getName()); // buffer data name
        LOG_DEBUG(GwLog::DEBUG, "ChartGrd: chrtRng: %d, intv: %d, slots: %.1f, valAxis: %d, i: %d, sVal.length: %d", chrtRng, intv, slots, valAxis, i, sizeof(sVal));
    }
}

// draw chart
template <typename T>
void Chart<T>::drawChrt(int8_t chrtIntv, GwApi::BoatValue currValue)
{
    float chrtScl; // Scale for data values in pixels per value
    int chrtVal; // Current data value
    static int chrtPrevVal; // Last data value in chart area
    bool bufDataValid = false; // Flag to indicate if buffer data is valid
    static int numNoData; // Counter for multiple invalid data values in a row
    //    GwApi::BoatValue currValue; // temporary boat value to display current data buffer value

    int x, y; // x and y coordinates for drawing
    static int prevX, prevY; // Last x and y coordinates for drawing

    // Identify buffer size and buffer start position for chart
    count = dataBuf.getCurrentSize();
    currIdx = dataBuf.getLastIdx();
    numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display
    if (chrtIntv != oldChrtIntv || count == 1) {
        // new data interval selected by user; this is only x * 230 values instead of 240 seconds (4 minutes) per interval step
        intvBufSize = timAxis * chrtIntv;
        numBufVals = min(count, (timAxis - 60) * chrtIntv);
        bufStart = max(0, count - numBufVals);
        lastAddedIdx = currIdx;
        oldChrtIntv = chrtIntv;
    } else {
        numBufVals = numBufVals + numAddedBufVals;
        lastAddedIdx = currIdx;
        if (count == bufSize) {
            bufStart = max(0, bufStart - numAddedBufVals);
        }
    }

    calcChrtRng();
    chrtScl = float(valAxis) / float(chrtRng); // Chart scale: pixels per value step

    // Do we have valid buffer data?
    if (dataBuf.getMax() == dbMAX_VAL) {
        // only <MAX_VAL> values in buffer -> no valid wind data available
        bufDataValid = false;
    } else if (!currValue.valid && !useSimuData) {
        // currently no valid boat data available and no simulation mode
        numNoData++;
        bufDataValid = true;
        if (numNoData > 3) {
            // If more than 4 invalid values in a row, send message
            bufDataValid = false;
        }
    } else {
        numNoData = 0; // reset data error counter
        bufDataValid = true; // At least some wind data available
    }

    // Draw wind values in chart
    //***********************************************************************
    if (bufDataValid) {
        for (int i = 0; i < (numBufVals / chrtIntv); i++) {
            chrtVal = static_cast<int>(dataBuf.get(bufStart + (i * chrtIntv))); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
            if (chrtVal == dbMAX_VAL) {
                chrtPrevVal = dbMAX_VAL;
            } else {
                chrtVal = static_cast<int>((chrtVal / 1000.0) + 0.5); // Convert to real value and round
                if (chrtDir == 0) { // horizontal chart
                    x = cStart.x + i; // Position in chart area
                    y = cStart.y + (chrtVal * chrtScl); // value
                } else { // vertical chart
                    x = cStart.x + (chrtVal * chrtScl); // value
                    y = cStart.y + timAxis - i; // Position in chart area
                }
                if (i >= (numBufVals / chrtIntv) - 10) // log chart data of 1 line (adjust for test purposes)
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %d, {x,y} {%d,%d}", i, chrtVal, x, y);

                if ((i == 0) || (chrtPrevVal == dbMAX_VAL)) {
                    // just a dot for 1st chart point or after some invalid values
                    prevX = x;
                    prevY = y;
                }
                // Draw line with 2 pixels width + make sure vertical lines are drawn correctly
                if (chrtDir == 0 || x == prevX) { // vertical line
                    getdisplay().drawLine(prevX, prevY, x, y, fgColor);
                    getdisplay().drawLine(prevX - 1, prevY, x - 1, y, fgColor);
                } else if (chrtDir == 1 || x != prevX) { // line with some horizontal trend -> normal state
                    getdisplay().drawLine(prevX, prevY, x, y, fgColor);
                    getdisplay().drawLine(prevX, prevY - 1, x, y - 1, fgColor);
                }
                chrtPrevVal = chrtVal;
                prevX = x;
                prevY = y;
            }
            // Reaching chart area bottom end
            if (i >= timAxis - 1) {
                oldChrtIntv = 0; // force reset of buffer start and number of values to show in next display loop
                break;
            }
        }

        //        drawChrtValAxis();

        // uses BoatValue temp variable <currValue> to format latest buffer value
        // doesn't work unfortunately when 'simulation data' is active, because OBP60Formatter generates own simulation value in that case
        uint16_t lastVal = dataBuf.getLast();
        currValue.value = lastVal / 1000.0;
        currValue.valid = (static_cast<int16_t>(lastVal) != dbMAX_VAL);
        LOG_DEBUG(GwLog::DEBUG, "Chart drawChrt: lastVal: %d, currValue-value: %.1f, Valid: %d, Name: %s, Address: %p", lastVal, currValue.value,
            currValue.valid, currValue.getName(), (void*)&currValue);
        prntCurrValue(&currValue, { x, y });

    } else {
        // No valid data available
        LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
        getdisplay().setFont(&Ubuntu_Bold10pt8b);
        int pX, pY;
        if (chrtDir == 0) {
            pX = dWidth / 2;
            pY = cStart.y + (valAxis / 2) - 10;
        } else {
            pX = valAxis / 2;
            pY = cStart.y + (timAxis / 2) - 10;
        }
        getdisplay().fillRect(pX - 33, pY - 10, 66, 24, commonData->bgcolor); // Clear area for message
        drawTextCenter(pX, pY, "No data");
    }

    drawChrtValAxis();
}

// Print current data value
template <typename T>
void Chart<T>::prntCurrValue(GwApi::BoatValue* currValue, const Pos chrtPos)
{
    int currentZone;
    static int lastZone = 0;
    static bool flipVal = false;
    int xPosVal;
    static const int yPosVal = (chrtDir == 0) ? cStart.y + valAxis - 5 : cStart.y + timAxis - 5;

    // flexible move of location for latest boat data value, in case chart data is printed at the current location
    /*    xPosVal = flipVal ? 8 : valAxis - 135;
        currentZone = (chrtPos.y >= yPosVal - 32) && (chrtPos.y <= yPosVal + 6) && (chrtPos.x >= xPosVal - 4) && (chrtPos.x <= xPosVal + 146) ? 1 : 0; // Define current zone for data value
        if (currentZone != lastZone) {
            // Only flip when x moves to a different zone
            if ((chrtPos.y >= yPosVal - 32) && (chrtPos.y <= yPosVal + 6) && (chrtPos.x >= xPosVal - 3) && (chrtPos.x <= xPosVal + 146)) {
                flipVal = !flipVal;
                xPosVal = flipVal ? 8 : valAxis - 135;
            }
        }
        lastZone = currentZone; */

    xPosVal = (chrtDir == 0) ? cStart.x + timAxis - 117 : cStart.x + valAxis - 117;
    FormattedData frmtDbData = formatValue(currValue, *commonData);
    double testdbValue = frmtDbData.value;
    String sdbValue = frmtDbData.svalue; // value (string)
    String dbUnit = frmtDbData.unit; // Unit of value
    LOG_DEBUG(GwLog::DEBUG, "Chart CurrValue: dbValue: %.2f, sdbValue: %s, fmrtDbValue: %.2f, dbFormat: %s, dbUnit: %s, Valid: %d, Name: %s, Address: %p", currValue->value, sdbValue,
        testdbValue, currValue->getFormat(), dbUnit, currValue->valid, currValue->getName(), (void*)currValue);
    getdisplay().fillRect(xPosVal - 3, yPosVal - 34, 118, 40, bgColor); // Clear area for TWS value
    getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
    getdisplay().setCursor(xPosVal, yPosVal);
    if (useSimuData)
        getdisplay().printf("%2.1f", currValue->value); // Value
    else
        getdisplay().print(sdbValue); // Value
    //    getdisplay().setFont(&Ubuntu_Bold12pt8b);
    //    getdisplay().setCursor(xPosVal + 76, yPosVal - 14);
    //    getdisplay().print(dbName); // Name
    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal + 1);
    getdisplay().print(dbUnit); // Unit
}

// check and adjust chart range
template <typename T>
void Chart<T>::calcChrtRng()
{
    int diffRng;

    diffRng = dataBuf.getMax(numBufVals) / 1000;
    if (diffRng > chrtRng) {
        chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 value
    } else if (diffRng + 10 < chrtRng) { // Reduce chart range for higher resolution if possible
        chrtRng = max(dfltRng, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10);
    }
    LOG_DEBUG(GwLog::DEBUG, "Chart Range: diffRng: %d, chrtRng: %d, Min: %.0f, Max: %.0f", diffRng, chrtRng, dataBuf.getMin(numBufVals) / 1000, dataBuf.getMax(numBufVals) / 1000);
}

// Explicitly instantiate class with required data types to avoid linker errors
template class Chart<uint16_t>;
template class Chart<int16_t>;
// --- Class Chart ---------------
