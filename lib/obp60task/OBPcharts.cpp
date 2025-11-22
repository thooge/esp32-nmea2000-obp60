// Function lib for display of boat data in various chart formats
#include "OBPcharts.h"
#include "OBP60Extensions.h"
#include "OBPRingBuffer.h"

// --- Class Chart ---------------
template <typename T>
Chart<T>::Chart(RingBuffer<T>& dataBuf, int8_t chrtDir, int8_t chrtSz, double dfltRng, CommonData& common, bool useSimuData)
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

    LOG_DEBUG(GwLog::DEBUG, "Chart Init: dataBuf: %p", (void*)&dataBuf);
    dWidth = getdisplay().width();
    dHeight = getdisplay().height();

    if (chrtDir == 0) {
        // horizontal chart timeline direction
        timAxis = dWidth;
        switch (chrtSz) {
        case 0:
            valAxis = dHeight - top - bottom;
            cStart = { 0, top };
            break;
        case 1:
            valAxis = (dHeight - top - bottom) / 2 - gap;
            cStart = { 0, top };
            break;
        case 2:
            valAxis = (dHeight - top - bottom) / 2 - gap;
            cStart = { 0, top + (valAxis + gap) + gap };
            break;
        default:
            LOG_DEBUG(GwLog::ERROR, "displayChart: wrong init parameter");
            return;
        }
    } else if (chrtDir == 1) {
        // vertical chart timeline direction
        timAxis = dHeight - top - bottom;
        switch (chrtSz) {
        case 0:
            valAxis = dWidth;
            cStart = { 0, top };
            break;
        case 1:
            valAxis = dWidth / 2 - gap - 1;
            cStart = { 0, top };
            break;
        case 2:
            valAxis = dWidth / 2 - gap - 1;
            cStart = { dWidth / 2 + gap, top };
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

    if (dbFormat == "formatCourse" || dbFormat == "FormatWind" || dbFormat == "FormatRot") {

        if (dbFormat == "FormatRot") {
            chrtDataFmt = 2; // Chart is showing data of rotational <degree> format
        } else {
            chrtDataFmt = 1; // Chart is showing data of course / wind <degree> format
        }
        rngStep = M_TWOPI / 360.0 * 10.0; // +/-10 degrees on each end of chrtMid; we are calculating with SI values

    } else {
        chrtDataFmt = 0; // Chart is showing any other data format than <degree>
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
// Parameters are chart time interval, and the current boat data value to be printed
template <typename T>
void Chart<T>::showChrt(int8_t chrtIntv, GwApi::BoatValue currValue)
{
    drawChrtTimeAxis(chrtIntv);
    drawChrt(chrtIntv, currValue);
    drawChrtValAxis();
}

// draw chart
template <typename T>
void Chart<T>::drawChrt(int8_t chrtIntv, GwApi::BoatValue& currValue)
{
    double chrtVal; // Current data value
    double chrtScl; // Scale for data values in pixels per value
    static double chrtPrevVal; // Last data value in chart area
    bool bufDataValid = false; // Flag to indicate if buffer data is valid
    static int numNoData; // Counter for multiple invalid data values in a row

    int x, y; // x and y coordinates for drawing
    static int prevX, prevY; // Last x and y coordinates for drawing

    // Identify buffer size and buffer start position for chart
    count = dataBuf.getCurrentSize();
    currIdx = dataBuf.getLastIdx();
    numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display

    if (chrtIntv != oldChrtIntv || count == 1) {
        // new data interval selected by user; this is only x * 230 values instead of 240 seconds (4 minutes) per interval step
        // intvBufSize = timAxis * chrtIntv; // obsolete
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

                if (chrtDir == 0) { // horizontal chart
                    x = cStart.x + i; // Position in chart area
                    if (chrtDataFmt == 0) {
                        y = cStart.y + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else { // degree type value
                        y = cStart.y + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    }
                } else { // vertical chart
                    y = cStart.y + timAxis - i; // Position in chart area
                    if (chrtDataFmt == 0) {
                        x = cStart.x + static_cast<int>(((chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    } else { // degree type value
                        x = cStart.x + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScl) + 0.5); // calculate chart point and round
                    }
                }

                if (i >= (numBufVals / chrtIntv) - 4) // log chart data of 1 line (adjust for test purposes)
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %.4f, {x,y} {%d,%d}", i, chrtVal, x, y);

                if ((i == 0) || (chrtPrevVal == dbMAX_VAL)) {
                    // just a dot for 1st chart point or after some invalid values
                    prevX = x;
                    prevY = y;

                } else if (chrtDataFmt != 0) { // cross borders check for degree values; shift values to [-PI..0..PI]; when crossing borders, range is 2x PI degrees

                    // Normalize both values relative to chrtMin (shift range to start at 0)
                    double normCurr = WindUtils::to2PI(chrtVal - chrtMin);
                    double normPrev = WindUtils::to2PI(chrtPrevVal - chrtMin);
                    // Check if pixel positions are far apart (crossing chart boundary); happens when one value is near chrtMax and the other near chrtMin
                    bool crossedBorders = std::abs(normCurr - normPrev) > (chrtRng / 2.0);

                    if (crossedBorders) { // If current value crosses chart borders compared to previous value, split line
                        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: crossedBorders: %d, chrtVal: %.2f, chrtPrevVal: %.2f", crossedBorders, chrtVal, chrtPrevVal);
                        bool wrappingFromHighToLow = normCurr < normPrev; // Determine which edge we're crossing
                        int xSplit = wrappingFromHighToLow ? (cStart.x + valAxis) : cStart.x;
                        getdisplay().drawLine(prevX, prevY, xSplit, y, fgColor);
                        getdisplay().drawLine(prevX, prevY - 1, ((xSplit != prevX) ? xSplit : xSplit - 1), ((xSplit != prevX) ? y - 1 : y), fgColor);
                        prevX = wrappingFromHighToLow ? cStart.x : (cStart.x + valAxis);
                    }
                }

                // Draw line with 2 pixels width + make sure vertical lines are drawn correctly
                if (chrtDir == 0 || x == prevX) { // vertical line
                    getdisplay().drawLine(prevX, prevY, x, y, fgColor);
                    getdisplay().drawLine(prevX - 1, prevY, x - 1, y, fgColor);
//                    getdisplay().drawLine(prevX + 1, prevY, x - 1, y, fgColor);
                } else if (chrtDir == 1 || x != prevX) { // line with some horizontal trend -> normal state
                    getdisplay().drawLine(prevX, prevY, x, y, fgColor);
                    getdisplay().drawLine(prevX, prevY - 1, x, y - 1, fgColor);
//                    getdisplay().drawLine(prevX, prevY + 1, x, y - 1, fgColor);
                }
                chrtPrevVal = chrtVal;
                prevX = x;
                prevY = y;
            }

            // Reaching chart area bottom end
            if (i >= timAxis - 1) {
                oldChrtIntv = 0; // force reset of buffer start and number of values to show in next display loop

                if (chrtDataFmt == 1) { // degree of course or wind
                    recalcRngCntr = true;
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot FreeTop: timAxis: %d, i: %d, bufStart: %d, numBufVals: %d, recalcRngCntr: %d", timAxis, i, bufStart, numBufVals, recalcRngCntr);
                }
                break;
            }
        }

        // uses BoatValue temp variable <currValue> to format latest buffer value
        // doesn't work unfortunately when 'simulation data' is active, because OBP60Formatter generates own simulation value in that case
        currValue.value = dataBuf.getLast();
        currValue.valid = currValue.value != dbMAX_VAL;
        Chart<T>::prntCurrValue(currValue, { x, y });
        LOG_DEBUG(GwLog::DEBUG, "Chart drawChrt: currValue-value: %.1f, Valid: %d, Name: %s, Address: %p", currValue.value, currValue.valid, currValue.getName(), (void*)&currValue);

    } else {
        // No valid data available
        getdisplay().setFont(&Ubuntu_Bold10pt8b);

        int pX, pY;
        if (chrtDir == 0) { // horizontal chart
            pX = cStart.x + (timAxis / 2);
            pY = cStart.y + (valAxis / 2) - 10;
        } else { // vertical chart
            pX = cStart.x + (valAxis / 2);
            pY = cStart.y + (timAxis / 2) - 10;
        }

        getdisplay().fillRect(pX - 33, pY - 10, 66, 24, bgColor); // Clear area for message
        drawTextCenter(pX, pY, "No data");
        LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
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
    if (chrtDataFmt == 0) {
        // Chart data is of any type but 'degree'

        double oldRngMin = rngMin;
        double oldRngMax = rngMax;

        // Chart starts at lowest range value, but at least '0' or includes even negative values
        double currMinVal = dataBuf.getMin(numBufVals);
        LOG_DEBUG(GwLog::DEBUG, "calcChrtRange0a: currMinVal: %.1f, currMaxVal: %.1f, rngMin: %.1f, rngMid: %.1f, rngMax: %.1f, rng: %.1f, rngStep: %.1f, oldRngMin: %.1f, oldRngMax: %.1f, dfltRng: %.1f, numBufVals: %d",
            currMinVal, dataBuf.getMax(numBufVals), rngMin, rngMid, rngMax, rng, rngStep, oldRngMin, oldRngMax, dfltRng, numBufVals);

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
        LOG_DEBUG(GwLog::DEBUG, "calcChrtRange1a: currMinVal: %.1f, currMaxVal: %.1f, rngMin: %.1f, rngMid: %.1f, rngMax: %.1f, rng: %.1f, rngStep: %.1f, oldRngMin: %.1f, oldRngMax: %.1f, dfltRng: %.1f, numBufVals: %d",
            currMinVal, currMaxVal, rngMin, rngMid, rngMax, rng, rngStep, oldRngMin, oldRngMax, dfltRng, numBufVals);
    } else {

        if (chrtDataFmt == 1) {
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

        } else if (chrtDataFmt == 2) {
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
        LOG_DEBUG(GwLog::DEBUG, "calcChrtRange2b: rngMid: %.1f°, rngMin: %.1f°, rngMax: %.1f°, diffRng: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMid * RAD_TO_DEG, rngMin * RAD_TO_DEG, rngMax * RAD_TO_DEG,
            diffRng * RAD_TO_DEG, rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);
    }
}

// chart time axis label + lines
template <typename T>
void Chart<T>::drawChrtTimeAxis(int8_t chrtIntv)
{
    int timeRng;
    float slots, intv, i;
    char sTime[6];
    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setTextColor(fgColor);

    if (chrtDir == 0) { // horizontal chart
        getdisplay().fillRect(0, top, dWidth, 2, fgColor);

        timeRng = chrtIntv * 4; // Chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [8] 32 min.
        slots = timAxis / 80.0; // number of axis labels
        intv = timeRng / slots; // minutes per chart axis interval
        i = timeRng; // Chart axis label start at -32, -16, -12, ... minutes

        for (int j = 0; j < timAxis - 30; j += 80) { // fill time axis with values but keep area free on right hand side for value label
            // LOG_DEBUG(GwLog::DEBUG, "ChartTimeAxis: timAxis: %d, {x,y}: {%d,%d}, i: %.1f, j: %d, chrtIntv: %d, intv: %.1f, slots: %.1f", timAxis, cStart.x, cStart.y, i, j, chrtIntv, intv, slots);

            // Format time label based on interval
            if (chrtIntv < 3) {
                snprintf(sTime, sizeof(sTime), "-%.1f", i);
            } else {
                snprintf(sTime, sizeof(sTime), "-%.0f", std::round(i));
            }

            // draw text with appropriate offset
            //            int tOffset = (j == 0) ? 13 : (chrtIntv < 3 ? -4 : -4);
            int tOffset = j == 0 ? 13 : -4;
            drawTextCenter(cStart.x + j + tOffset, cStart.y - 8, sTime);
            getdisplay().drawLine(cStart.x + j, cStart.y, cStart.x + j, cStart.y + 5, fgColor); // draw short vertical time mark

            i -= intv;
        }

    } else { // chrtDir == 1; vertical chart
        timeRng = chrtIntv * 4; // chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [8] 32 min.
        slots = timAxis / 75.0; // number of axis labels
        intv = timeRng / slots; // minutes per chart axis interval
        i = -intv; // chart axis label start at -32, -16, -12, ... minutes

        for (int j = 75; j < (timAxis - 75); j += 75) { // don't print time label at upper and lower end of time axis
            if (chrtIntv < 3) { // print 1 decimal if time range is single digit (4 or 8 minutes)
                snprintf(sTime, sizeof(sTime), "%.1f", i);
            } else {
                snprintf(sTime, sizeof(sTime), "%.0f", std::floor(i));
            }

            getdisplay().drawLine(cStart.x, cStart.y + j, cStart.x + valAxis, cStart.y + j, fgColor); // Grid line

            if (chrtSz == 0) { // full size chart
                getdisplay().fillRect(0, cStart.y + j - 9, 32, 15, bgColor); // clear small area to remove potential chart lines
                getdisplay().setCursor((4 - strlen(sTime)) * 7, cStart.y + j + 3); // time value; print left screen; value right-formated
                getdisplay().printf("%s", sTime); // Range value
            } else if (chrtSz == 2) { // half size chart; right side
                drawTextCenter(dWidth / 2, cStart.y + j, sTime); // time value; print mid screen
            }

            i -= intv;
        }
    }
}

// chart value axis labels + lines
template <typename T>
void Chart<T>::drawChrtValAxis()
{
    double slots;
    int i, intv;
    double cVal, cchrtRng, crngMin;
    char sVal[6];
    std::unique_ptr<GwApi::BoatValue> tmpBVal; // Temp variable to get formatted and converted data value from OBP60Formatter
    tmpBVal = std::unique_ptr<GwApi::BoatValue>(new GwApi::BoatValue(dataBuf.getName()));
    tmpBVal->setFormat(dataBuf.getFormat());
    tmpBVal->valid = true;

    if (chrtDir == 0) { // horizontal chart
        slots = valAxis / 60.0; // number of axis labels
        tmpBVal->value = chrtRng;
        cchrtRng = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        intv = static_cast<int>(round(cchrtRng / slots));
        i = intv;

        getdisplay().setFont(&Ubuntu_Bold10pt8b);

        for (int j = 60; j < valAxis - 30; j += 60) {
            LOG_DEBUG(GwLog::DEBUG, "ChartValAxis: chrtRng: %.2f, cchrtRng: %.2f, intv: %d, slots: %.1f, valAxis: %d, i: %d, j: %d", chrtRng, cchrtRng, intv, slots, valAxis, i, j);
            getdisplay().drawLine(cStart.x, cStart.y + j, cStart.x + timAxis, cStart.y + j, fgColor);

            getdisplay().fillRect(cStart.x, cStart.y + j - 9, cStart.x + 32, 18, bgColor); // Clear small area to remove potential chart lines
            String sVal = String(i);
            getdisplay().setCursor((3 - sVal.length()) * 8, cStart.y + j + 6); // value right-formated
            getdisplay().printf("%s", sVal); // Range value

            i += intv;
        }

        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        drawTextRalign(cStart.x + timAxis, cStart.y - 3, dbName); // buffer data name

    } else { // chrtDir == 1; vertical chart
        getdisplay().setFont(&Ubuntu_Bold10pt8b);

        getdisplay().fillRect(cStart.x, top, valAxis, 2, fgColor); // top chart line
        getdisplay().setCursor(cStart.x, cStart.y - 2);
        tmpBVal->value = chrtMin;
        cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        getdisplay().printf("%s", sVal); // Range low end

        tmpBVal->value = chrtMid;
        cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        drawTextCenter(cStart.x + (valAxis / 2), cStart.y - 10, sVal); // Range mid end

        tmpBVal->value = chrtMax;
        cVal = formatValue(tmpBVal.get(), *commonData).cvalue; // value (converted)
        snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
        drawTextRalign(cStart.x + valAxis - 1, cStart.y - 2, sVal); // Range high end

        for (int j = 0; j <= valAxis + 2; j += ((valAxis + 2) / 2)) {
            getdisplay().drawLine(cStart.x + j, cStart.y, cStart.x + j, cStart.y + timAxis, fgColor);
        }

        if (chrtSz == 0) {
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            drawTextCenter(cStart.x + (valAxis / 4) + 5, cStart.y - 11, dbName); // buffer data name
        }
        LOG_DEBUG(GwLog::DEBUG, "ChartGrd: chrtRng: %.2f, intv: %d, slots: %.1f, valAxis: %d, i: %d", chrtRng, intv, slots, valAxis, i);
    }
}

// Print current data value
template <typename T>
void Chart<T>::prntCurrValue(GwApi::BoatValue& currValue, const Pos chrtPos)
{
    int currentZone;
    static int lastZone = 0;
    static bool flipVal = false;
    int xPosVal;
    static const int yPosVal = (chrtDir == 0) ? cStart.y + valAxis - 5 : cStart.y + timAxis - 5;
    xPosVal = cStart.x + 1;

    FormattedData frmtDbData = formatValue(&currValue, *commonData);
    double testdbValue = frmtDbData.value;
    String sdbValue = frmtDbData.svalue; // value (string)
    String dbUnit = frmtDbData.unit; // Unit of value
    LOG_DEBUG(GwLog::DEBUG, "Chart CurrValue: dbValue: %.2f, sdbValue: %s, fmrtDbValue: %.2f, dbFormat: %s, dbUnit: %s, Valid: %d, Name: %s, Address: %p", currValue.value, sdbValue,
        testdbValue, currValue.getFormat(), dbUnit, currValue.valid, currValue.getName(), currValue);

    getdisplay().fillRect(xPosVal, yPosVal - 34, 121, 40, bgColor); // Clear area for TWS value
    getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
    getdisplay().setCursor(xPosVal + 1, yPosVal);
    if (useSimuData) {
        getdisplay().printf("%2.1f", currValue.value); // Value
    } else {
        getdisplay().print(sdbValue); // Value
    }

    getdisplay().setFont(&Ubuntu_Bold10pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal - 17);
    getdisplay().print(dbName); // Name

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal + 1);
    getdisplay().print(dbUnit); // Unit
}

// Identify Min and Max values of range for course data and select them considering smallest gap
// E.g., Min=30°, Max=270° will be converted to smaller range of Min=270° and Max=30°
// obsolete; creates random results by purpose with large data arrays when data is equally distributed
template <typename T>
void Chart<T>::getAngleMinMax(const std::vector<double>& angles, double& rngMin, double& rngMax)
{
    if (angles.empty()) {
        rngMin = 0;
        rngMax = 0;
        return;
    }

    if (angles.size() == 1) {
        rngMin = angles[0];
        rngMax = angles[0];
        return;
    }

    // Sort angles
    std::vector<double> sorted = angles;
    std::sort(sorted.begin(), sorted.end());

    // Find the largest gap between consecutive angles
    double maxGap = 0.0;
    int maxGapIndex = 0;
    for (size_t i = 0; i < sorted.size(); i++) {
        double next = sorted[(i + 1) % sorted.size()];
        double curr = sorted[i];

        // Calculate gap (wrapping around at 360°/2*Pi)
        double gap = (i == sorted.size() - 1) ? (M_TWOPI - curr + next) : (next - curr);

        if (gap > maxGap) {
            maxGap = gap;
            maxGapIndex = i;
        }
    }

    // The range is on the opposite side of the largest gap
    // Min is after the gap, max is before it
    rngMin = sorted[(maxGapIndex + 1) % sorted.size()];
    rngMax = sorted[maxGapIndex];
}

// Explicitly instantiate class with required data types to avoid linker errors
template class Chart<uint16_t>;
// --- Class Chart ---------------
