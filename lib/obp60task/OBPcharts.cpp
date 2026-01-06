// Function lib for display of boat data in various chart formats
#include "OBPcharts.h"
// #include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPRingBuffer.h"

std::map<String, ChartProps> Chart::dfltChrtDta = {
    { "formatWind", { 60.0 * DEG_TO_RAD, 10.0 * DEG_TO_RAD } }, // default course range 60 degrees
    { "formatCourse", { 60.0 * DEG_TO_RAD, 10.0 * DEG_TO_RAD } }, // default course range 60 degrees
    //{ "formatKnots", { 7.71, 2.57 } }, // default speed range in m/s
    { "formatKnots", { 7.71, 2.56 } }, // default speed range in m/s
    { "formatDepth", { 15.0, 5.0 } }, // default depth range in m
    { "kelvinToC", { 30.0, 5.0 } } // default temp range in °C/K
};

// --- Class Chart ---------------

// Chart - object holding the actual chart, incl. data buffer and format definition
// Parameters: <chrtDir> chart timeline direction: 'H' = horizontal, 'V' = vertical;
//             <chrtSz> chart size: [0] = full size, [1] = half size left/top, [2] half size right/bottom;
//             <dfltRng> default range of chart, e.g. 30 = [0..30];
//             <common> common program data; required for logger and color data
//             <useSimuData> flag to indicate if simulation data is active
// Chart::Chart(RingBuffer<uint16_t>& dataBuf, char chrtDir, int8_t chrtSz, double dfltRng, CommonData& common, bool useSimuData)
Chart::Chart(RingBuffer<uint16_t>& dataBuf, double dfltRng, CommonData& common, bool useSimuData)
    : dataBuf(dataBuf)
    //, chrtDir(chrtDir)
    //, chrtSz(chrtSz)
    , dfltRng(dfltRng)
    , commonData(&common)
    , useSimuData(useSimuData)
{
    logger = commonData->logger;
    fgColor = commonData->fgcolor;
    bgColor = commonData->bgcolor;

    dWidth = getdisplay().width();
    dHeight = getdisplay().height();

    /*    if (chrtDir == 'H') {
            // horizontal chart timeline direction
            timAxis = dWidth - 1;
            switch (chrtSz) {
            case 0:
                valAxis = dHeight - top - bottom;
                cRoot = { 0, top - 1 };
                break;
            case 1:
                valAxis = (dHeight - top - bottom) / 2 - hGap;
                cRoot = { 0, top - 1 };
                break;
            case 2:
                valAxis = (dHeight - top - bottom) / 2 - hGap;
                cRoot = { 0, top + (valAxis + hGap) + hGap - 1 };
                break;
            default:
                LOG_DEBUG(GwLog::ERROR, "obp60:Chart %s: wrong init parameter", dataBuf.getName());
                return;
            }

        } else if (chrtDir == 'V') {
            // vertical chart timeline direction
            timAxis = dHeight - top - bottom;
            switch (chrtSz) {
            case 0:
                valAxis = dWidth - 1;
                cRoot = { 0, top - 1 };
                break;
            case 1:
                valAxis = dWidth / 2 - vGap;
                cRoot = { 0, top - 1 };
                break;
            case 2:
                valAxis = dWidth / 2 - vGap;
                cRoot = { dWidth / 2 + vGap - 1, top - 1 };
                break;
            default:
                LOG_DEBUG(GwLog::ERROR, "obp60:Chart %s: wrong init parameter", dataBuf.getName());
                return;
            }
        } else {
            LOG_DEBUG(GwLog::ERROR, "obp60:Chart %s: wrong init parameter", dataBuf.getName());
            return;
        } */

    dataBuf.getMetaData(dbName, dbFormat);
    dbMIN_VAL = dataBuf.getMinVal();
    dbMAX_VAL = dataBuf.getMaxVal();
    bufSize = dataBuf.getCapacity();

    // Initialize chart data format; shorter version of standard format indicator
    if (dbFormat == "formatCourse" || dbFormat == "formatWind" || dbFormat == "formatRot") {
        chrtDataFmt = 'W'; // Chart is showing data of course / wind <degree> format
    } else if (dbFormat == "formatRot") {
        chrtDataFmt = 'R'; // Chart is showing data of rotational <degree> format
    } else if (dbFormat == "formatKnots") {
        chrtDataFmt = 'S'; // Chart is showing data of speed or windspeed format
    } else if (dbFormat == "formatDepth") {
        chrtDataFmt = 'D'; // Chart ist showing data of <depth> format
    } else if (dbFormat == "kelvinToC") {
        chrtDataFmt = 'T'; // Chart ist showing data of <temp> format
    } else {
        chrtDataFmt = 'O'; // Chart is showing any other data format
    }

    // "0" value is the same for any data format but for user defined temperature format
    zeroValue = 0.0;
    if (chrtDataFmt == 'T') {
        tempFormat = commonData->config->getString(commonData->config->tempFormat); // [K|°C|°F]
        if (tempFormat == "K") {
            zeroValue = 0.0;
        } else if (tempFormat == "C") {
            zeroValue = 273.15;
        } else if (tempFormat == "F") {
            zeroValue = 255.37;
        }
    }

    // Read default range and range step for this chart type
    if (dfltChrtDta.count(dbFormat)) {
        dfltRng = dfltChrtDta[dbFormat].range;
        rngStep = dfltChrtDta[dbFormat].step;
    } else {
        dfltRng = 15.0;
        rngStep = 5.0;
    }

    // Initialize chart range values
    chrtMin = zeroValue;
    chrtMax = chrtMin + dfltRng;
    chrtMid = (chrtMin + chrtMax) / 2;
    chrtRng = dfltRng;
    recalcRngCntr = true; // initialize <chrtMid> and chart borders on first screen call

    LOG_DEBUG(GwLog::DEBUG, "Chart Init: dWidth: %d, dHeight: %d, timAxis: %d, valAxis: %d, cRoot {x,y}: %d, %d, dbname: %s, rngStep: %.4f, chrtDataFmt: %c",
        dWidth, dHeight, timAxis, valAxis, cRoot.x, cRoot.y, dbName, rngStep, chrtDataFmt);
};

Chart::~Chart()
{
}

// Perform all actions to draw chart
// Parameters: <chrtDir>:       chart timeline direction: 'H' = horizontal, 'V' = vertical
//             <chrtSz>:        chart size: [0] = full size, [1] = half size left/top, [2] half size right/bottom
//             <chrtIntv>:      chart timeline interval
//             <showCurrValue>: current boat data shall be shown [true/false]
//             <currValue>:     current boat data value to be printed
// void Chart::showChrt(GwApi::BoatValue currValue, int8_t& chrtIntv, const bool showCurrValue)
void Chart::showChrt(char chrtDir, int8_t chrtSz, int8_t& chrtIntv, bool showCurrValue, GwApi::BoatValue currValue)
{
    // this->chrtDir = chrtDir;
    //  this->chrtSz = chrtSz;

    if (!setChartDimensions(chrtDir, chrtSz)) {
        return; // wrong chart dimension parameters
    }

    drawChrt(chrtDir, chrtIntv, currValue);
    drawChrtTimeAxis(chrtDir, chrtSz, chrtIntv);
    drawChrtValAxis(chrtDir, chrtSz);

    if (!bufDataValid) { // No valid data available
        prntNoValidData(chrtDir);
        return;
    }

    if (showCurrValue) { // show latest value from history buffer; usually this should be the most current one
        currValue.value = dataBuf.getLast();
        currValue.valid = currValue.value != dbMAX_VAL;
        prntCurrValue(chrtDir, currValue);
    }
}

// define dimensions and start points for chart
bool Chart::setChartDimensions(const char direction, const int8_t size)
{
    if ((direction != 'H' && direction != 'V') || (size < 0 || size > 2)) {
        LOG_DEBUG(GwLog::ERROR, "obp60:setChartDimensions %s: wrong parameters", dataBuf.getName());
        return false;
    }

    if (direction == 'H') {
        // horizontal chart timeline direction
        timAxis = dWidth - 1;
        switch (size) {
        case 0:
            valAxis = dHeight - top - bottom;
            cRoot = { 0, top - 1 };
            break;
        case 1:
            valAxis = (dHeight - top - bottom) / 2 - hGap;
            cRoot = { 0, top - 1 };
            break;
        case 2:
            valAxis = (dHeight - top - bottom) / 2 - hGap;
            cRoot = { 0, top + (valAxis + hGap) + hGap - 1 };
            break;
        }

    } else if (direction == 'V') {
        // vertical chart timeline direction
        timAxis = dHeight - top - bottom;
        switch (size) {
        case 0:
            valAxis = dWidth - 1;
            cRoot = { 0, top - 1 };
            break;
        case 1:
            valAxis = dWidth / 2 - vGap;
            cRoot = { 0, top - 1 };
            break;
        case 2:
            valAxis = dWidth / 2 - vGap;
            cRoot = { dWidth / 2 + vGap - 1, top - 1 };
            break;
        }
    }
    LOG_DEBUG(GwLog::ERROR, "obp60:setChartDimensions %s: direction: %c, size: %d, dWidth: %d, dHeight: %d, timAxis: %d, valAxis: %d, cRoot{%d, %d}, top: %d, bottom: %d, hGap: %d, vGap: %d",
        dataBuf.getName(), direction, size, dWidth, dHeight, timAxis, valAxis, cRoot.x, cRoot.y, top, bottom, hGap, vGap);
    return true;
}

// draw chart
void Chart::drawChrt(const char chrtDir, int8_t& chrtIntv, GwApi::BoatValue& currValue)
{
    double chrtVal; // Current data value
    double chrtScl; // Scale for data values in pixels per value

    int x, y; // x and y coordinates for drawing

    getBufStartNSize(chrtIntv);

    // LOG_DEBUG(GwLog::DEBUG, "Chart:drawChart: min: %.1f, mid: %.1f, max: %.1f, rng: %.1f", chrtMin, chrtMid, chrtMax, chrtRng);
    calcChrtBorders(chrtMin, chrtMid, chrtMax, chrtRng);
    LOG_DEBUG(GwLog::DEBUG, "Chart:drawChart2: min: %.1f, mid: %.1f, max: %.1f, rng: %.1f", chrtMin, chrtMid, chrtMax, chrtRng);

    // Chart scale: pixels per value step
    chrtScl = double(valAxis) / chrtRng;

    // Do we have valid buffer data?
    if (dataBuf.getMax() == dbMAX_VAL) { // only <MAX_VAL> values in buffer -> no valid wind data available
        bufDataValid = false;
    } else if (!currValue.valid && !useSimuData) { // currently no valid boat data available and no simulation mode
        numNoData++;
        bufDataValid = true;
        if (numNoData > THRESHOLD_NO_DATA) { // If more than 4 invalid values in a row, send message
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
            chrtVal = dataBuf.get(bufStart + (i * chrtIntv)); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
            if (chrtVal == dbMAX_VAL) {
                chrtPrevVal = dbMAX_VAL;
            } else {

                if (chrtDir == 'H') { // horizontal chart
                    x = cRoot.x + i; // Position in chart area

                    if (chrtDataFmt == 'S' or chrtDataFmt == 'T') { // speed or temperature data format -> print low values at bottom
                        y = cRoot.y + valAxis - static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else if (chrtDataFmt == 'W' || chrtDataFmt == 'R') { // degree type value
                        y = cRoot.y + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else { // any other data format
                        y = cRoot.y + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    }

                } else { // vertical chart
                    y = cRoot.y + timAxis - i; // Position in chart area

                    // if (chrtDataFmt == 'S' || chrtDataFmt == 'D' || chrtDataFmt == 'T') {
                    if (chrtDataFmt == 'W' || chrtDataFmt == 'R') { // degree type value
                        x = cRoot.x + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else {
                        x = cRoot.x + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    }
                }

                // if (i >= (numBufVals / chrtIntv) - 5) // log chart data of 1 line (adjust for test purposes)
                //    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %.2f, chrtMin: %.2f, {x,y} {%d,%d}", i, chrtVal, chrtMin, x, y);

                if ((i == 0) || (chrtPrevVal == dbMAX_VAL)) {
                    // just a dot for 1st chart point or after some invalid values
                    prevX = x;
                    prevY = y;

                } else if (chrtDataFmt == 'W' || chrtDataFmt == 'R') {
                    // cross borders check for degree values; shift values to [-PI..0..PI]; when crossing borders, range is 2x PI degrees
                    double normCurr = WindUtils::to2PI(chrtVal - chrtMin);
                    double normPrev = WindUtils::to2PI(chrtPrevVal - chrtMin);
                    // Check if pixel positions are far apart (crossing chart boundary); happens when one value is near chrtMax and the other near chrtMin
                    bool crossedBorders = std::abs(normCurr - normPrev) > (chrtRng / 2.0);

                    if (crossedBorders) { // If current value crosses chart borders compared to previous value, split line
                        // LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: crossedBorders: %d, chrtVal: %.2f, chrtPrevVal: %.2f", crossedBorders, chrtVal, chrtPrevVal);
                        bool wrappingFromHighToLow = normCurr < normPrev; // Determine which edge we're crossing
                        if (chrtDir == 'H') {
                            int ySplit = wrappingFromHighToLow ? (cRoot.y + valAxis) : cRoot.y;
                            drawBoldLine(prevX, prevY, x, ySplit);
                            prevY = wrappingFromHighToLow ? cRoot.y : (cRoot.y + valAxis);
                        } else { // vertical chart
                            int xSplit = wrappingFromHighToLow ? (cRoot.x + valAxis) : cRoot.x;
                            drawBoldLine(prevX, prevY, xSplit, y);
                            prevX = wrappingFromHighToLow ? cRoot.x : (cRoot.x + valAxis);
                        }
                    }
                }

                if (chrtDataFmt == 'D') {
                    if (chrtDir == 'H') { // horizontal chart
                        drawBoldLine(x, y, x, cRoot.y + valAxis);
                    } else { // vertical chart
                        drawBoldLine(x, y, cRoot.x + valAxis, y);
                    }
                } else {
                    drawBoldLine(prevX, prevY, x, y);
                }

                /*                if (chrtDir == 'H' || x == prevX) { // horizontal chart & vertical line
                                    if (chrtDataFmt == 'D') {
                                        drawBoldLine(x, y, x, cRoot.y + valAxis);
                                    }
                                    drawBoldLine(prevX, prevY, x, y);
                                } else if (chrtDir == 'V' || x != prevX) { // vertical chart & line with some horizontal trend -> normal state
                                    if (chrtDataFmt == 'D') {
                                        drawBoldLine(x, y, cRoot.x + valAxis, y);
                                    }
                                    drawBoldLine(prevX, prevY, x, y);
                                } */
                chrtPrevVal = chrtVal;
                prevX = x;
                prevY = y;
            }

            // Reaching chart area top end
            if (i >= timAxis - 1) {
                oldChrtIntv = 0; // force reset of buffer start and number of values to show in next display loop

                if (chrtDataFmt == 'W') { // degree of course or wind
                    recalcRngCntr = true;
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: chart end: timAxis: %d, i: %d, bufStart: %d, numBufVals: %d, recalcRngCntr: %d", timAxis, i, bufStart, numBufVals, recalcRngCntr);
                }
                break;
            }
        }
    }
}

// Identify buffer size and buffer start position for chart
void Chart::getBufStartNSize(int8_t& chrtIntv)
{
    count = dataBuf.getCurrentSize();
    currIdx = dataBuf.getLastIdx();
    numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display

    if (chrtIntv != oldChrtIntv || count == 1) {
        // new data interval selected by user; this is only x * 230 values instead of 240 seconds (4 minutes) per interval step
        numBufVals = min(count, (timAxis - MIN_FREE_VALUES) * chrtIntv); // keep free or release MIN_FREE_VALUES on chart for plotting of new values
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
}

// check and adjust chart range and set range borders and range middle
void Chart::calcChrtBorders(double& rngMin, double& rngMid, double& rngMax, double& rng)
{
    if (chrtDataFmt == 'W' || chrtDataFmt == 'R') {
        // Chart data is of type 'course', 'wind' or 'rot'

        if (chrtDataFmt == 'W') {
            // Chart data is of type 'course' or 'wind'

            if ((count == 1 && rngMid == 0) || rngMid == dbMAX_VAL) {
                recalcRngCntr = true; // initialize <rngMid>
            }

            // Set rngMid
            if (recalcRngCntr) {
                rngMid = dataBuf.getMid(numBufVals);
                if (rngMid == dbMAX_VAL) {
                    rngMid = 0;
                } else {
                    rngMid = std::round(rngMid / rngStep) * rngStep; // Set new center value; round to next <rngStep> value

                    // Check if range between  'min' and 'max' is > 180° or crosses '0'
                    rngMin = dataBuf.getMin(numBufVals);
                    rngMax = dataBuf.getMax(numBufVals);
                    rng = (rngMax >= rngMin ? rngMax - rngMin : M_TWOPI - rngMin + rngMax);
                    rng = max(rng, dfltRng); // keep at least default chart range
                    if (rng > M_PI) { // If wind range > 180°, adjust wndCenter to smaller wind range end
                        rngMid = WindUtils::to2PI(rngMid + M_PI);
                    }
                }
                recalcRngCntr = false; // Reset flag for <rngMid> determination
                LOG_DEBUG(GwLog::DEBUG, "calcChrtRange: rngMid: %.1f°, rngMin: %.1f°, rngMax: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMid * RAD_TO_DEG, rngMin * RAD_TO_DEG, rngMax * RAD_TO_DEG,
                    rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);
            }

        } else if (chrtDataFmt == 'R') {
            // Chart data is of type 'rotation'; then we want to have <rndMid> always to be '0'
            rngMid = 0;
        }

        // check and adjust range between left, center, and right chart limit
        double halfRng = rng / 2.0; // we calculate with range between <rngMid> and edges
        double diffRng = getAngleRng(rngMid, numBufVals);
        diffRng = (diffRng == dbMAX_VAL ? 0 : std::ceil(diffRng / rngStep) * rngStep);
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtBorders: diffRng: %.1f°, halfRng: %.1f°", diffRng * RAD_TO_DEG, halfRng * RAD_TO_DEG);

        if (diffRng > halfRng) {
            halfRng = diffRng; // round to next <rngStep> value
        } else if (diffRng + rngStep < halfRng) { // Reduce chart range for higher resolution if possible
            halfRng = max(dfltRng / 2.0, diffRng);
        }

        rngMin = WindUtils::to2PI(rngMid - halfRng);
        rngMax = (halfRng < M_PI ? rngMid + halfRng : rngMid + halfRng - (M_TWOPI / 360)); // if chart range is 360°, then make <rngMax> 1° smaller than <rngMin>
        rngMax = WindUtils::to2PI(rngMax);

        rng = halfRng * 2.0;
        LOG_DEBUG(GwLog::DEBUG, "calcChrtBorders: rngMin: %.1f°, rngMid: %.1f°, rngMax: %.1f°, diffRng: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMin * RAD_TO_DEG, rngMid * RAD_TO_DEG, rngMax * RAD_TO_DEG,
            diffRng * RAD_TO_DEG, rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);

    } else { // chart data is of any other type

        double oldRngMin = rngMin;
        double oldRngMax = rngMax;

        double currMinVal = dataBuf.getMin(numBufVals);
        double currMaxVal = dataBuf.getMax(numBufVals);

        if (currMinVal == dbMAX_VAL || currMaxVal == dbMAX_VAL) {
            return; // no valid data
        }

        // check if current chart border have to be adjusted
        if (currMinVal < oldRngMin || (currMinVal > (oldRngMin + rngStep))) { // decrease rngMin if required or increase if lowest value is higher than old rngMin
            rngMin = std::floor(currMinVal / rngStep) * rngStep; // align low range to lowest buffer value and nearest range interval
        }
        if ((currMaxVal > oldRngMax) || (currMaxVal < (oldRngMax - rngStep))) { // increase rngMax if required or decrease if lowest value is lower than old rngMax
            rngMax = std::ceil(currMaxVal / rngStep) * rngStep;
        }

        // Chart range starts at least at '0' if minimum data value allows it
        if (rngMin > zeroValue && dbMIN_VAL <= zeroValue) {
            rngMin = zeroValue;
        }

        // ensure minimum chart range in user format
        if ((rngMax - rngMin) < dfltRng) {
            rngMax = rngMin + dfltRng;
        }

        rngMid = (rngMin + rngMax) / 2.0;
        rng = rngMax - rngMin;

        LOG_DEBUG(GwLog::DEBUG, "calcChrtRange-end: currMinVal: %.1f, currMaxVal: %.1f, rngMin: %.1f, rngMid: %.1f, rngMax: %.1f, rng: %.1f, rngStep: %.1f, zeroValue: %.1f, dbMIN_VAL: %.1f",
            currMinVal, currMaxVal, rngMin, rngMid, rngMax, rng, rngStep, zeroValue, dbMIN_VAL);
    }
}

// chart time axis label + lines
void Chart::drawChrtTimeAxis(const char chrtDir, const int8_t chrtSz, int8_t& chrtIntv)
{
    float axSlots, intv, i;
    char sTime[6];
    int timeRng = chrtIntv * 4; // chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [8] 32 min.

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setTextColor(fgColor);

    axSlots = 5; // number of axis labels
    intv = timAxis / (axSlots - 1); // minutes per chart axis interval (interval is 1 less than axSlots)
    i = timeRng; // Chart axis label start at -32, -16, -12, ... minutes

    if (chrtDir == 'H') { // horizontal chart
        getdisplay().fillRect(0, cRoot.y, dWidth, 2, fgColor);

        for (float j = 0; j < timAxis - 1; j += intv) { // fill time axis with values but keep area free on right hand side for value label

            // draw text with appropriate offset
            int tOffset = j == 0 ? 13 : -4;
            snprintf(sTime, sizeof(sTime), "-%.0f", i);
            drawTextCenter(cRoot.x + j + tOffset, cRoot.y - 8, sTime);
            getdisplay().drawLine(cRoot.x + j, cRoot.y, cRoot.x + j, cRoot.y + 5, fgColor); // draw short vertical time mark

            i -= chrtIntv;
        }

    } else { // vertical chart

        for (float j = intv; j < timAxis - 1; j += intv) { // don't print time label at upper and lower end of time axis

            i -= chrtIntv; // we start not at top chart position
            snprintf(sTime, sizeof(sTime), "-%.0f", i);
            getdisplay().drawLine(cRoot.x, cRoot.y + j, cRoot.x + valAxis, cRoot.y + j, fgColor); // Grid line

            if (chrtSz == 0) { // full size chart
                getdisplay().fillRect(0, cRoot.y + j - 9, 32, 15, bgColor); // clear small area to remove potential chart lines
                getdisplay().setCursor((4 - strlen(sTime)) * 7, cRoot.y + j + 3); // time value; print left screen; value right-formated
                getdisplay().printf("%s", sTime); // Range value
            } else if (chrtSz == 2) { // half size chart; right side
                drawTextCenter(dWidth / 2, cRoot.y + j, sTime); // time value; print mid screen
            }
        }
    }
}

// chart value axis labels + lines
void Chart::drawChrtValAxis(const char chrtDir, const int8_t chrtSz)
{
    double axLabel;
    double cVal;
    // char sVal[6];

    getdisplay().setTextColor(fgColor);

    if (chrtDir == 'H') {

        // print buffer data name on right hand side of time axis (max. size 5 characters)
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        drawTextRalign(cRoot.x + timAxis, cRoot.y - 3, dbName.substring(0, 5));

        if (chrtSz == 0) { // full size chart

            if (chrtDataFmt == 'W') {
                prntHorizThreeValueAxisLabel(&Ubuntu_Bold12pt8b);
                return;
            }

            // for any other data formats print multiple axis value lines on full charts
            prntHorizMultiValueAxisLabel(&Ubuntu_Bold12pt8b);
            return;

        } else { // half size chart -> just print edge values + middle chart line
            LOG_DEBUG(GwLog::DEBUG, "Chart::drawChrtValAxis: chrtDataFmt: %c, chrtMin: %.2f, chrtMid: %.2f, chrtMax: %.2f", chrtDataFmt, chrtMin, chrtMid, chrtMax);

            prntHorizThreeValueAxisLabel(&Ubuntu_Bold10pt8b);
            return;
        }

    } else { // vertical chart
        char sVal[6];

        if (chrtSz == 0) { // full size chart
            getdisplay().setFont(&Ubuntu_Bold12pt8b); // use larger font
            drawTextRalign(cRoot.x + (valAxis * 0.42), cRoot.y - 2, dbName.substring(0, 6)); // print buffer data name (max. size 5 characters)
        } else {
            getdisplay().setFont(&Ubuntu_Bold10pt8b); // use smaller font
        }
        getdisplay().fillRect(cRoot.x, cRoot.y, valAxis, 2, fgColor); // top chart line

        cVal = chrtMin;
        cVal = convertValue(cVal, dbName, dbFormat, *commonData); // value (converted)
        if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
            cVal = chrtMin; // no value conversion
        }
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        getdisplay().setCursor(cRoot.x, cRoot.y - 2);
        getdisplay().printf("%s", sVal); // Range low end

        cVal = chrtMid;
        cVal = convertValue(cVal, dbName, dbFormat, *commonData); // value (converted)
        if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
            cVal = chrtMid; // no value conversion
        }
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        drawTextCenter(cRoot.x + (valAxis / 2), cRoot.y - 9, sVal); // Range mid end

        cVal = chrtMax;
        cVal = convertValue(cVal, dbName, dbFormat, *commonData); // value (converted)
        if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
            cVal = chrtMax; // no value conversion
        }
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        drawTextRalign(cRoot.x + valAxis - 2, cRoot.y - 2, sVal); // Range high end

        // draw vertical grid lines for each axis label
        for (int j = 0; j <= valAxis; j += (valAxis / 2)) {
            getdisplay().drawLine(cRoot.x + j, cRoot.y, cRoot.x + j, cRoot.y + timAxis, fgColor);
        }
    }
}

// Print current data value
void Chart::prntCurrValue(const char chrtDir, GwApi::BoatValue& currValue)
{
    const int xPosVal = (chrtDir == 'H') ? cRoot.x + (timAxis / 2) - 56 : cRoot.x + 32;
    const int yPosVal = (chrtDir == 'H') ? cRoot.y + valAxis - 7 : cRoot.y + timAxis - 7;

    FormattedData frmtDbData = formatValue(&currValue, *commonData);
    String sdbValue = frmtDbData.svalue; // value as formatted string
    String dbUnit = frmtDbData.unit; // Unit of value; limit length to 3 characters
    // LOG_DEBUG(GwLog::DEBUG, "Chart CurrValue: dbValue: %.2f, sdbValue: %s, dbFormat: %s, dbUnit: %s, Valid: %d, Name: %s, Address: %p", currValue.value, sdbValue,
    //   currValue.getFormat(), dbUnit, currValue.valid, currValue.getName(), currValue);

    getdisplay().fillRect(xPosVal - 1, yPosVal - 35, 128, 41, bgColor); // Clear area for TWS value
    getdisplay().drawRect(xPosVal, yPosVal - 34, 126, 40, fgColor); // Draw box for TWS value
    getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
    getdisplay().setCursor(xPosVal + 1, yPosVal);
    getdisplay().print(sdbValue); // value

    getdisplay().setFont(&Ubuntu_Bold10pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal - 17);
    getdisplay().print(dbName.substring(0, 3)); // Name, limited to 3 characters

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal + 0);
    getdisplay().print(dbUnit); // Unit
}

// print message for no valid data availabletemplate <typename T>
void Chart::prntNoValidData(const char chrtDir)
{
    int pX, pY;

    getdisplay().setFont(&Ubuntu_Bold10pt8b);

    if (chrtDir == 'H') {
        pX = cRoot.x + (timAxis / 2);
        pY = cRoot.y + (valAxis / 2) - 10;
    } else {
        pX = cRoot.x + (valAxis / 2);
        pY = cRoot.y + (timAxis / 2) - 10;
    }

    getdisplay().fillRect(pX - 37, pY - 10, 78, 24, bgColor); // Clear area for message
    drawTextCenter(pX, pY, "No data");

    LOG_DEBUG(GwLog::LOG, "Page chart <%s>: No valid data available", dbName);
}

// Get maximum difference of last <amount> of dataBuf ringbuffer values to center chart; for angle data only
double Chart::getAngleRng(double center, size_t amount)
{
    size_t count = dataBuf.getCurrentSize();

    if (dataBuf.isEmpty() || amount <= 0) {
        return dbMAX_VAL;
    }
    if (amount > count)
        amount = count;

    double value = 0;
    double range = 0;
    double maxRng = dbMIN_VAL;

    // Start from the newest value (last) and go backwards x times
    for (size_t i = 0; i < amount; i++) {
        value = dataBuf.get(count - 1 - i);

        if (value == dbMAX_VAL) {
            continue; // ignore invalid values
        }

        range = abs(fmod((value - center + (M_TWOPI + M_PI)), M_TWOPI) - M_PI);
        if (range > maxRng)
            maxRng = range;
    }

    if (maxRng > M_PI) {
        maxRng = M_PI;
    }

    return (maxRng != dbMIN_VAL ? maxRng : dbMAX_VAL); // Return range from <mid> to <max>
}

// print horizontal axis label with only three values: top, mid, and bottom
void Chart::prntHorizThreeValueAxisLabel(const GFXfont* font)
{
    double axLabel;
    double chrtMin, chrtMid, chrtMax;
    int xOffset, yOffset; // offset for text position of x axis label for different font sizes
    String sVal;

    if (font == &Ubuntu_Bold10pt8b) {
        xOffset = 39;
        yOffset = 15;
    } else if (font == &Ubuntu_Bold12pt8b) {
        xOffset = 51;
        yOffset = 18;
    }
    getdisplay().setFont(font);

    // convert & round chart bottom+top label to next range step
    chrtMin = convertValue(this->chrtMin, dbName, dbFormat, *commonData);
    chrtMid = convertValue(this->chrtMid, dbName, dbFormat, *commonData);
    chrtMax = convertValue(this->chrtMax, dbName, dbFormat, *commonData);
    chrtMin = std::round(chrtMin * 100.0) / 100.0;
    chrtMid = std::round(chrtMid * 100.0) / 100.0;
    chrtMax = std::round(chrtMax * 100.0) / 100.0;

    // print top axis label
    axLabel = (chrtDataFmt == 'S' || chrtDataFmt == 'T') ? chrtMax : chrtMin;
    sVal = formatLabel(axLabel);
    getdisplay().fillRect(cRoot.x, cRoot.y + 2, xOffset + 4, yOffset, bgColor); // Clear small area to remove potential chart lines
    drawTextRalign(cRoot.x + xOffset, cRoot.y + yOffset, sVal); // range value

    // print mid axis label
    axLabel = chrtMid;
    sVal = formatLabel(axLabel);
    getdisplay().fillRect(cRoot.x, cRoot.y + (valAxis / 2) - 8, xOffset + 4, 16, bgColor); // Clear small area to remove potential chart lines
    drawTextRalign(cRoot.x + xOffset, cRoot.y + (valAxis / 2) + 6, sVal); // range value
    getdisplay().drawLine(cRoot.x + xOffset + 4, cRoot.y + (valAxis / 2), cRoot.x + timAxis, cRoot.y + (valAxis / 2), fgColor);

    // print bottom axis label
    axLabel = (chrtDataFmt == 'S' || chrtDataFmt == 'T') ? chrtMin : chrtMax;
    sVal = formatLabel(axLabel);
    getdisplay().fillRect(cRoot.x, cRoot.y + valAxis - 16, xOffset + 3, 16, bgColor); // Clear small area to remove potential chart lines
    drawTextRalign(cRoot.x + xOffset, cRoot.y + valAxis, sVal); // range value
    getdisplay().drawLine(cRoot.x + xOffset + 2, cRoot.y + valAxis, cRoot.x + timAxis, cRoot.y + valAxis, fgColor);
}

// print horizontal axis label with multiple axis lines
void Chart::prntHorizMultiValueAxisLabel(const GFXfont* font)
{
    double chrtMin, chrtMax, chrtRng;
    double axSlots, axIntv, axLabel;
    int xOffset; // offset for text position of x axis label for different font sizes
    String sVal;

    if (font == &Ubuntu_Bold10pt8b) {
        xOffset = 38;
    } else if (font == &Ubuntu_Bold12pt8b) {
        xOffset = 50;
    }
    getdisplay().setFont(font);

    chrtMin = convertValue(this->chrtMin, dbName, dbFormat, *commonData);
    // chrtMin = std::floor(chrtMin / rngStep) * rngStep;
    chrtMin = std::round(chrtMin * 100.0) / 100.0;
    chrtMax = convertValue(this->chrtMax, dbName, dbFormat, *commonData);
    // chrtMax = std::ceil(chrtMax / rngStep) * rngStep;
    chrtMax = std::round(chrtMax * 100.0) / 100.0;
    chrtRng = std::round((chrtMax - chrtMin) * 100) / 100;

    axSlots = valAxis / static_cast<double>(VALAXIS_STEP); // number of axis labels (and we want to have a double calculation, no integer)
    axIntv = chrtRng / axSlots;
    axLabel = chrtMin + axIntv;
    LOG_DEBUG(GwLog::DEBUG, "Chart::printHorizMultiValueAxisLabel: chrtRng: %.2f, th-chrtRng: %.2f, axSlots: %.2f, axIntv: %.2f, axLabel: %.2f, chrtMin: %.2f, chrtMid: %.2f, chrtMax: %.2f", chrtRng, this->chrtRng, axSlots, axIntv, axLabel, this->chrtMin, chrtMid, chrtMax);

    int loopStrt, loopEnd, loopStp;
    if (chrtDataFmt == 'S' || chrtDataFmt == 'T' || chrtDataFmt == 'O') {
        // High value at top
        loopStrt = valAxis - VALAXIS_STEP;
        loopEnd = VALAXIS_STEP / 2;
        loopStp = VALAXIS_STEP * -1;
    } else {
        // Low value at top
        loopStrt = VALAXIS_STEP;
        loopEnd = valAxis - (VALAXIS_STEP / 2);
        loopStp = VALAXIS_STEP;
    }

    for (int j = loopStrt; (loopStp > 0) ? (j < loopEnd) : (j > loopEnd); j += loopStp) {
        sVal = formatLabel(axLabel);
        // sVal = convNformatLabel(axLabel);
        getdisplay().fillRect(cRoot.x, cRoot.y + j - 11, xOffset + 4, 21, bgColor); // Clear small area to remove potential chart lines
        drawTextRalign(cRoot.x + xOffset, cRoot.y + j + 7, sVal); // range value
        getdisplay().drawLine(cRoot.x + xOffset + 3, cRoot.y + j, cRoot.x + timAxis, cRoot.y + j, fgColor);

        axLabel += axIntv;
    }
}

// Draw chart line with thickness of 2px
void Chart::drawBoldLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{

    int16_t dx = std::abs(x2 - x1);
    int16_t dy = std::abs(y2 - y1);

    getdisplay().drawLine(x1, y1, x2, y2, fgColor);

    if (dx >= dy) { // line has horizontal tendency
        getdisplay().drawLine(x1, y1 - 1, x2, y2 - 1, fgColor);
    } else { // line has vertical tendency
        getdisplay().drawLine(x1 - 1, y1, x2 - 1, y2, fgColor);
    }
}

// Convert and format current axis label to user defined format; helper function for easier handling of OBP60Formatter
String Chart::convNformatLabel(double label)
{
    GwApi::BoatValue tmpBVal(dbName); // temporary boat value for string formatter
    String sVal;

    tmpBVal.setFormat(dbFormat);
    tmpBVal.valid = true;
    tmpBVal.value = label;
    sVal = formatValue(&tmpBVal, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
    if (sVal.length() > 0 && sVal[0] == '!') {
        sVal = sVal.substring(1); // cut leading "!" created at OBPFormatter for use with other font than 7SEG
    }

    return sVal;
}

// Format current axis label for printing w/o data format conversion (has been done earlier)
String Chart::formatLabel(const double& label)
{
    char sVal[11];

    if (dbFormat == "formatCourse" || dbFormat == "formatWind") {
        // Format 3 numbers with prefix zero
        snprintf(sVal, sizeof(sVal), "%03.0f", label);

    } else if (dbFormat == "formatRot") {
        if (label > -10 && label < 10) {
            snprintf(sVal, sizeof(sVal), "%3.2f", label);
        } else {
            snprintf(sVal, sizeof(sVal), "%3.0f", label);
        }
    }

    else {
        if (label < 10) {
            snprintf(sVal, sizeof(sVal), "%3.1f", label);
        } else {
            snprintf(sVal, sizeof(sVal), "%3.0f", label);
        }
    }

    return String(sVal);
}
// --- Class Chart ---------------
