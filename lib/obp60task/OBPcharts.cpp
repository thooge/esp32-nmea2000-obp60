// Function lib for display of boat data in various chart formats
#include "OBPcharts.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPRingBuffer.h"

// --- Class Chart ---------------

// Chart - object holding the actual chart, incl. data buffer and format definition
// Parameters: <chrtDir> chart timeline direction: 'H' = horizontal, 'V' = vertical;
//             <chrtSz> chart size: [0] = full size, [1] = half size left/top, [2] half size right/bottom;
//             <dfltRng> default range of chart, e.g. 30 = [0..30];
//             <common> common program data; required for logger and color data
//             <useSimuData> flag to indicate if simulation data is active
template <typename T>
Chart<T>::Chart(RingBuffer<T>& dataBuf, char chrtDir, int8_t chrtSz, double dfltRng, CommonData& common, bool useSimuData)
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

    // LOG_DEBUG(GwLog::DEBUG, "Chart Init: Chart::dataBuf: %p, passed dataBuf: %p", (void*)&this->dataBuf, (void*)&dataBuf);
    dWidth = getdisplay().width();
    dHeight = getdisplay().height();

    if (chrtDir == 'H') {
        // horizontal chart timeline direction
        timAxis = dWidth - 1;
        switch (chrtSz) {
        case 0:
            valAxis = dHeight - top - bottom;
            cStart = { 0, top - 1 };
            break;
        case 1:
            valAxis = (dHeight - top - bottom) / 2 - hGap;
            cStart = { 0, top - 1 };
            break;
        case 2:
            valAxis = (dHeight - top - bottom) / 2 - hGap;
            cStart = { 0, top + (valAxis + hGap) + hGap - 1 };
            break;
        default:
            LOG_DEBUG(GwLog::ERROR, "displayChart: wrong init parameter");
            return;
        }

    } else if (chrtDir == 'V') {
        // vertical chart timeline direction
        timAxis = dHeight - top - bottom;
        switch (chrtSz) {
        case 0:
            valAxis = dWidth - 1;
            cStart = { 0, top - 1 };
            break;
        case 1:
            //            valAxis = dWidth / 2 - vGap - 1;
            valAxis = dWidth / 2 - vGap;
            cStart = { 0, top - 1 };
            break;
        case 2:
            //            valAxis = dWidth / 2 - vGap - 1;
            valAxis = dWidth / 2 - vGap;
            cStart = { dWidth / 2 + vGap - 1, top - 1 };
            break;
        default:
            LOG_DEBUG(GwLog::ERROR, "displayChart: wrong init parameter");
            return;
        }
    } else {
        LOG_DEBUG(GwLog::ERROR, "displayChart: wrong init parameter");
        return;
    }

    dataBuf.getMetaData(dbName, dbFormat);
    dbMIN_VAL = dataBuf.getMinVal();
    dbMAX_VAL = dataBuf.getMaxVal();
    bufSize = dataBuf.getCapacity();

    if (dbFormat == "formatCourse" || dbFormat == "formatWind" || dbFormat == "formatRot") {

        if (dbFormat == "formatRot") {
            chrtDataFmt = 'R'; // Chart is showing data of rotational <degree> format
        } else {
            chrtDataFmt = 'W'; // Chart is showing data of course / wind <degree> format
        }
        rngStep = M_TWOPI / 360.0 * 10.0; // +/-10 degrees on each end of chrtMid; we are calculating with SI values

    } else {
        if (dbFormat == "formatDepth") {
            chrtDataFmt = 'D'; // Chart ist showing data of <depth> format
        } else {
            chrtDataFmt = 'S'; // Chart is showing any other data format than <degree>
        }
        rngStep = 5.0; // +/- 10 for all other values (eg. m/s, m, K, mBar)
    }

    chrtMin = 0;
    chrtMax = 0;
    chrtMid = dbMAX_VAL;
    chrtRng = dfltRng;
    recalcRngCntr = true; // initialize <chrtMid> on first screen call

    LOG_DEBUG(GwLog::DEBUG, "Chart Init: dWidth: %d, dHeight: %d, timAxis: %d, valAxis: %d, cStart {x,y}: %d, %d, dbname: %s, rngStep: %.4f", dWidth, dHeight, timAxis, valAxis, cStart.x, cStart.y, dbName, rngStep);
};

template <typename T>
Chart<T>::~Chart()
{
}

// Perform all actions to draw chart
// Parameters: <chrtIntv> chart time interval, <currValue> current boat data value to be printed, <showCurrValue> current boat data shall be shown yes/no
template <typename T>
void Chart<T>::showChrt(int8_t chrtIntv, GwApi::BoatValue currValue, bool showCurrValue)
{
    drawChrt(chrtIntv, currValue);
    drawChrtTimeAxis(chrtIntv);
    drawChrtValAxis();

    if (bufDataValid) {
        if (showCurrValue) {
            // uses BoatValue temp variable <currValue> to format latest buffer value
            // doesn't work unfortunately when 'simulation data' is active, because OBP60Formatter generates own simulation value in that case
            currValue.value = dataBuf.getLast();
            currValue.valid = currValue.value != dbMAX_VAL;
            Chart<T>::prntCurrValue(currValue);
            LOG_DEBUG(GwLog::DEBUG, "OBPcharts showChrt: currValue-value: %.1f, Valid: %d, Name: %s, Address: %p", currValue.value, currValue.valid, currValue.getName(), (void*)&currValue);
        }

    } else { // No valid data available -> print message
        getdisplay().setFont(&Ubuntu_Bold10pt8b);

        int pX, pY;
        if (chrtDir == 'H') {
            pX = cStart.x + (timAxis / 2);
            pY = cStart.y + (valAxis / 2) - 10;
        } else {
            pX = cStart.x + (valAxis / 2);
            pY = cStart.y + (timAxis / 2) - 10;
        }

        getdisplay().fillRect(pX - 37, pY - 10, 78, 24, bgColor); // Clear area for message
        drawTextCenter(pX, pY, "No data");
        LOG_DEBUG(GwLog::LOG, "Page chart: No valid data available");
    }
}

// draw chart
template <typename T>
void Chart<T>::drawChrt(int8_t chrtIntv, GwApi::BoatValue& currValue)
{
    double chrtVal; // Current data value
    double chrtScl; // Scale for data values in pixels per value
    static double chrtPrevVal; // Last data value in chart area
    static int numNoData; // Counter for multiple invalid data values in a row

    int x, y; // x and y coordinates for drawing
    static int prevX, prevY; // Last x and y coordinates for drawing

    // Identify buffer size and buffer start position for chart
    count = dataBuf.getCurrentSize();
    currIdx = dataBuf.getLastIdx();
    numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display

    if (chrtIntv != oldChrtIntv || count == 1) {
        // new data interval selected by user; this is only x * 230 values instead of 240 seconds (4 minutes) per interval step
        numBufVals = min(count, (timAxis - 60) * chrtIntv); // keep free or release 60 values on chart for plotting of new values
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

    calcChrtBorders(chrtMid, chrtMin, chrtMax, chrtRng);
    LOG_DEBUG(GwLog::DEBUG, "calcChrtBorders: min: %.3f, mid: %.3f, max: %.3f, rng: %.3f", chrtMin, chrtMid, chrtMax, chrtRng);

    chrtScl = double(valAxis) / chrtRng; // Chart scale: pixels per value step

    // Do we have valid buffer data?
    if (dataBuf.getMax() == dbMAX_VAL) { // only <MAX_VAL> values in buffer -> no valid wind data available
        bufDataValid = false;
    } else if (!currValue.valid && !useSimuData) { // currently no valid boat data available and no simulation mode
        numNoData++;
        bufDataValid = true;
        if (numNoData > 3) { // If more than 4 invalid values in a row, send message
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
                    x = cStart.x + i; // Position in chart area

                    if (chrtDataFmt == 'S') { // speed data format -> print low values at bottom
                        //                        y = cStart.y + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                        y = cStart.y + valAxis - static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else if (chrtDataFmt == 'D') {
                        y = cStart.y + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else { // degree type value
                        y = cStart.y + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    }

                } else { // vertical chart
                    y = cStart.y + timAxis - i; // Position in chart area

                    if (chrtDataFmt == 'S' || chrtDataFmt == 'D') {
                        x = cStart.x + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else { // degree type value
                        x = cStart.x + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    }
                }

                // if (i >= (numBufVals / chrtIntv) - 5) // log chart data of 1 line (adjust for test purposes)
                //    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %.4f, {x,y} {%d,%d}", i, chrtVal, x, y);

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
                            int ySplit = wrappingFromHighToLow ? (cStart.y + valAxis) : cStart.y;
                            getdisplay().drawLine(prevX, prevY, x, ySplit, fgColor);
                            if (x != prevX) { // line with some horizontal trend
                                getdisplay().drawLine(prevX, prevY - 1, x, ySplit - 1, fgColor);
                            } else {
                                getdisplay().drawLine(prevX, prevY - 1, x - 1, ySplit, fgColor);
                            }
                            prevY = wrappingFromHighToLow ? cStart.y : (cStart.y + valAxis);
                        } else { // vertical chart
                            int xSplit = wrappingFromHighToLow ? (cStart.x + valAxis) : cStart.x;
                            getdisplay().drawLine(prevX, prevY, xSplit, y, fgColor);
                            getdisplay().drawLine(prevX, prevY - 1, ((xSplit != prevX) ? xSplit : xSplit - 1), ((xSplit != prevX) ? y - 1 : y), fgColor);
                            prevX = wrappingFromHighToLow ? cStart.x : (cStart.x + valAxis);
                        }
                    }
                }

                // Draw line with 2 pixels width + make sure vertical lines are drawn correctly
                if (chrtDir == 'H' || x == prevX) { // horizontal chart & vertical line
                    if (chrtDataFmt == 'D') {
                        getdisplay().drawLine(x, y, x, cStart.y + valAxis, fgColor);
                        getdisplay().drawLine(x - 1, y, x - 1, cStart.y + valAxis, fgColor);
                    }
                    getdisplay().drawLine(prevX, prevY, x, y, fgColor);
                    getdisplay().drawLine(prevX - 1, prevY, x - 1, y, fgColor);
                } else if (chrtDir == 'V' || x != prevX) { // vertical chart & line with some horizontal trend -> normal state
                    if (chrtDataFmt == 'D') {
                        getdisplay().drawLine(x, y, cStart.x + valAxis, y, fgColor);
                        getdisplay().drawLine(x, y - 1, cStart.x + valAxis, y - 1, fgColor);
                    }
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

                if (chrtDataFmt == 'W') { // degree of course or wind
                    recalcRngCntr = true;
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: chart end: timAxis: %d, i: %d, bufStart: %d, numBufVals: %d, recalcRngCntr: %d", timAxis, i, bufStart, numBufVals, recalcRngCntr);
                }
                break;
            }
        }
    }
}

// Get maximum difference of last <amount> of dataBuf ringbuffer values to center chart
template <typename T>
double Chart<T>::getRng(double center, size_t amount)
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

// check and adjust chart range and set range borders and range middle
template <typename T>
void Chart<T>::calcChrtBorders(double& rngMid, double& rngMin, double& rngMax, double& rng)
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
                LOG_DEBUG(GwLog::DEBUG, "calcChrtRange1b: rngMid: %.1f°, rngMin: %.1f°, rngMax: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMid * RAD_TO_DEG, rngMin * RAD_TO_DEG, rngMax * RAD_TO_DEG,
                    rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);
            }

        } else if (chrtDataFmt == 'R') {
            // Chart data is of type 'rotation'; then we want to have <rndMid> always to be '0'
            rngMid = 0;
        }

        // check and adjust range between left, center, and right chart limit
        double halfRng = rng / 2.0; // we calculate with range between <rngMid> and edges
        double diffRng = getRng(rngMid, numBufVals);
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange2: diffRng: %.1f°, halfRng: %.1f°", diffRng * RAD_TO_DEG, halfRng * RAD_TO_DEG);
        diffRng = (diffRng == dbMAX_VAL ? 0 : std::ceil(diffRng / rngStep) * rngStep);
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange2: diffRng: %.1f°, halfRng: %.1f°", diffRng * RAD_TO_DEG, halfRng * RAD_TO_DEG);

        if (diffRng > halfRng) {
            halfRng = diffRng; // round to next <rngStep> value
        } else if (diffRng + rngStep < halfRng) { // Reduce chart range for higher resolution if possible
            halfRng = max(dfltRng / 2.0, diffRng);
        }

        rngMin = WindUtils::to2PI(rngMid - halfRng);
        rngMax = (halfRng < M_PI ? rngMid + halfRng : rngMid + halfRng - (M_TWOPI / 360)); // if chart range is 360°, then make <rngMax> 1° smaller than <rngMin>
        rngMax = WindUtils::to2PI(rngMax);
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange2: diffRng: %.1f°, halfRng: %.1f°", diffRng * RAD_TO_DEG, halfRng * RAD_TO_DEG);

        rng = halfRng * 2.0;
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange2b: rngMid: %.1f°, rngMin: %.1f°, rngMax: %.1f°, diffRng: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMid * RAD_TO_DEG, rngMin * RAD_TO_DEG, rngMax * RAD_TO_DEG,
        //    diffRng * RAD_TO_DEG, rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);

    } else {
        double oldRngMin = rngMin;
        double oldRngMax = rngMax;

        // Chart starts at lowest range value, but at least '0' or includes even negative values
        double currMinVal = dataBuf.getMin(numBufVals);
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange0a: currMinVal: %.1f, currMaxVal: %.1f, rngMin: %.1f, rngMid: %.1f, rngMax: %.1f, rng: %.1f, rngStep: %.1f, oldRngMin: %.1f, oldRngMax: %.1f, dfltRng: %.1f, numBufVals: %d",
        //     currMinVal, dataBuf.getMax(numBufVals), rngMin, rngMid, rngMax, rng, rngStep, oldRngMin, oldRngMax, dfltRng, numBufVals);

        if (currMinVal != dbMAX_VAL) { // current min value is valid
            if (currMinVal > 0 && dbMIN_VAL == 0) { // Chart range starts at least at '0' or includes negative values
                rngMin = 0;
            } else if (currMinVal < oldRngMin || (oldRngMin < 0 && (currMinVal > (oldRngMin + rngStep)))) { // decrease rngMin if required or increase if lowest value is higher than old rngMin
                rngMin = std::floor(currMinVal / rngStep) * rngStep;
            }
        } // otherwise keep rngMin unchanged

        double currMaxVal = dataBuf.getMax(numBufVals);
        if (currMaxVal != dbMAX_VAL) { // current max value is valid
            if ((currMaxVal > oldRngMax) || (currMaxVal < (oldRngMax - rngStep))) { // increase rngMax if required or decrease if lowest value is lower than old rngMax
                rngMax = std::ceil(currMaxVal / rngStep) * rngStep;
                rngMax = std::max(rngMax, rngMin + dfltRng); // keep at least default chart range
            }
        } // otherwise keep rngMax unchanged

        rngMid = (rngMin + rngMax) / 2.0;
        rng = rngMax - rngMin;
        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange1a: currMinVal: %.1f, currMaxVal: %.1f, rngMin: %.1f, rngMid: %.1f, rngMax: %.1f, rng: %.1f, rngStep: %.1f, oldRngMin: %.1f, oldRngMax: %.1f, dfltRng: %.1f, numBufVals: %d",
        //     currMinVal, currMaxVal, rngMin, rngMid, rngMax, rng, rngStep, oldRngMin, oldRngMax, dfltRng, numBufVals);
    }
}

// chart time axis label + lines
template <typename T>
void Chart<T>::drawChrtTimeAxis(int8_t chrtIntv)
{
    float slots, intv, i;
    char sTime[6];
    int timeRng = chrtIntv * 4; // chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [8] 32 min.

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setTextColor(fgColor);

    if (chrtDir == 'H') { // horizontal chart
        getdisplay().fillRect(0, cStart.y, dWidth, 2, fgColor);

        slots = 5; // number of axis labels
        intv = timAxis / (slots - 1); // minutes per chart axis interval (interval is 1 less than slots)
        i = timeRng; // Chart axis label start at -32, -16, -12, ... minutes

        for (float j = 0; j < timAxis - 1; j += intv) { // fill time axis with values but keep area free on right hand side for value label

            // draw text with appropriate offset
            int tOffset = j == 0 ? 13 : -4;
            snprintf(sTime, sizeof(sTime), "-%.0f", i);
            drawTextCenter(cStart.x + j + tOffset, cStart.y - 8, sTime);
            getdisplay().drawLine(cStart.x + j, cStart.y, cStart.x + j, cStart.y + 5, fgColor); // draw short vertical time mark

            i -= chrtIntv;
        }

    } else { // vertical chart
        slots = 5; // number of axis labels
        intv = timAxis / (slots - 1); // minutes per chart axis interval (interval is 1 less than slots)
        i = timeRng; // Chart axis label start at -32, -16, -12, ... minutes

        for (float j = intv; j < timAxis - 1; j += intv) { // don't print time label at upper and lower end of time axis

            i -= chrtIntv; // we start not at top chart position
            snprintf(sTime, sizeof(sTime), "-%.0f", i);
            getdisplay().drawLine(cStart.x, cStart.y + j, cStart.x + valAxis, cStart.y + j, fgColor); // Grid line

            if (chrtSz == 0) { // full size chart
                getdisplay().fillRect(0, cStart.y + j - 9, 32, 15, bgColor); // clear small area to remove potential chart lines
                getdisplay().setCursor((4 - strlen(sTime)) * 7, cStart.y + j + 3); // time value; print left screen; value right-formated
                getdisplay().printf("%s", sTime); // Range value
            } else if (chrtSz == 2) { // half size chart; right side
                drawTextCenter(dWidth / 2, cStart.y + j, sTime); // time value; print mid screen
            }
        }
    }
}

// chart value axis labels + lines
template <typename T>
void Chart<T>::drawChrtValAxis()
{
    double slots;
    int i, intv;
    double cVal, cChrtRng, crngMin;
    char sVal[6];
    int sLen;
    std::unique_ptr<GwApi::BoatValue> tmpBVal; // Temp variable to get formatted and converted data value from OBP60Formatter
    tmpBVal = std::unique_ptr<GwApi::BoatValue>(new GwApi::BoatValue(dataBuf.getName()));
    tmpBVal->setFormat(dataBuf.getFormat());
    tmpBVal->valid = true;

    if (chrtDir == 'H') {
        slots = valAxis / 60.0; // number of axis labels
        tmpBVal->value = chrtRng;
        cChrtRng = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        if (useSimuData) {
            // cannot use <formatValue> in this case, because that would change the range value to some random data
            cChrtRng = tmpBVal->value; // take SI value in this case -> need to be improved
        }
        intv = static_cast<int>(round(cChrtRng / slots));
        i = intv;

        if (chrtSz == 0) { // full size chart -> print multiple value lines
            getdisplay().setFont(&Ubuntu_Bold12pt8b);

            int loopStrt, loopEnd, loopStp;
            if (chrtDataFmt == 'S') {
                loopStrt = valAxis - 60;
                loopEnd = 30;
                loopStp = -60;
            } else {
                loopStrt = 60;
                loopEnd = valAxis - 30;
                loopStp = 60;
            }

            for (int j = loopStrt; (loopStp > 0) ? (j < loopEnd) : (j > loopEnd); j += loopStp) {
                getdisplay().drawLine(cStart.x, cStart.y + j, cStart.x + timAxis, cStart.y + j, fgColor);

                getdisplay().fillRect(cStart.x, cStart.y + j - 11, 42, 21, bgColor); // Clear small area to remove potential chart lines
                String sVal = String(i);
                getdisplay().setCursor((3 - sVal.length()) * 10, cStart.y + j + 7); // value right-formated
                getdisplay().printf("%s", sVal); // Range value

                i += intv;
            }
        } else { // half size chart -> print just edge values + middle chart line
            getdisplay().setFont(&Ubuntu_Bold10pt8b);

            tmpBVal->value = (chrtDataFmt == 'D') ? chrtMin : chrtMax;
            cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
            if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
                cVal = tmpBVal->value; // no value conversion here
            }
            sLen = snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
            getdisplay().fillRect(cStart.x, cStart.y + 2, 42, 16, bgColor); // Clear small area to remove potential chart lines
            getdisplay().setCursor(cStart.x + ((3 - sLen) * 10), cStart.y + 16);
            getdisplay().printf("%s", sVal); // Range low end

            tmpBVal->value = chrtMid;
            cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
            if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
                cVal = tmpBVal->value; // no value conversion here
            }
            sLen = snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
            getdisplay().fillRect(cStart.x, cStart.y + (valAxis / 2) - 9, 42, 16, bgColor); // Clear small area to remove potential chart lines
            getdisplay().setCursor(cStart.x + ((3 - sLen) * 10), cStart.y + (valAxis / 2) + 5);
            getdisplay().printf("%s", sVal); // Range mid value
            getdisplay().drawLine(cStart.x + 43, cStart.y + (valAxis / 2), cStart.x + timAxis, cStart.y + (valAxis / 2), fgColor);

            tmpBVal->value = (chrtDataFmt == 'D') ? chrtMax : chrtMin;
            cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
            if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
                cVal = tmpBVal->value; // no value conversion here
            }
            sLen = snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
            getdisplay().fillRect(cStart.x, cStart.y + valAxis - 16, 42, 16, bgColor); // Clear small area to remove potential chart lines
            getdisplay().setCursor(cStart.x + ((3 - sLen) * 10), cStart.y + valAxis - 1);
            getdisplay().printf("%s", sVal); // Range high end
            getdisplay().drawLine(cStart.x + 43, cStart.y + valAxis, cStart.x + timAxis, cStart.y + valAxis, fgColor);
        }

        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        drawTextRalign(cStart.x + timAxis, cStart.y - 3, dbName); // buffer data name

    } else { // vertical chart
        if (chrtSz == 0) { // full size chart -> use larger font
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            drawTextCenter(cStart.x + (valAxis / 4) + 25, cStart.y - 10, dbName); // buffer data name
        } else {
            getdisplay().setFont(&Ubuntu_Bold10pt8b);
        }
        getdisplay().fillRect(cStart.x, cStart.y, valAxis, 2, fgColor); // top chart line

        tmpBVal->value = chrtMin;
        cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
            cVal = tmpBVal->value; // no value conversion here
        }
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        getdisplay().setCursor(cStart.x, cStart.y - 2);
        getdisplay().printf("%s", sVal); // Range low end

        tmpBVal->value = chrtMid;
        cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
            cVal = tmpBVal->value; // no value conversion here
        }
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        drawTextCenter(cStart.x + (valAxis / 2), cStart.y - 10, sVal); // Range mid end

        tmpBVal->value = chrtMax;
        cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        if (useSimuData) { // dirty fix for problem that OBP60Formatter can only be used without data simulation -> returns random values in simulation mode
            cVal = tmpBVal->value; // no value conversion here
        }
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        drawTextRalign(cStart.x + valAxis - 2, cStart.y - 2, sVal); // Range high end

        for (int j = 0; j <= valAxis; j += (valAxis / 2)) {
            getdisplay().drawLine(cStart.x + j, cStart.y, cStart.x + j, cStart.y + timAxis, fgColor);
        }
    }
}

// Print current data value
template <typename T>
void Chart<T>::prntCurrValue(GwApi::BoatValue& currValue)
{
    const int xPosVal = (chrtDir == 'H') ? cStart.x + (timAxis / 2) - 56 : cStart.x + 32;
    const int yPosVal = (chrtDir == 'H') ? cStart.y + valAxis - 7 : cStart.y + timAxis - 7;

    FormattedData frmtDbData = formatValue(&currValue, *commonData);
    String sdbValue = frmtDbData.svalue; // value as formatted string
    String dbUnit = frmtDbData.unit; // Unit of value
    // LOG_DEBUG(GwLog::DEBUG, "Chart CurrValue: dbValue: %.2f, sdbValue: %s, dbFormat: %s, dbUnit: %s, Valid: %d, Name: %s, Address: %p", currValue.value, sdbValue,
    //    currValue.getFormat(), dbUnit, currValue.valid, currValue.getName(), currValue);

    getdisplay().fillRect(xPosVal - 1, yPosVal - 35, 128, 41, bgColor); // Clear area for TWS value
    getdisplay().drawRect(xPosVal, yPosVal - 34, 126, 40, fgColor); // Draw box for TWS value
    getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
    getdisplay().setCursor(xPosVal + 1, yPosVal);
    getdisplay().print(sdbValue); // alue

    getdisplay().setFont(&Ubuntu_Bold10pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal - 17);
    getdisplay().print(dbName); // Name

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal + 0);
    getdisplay().print(dbUnit); // Unit
}

// Explicitly instantiate class with required data types to avoid linker errors
template class Chart<uint16_t>;
// --- Class Chart ---------------
