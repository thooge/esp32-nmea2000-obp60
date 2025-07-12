#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "OBPRingBuffer.h"
#include "Pagedata.h"
#include <N2kMessages.h> // just for RadToDeg function
#include <vector>

static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

// Get maximum difference of last <amount> of TWD ringbuffer values to center chart
int getRng(const RingBuffer<int16_t>& windDirHstry, int center, size_t amount)
{
    int minVal = windDirHstry.getMinVal();
    size_t count = windDirHstry.getCurrentSize();
    size_t capacity = windDirHstry.getCapacity();
    size_t last = windDirHstry.getLastIdx();

    if (windDirHstry.isEmpty() || amount <= 0) {
        return minVal;
    }
    if (amount > count)
        amount = count;

    int value = 0;
    int rng = 0;
    int maxRng = minVal;
    // Start from the newest value (last) and go backwards x times
    for (size_t i = 0; i < amount; i++) {
        value = windDirHstry.get(((last - i) % capacity + capacity) % capacity);

        if (value == minVal) {
            continue;
        }

        value = value / 1000.0 * radToDeg;
        rng = abs(((value - center + 540) % 360) - 180);
        if (rng > maxRng)
            maxRng = rng;
    }
    if (maxRng > 180) {
        maxRng = 180;
    }

    return maxRng;
}

void fillSimData(PageData& pageData)
// Fill the TWD history buffer with simulated data
{
    int value = 20;
    int16_t value2 = 0;
    for (int i = 0; i < 600; i++) {
        value += random(-20, 20);
        if (value < 0)
            value += 360;
        if (value >= 360)
            value -= 360;
        value2 = static_cast<int16_t>(DegToRad(value) * 1000.0);
        pageData.boatHstry.twdHstry->add(value2); // Fill the buffer with some test data
    }
}

void fillTstBuffer(PageData& pageData)
{
    float value = 0;
    int value2 = 0;
    for (int i = 0; i < 60; i++) {
        pageData.boatHstry.twdHstry->add(-10); // Fill the buffer with some test data
    }
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            value += 10;
            value2 = static_cast<int>(DegToRad(value) * 1000.0);
            pageData.boatHstry.twdHstry->add(value2); // Fill the buffer with some test data
        }
        for (int j = 0; j < 20; j++) {
            value -= 10;
            value2 = static_cast<int>(DegToRad(value) * 1000.0);
            pageData.boatHstry.twdHstry->add(value2); // Fill the buffer with some test data
        }
    }
}

// ****************************************************************
class PageWindPlot : public Page {

    bool keylock = false; // Keylock
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both
    int dataIntv = 1; // Update interval for wind history chart:
                      // (1)|(2)|(3)|(4) seconds for approx. 4, 8, 12, 16 min. history chart
    bool showTWS = true; // Show TWS value in chart area

public:
    PageWindPlot(CommonData& common)
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG, "Instantiate PageWindPlot");
    }

    virtual void setupKeys()
    {
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
        commonData->keydata[1].label = "INTV";
        commonData->keydata[4].label = "TWS";
    }

    // Key functions
    virtual int handleKey(int key)
    {
        // Set chart mode TWD | TWS
        if (key == 1) {
            if (chrtMode == 'D') {
                chrtMode = 'S';
            } else if (chrtMode == 'S') {
                chrtMode = 'B';
            } else {
                chrtMode = 'D';
            }
            return 0; // Commit the key
        }

        // Set interval for wind history chart update time
        if (key == 2) {
            if (dataIntv == 1) {
                dataIntv = 2;
            } else if (dataIntv == 2) {
                dataIntv = 3;
            } else if (dataIntv == 3) {
                dataIntv = 4;
            } else {
                dataIntv = 1;
            }
            return 0; // Commit the key
        }

        // Switch TWS on/off
        if (key == 5) {
            showTWS = !showTWS;
            return 0; // Commit the key
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayPage(PageData& pageData)
    {
        GwConfigHandler* config = commonData->config;
        GwLog* logger = commonData->logger;

        float twsValue; // TWS value in chart area
        String twdName, twdUnit; // TWD name and unit
        int updFreq; // Update frequency for TWD
        int16_t twdLowest, twdHighest; // TWD range

        static bool isInitialized = false; // Flag to indicate that page is initialized
        static bool wndDataValid = false; // Flag to indicate if wind data is valid
        static int numNoData; // Counter for multiple invalid data values in a row
        static bool simulation = false;
        static bool holdValues = false;

        static int width; // Screen width
        static int height; // Screen height
        static int xCenter; // Center of screen in x direction
        static const int yOffset = 48; // Offset for y coordinates of chart area
        static int cHeight; // height of chart area
        static int bufSize; // History buffer size: 960 values for appox. 16 min. history chart
        int intvBufSize; // Buffer size used for currently selected time interval
        int count; // current size of buffer
        int numWndVals; // number of wind values available for current interval selection
        static int linesToShow; // current number of lines to display on chart
        static int bufStart; // 1st data value in buffer to show
        int numAddedBufVals; // Number of values added to buffer since last display
        size_t currIdx; // Current index in TWD history buffer
        static size_t lastIdx; // Last index of TWD history buffer
        static size_t lastAddedIdx = 0; // Last index of TWD history buffer when new data was added
        static int oldDataIntv; // remember recent user selection of data interval

        static int wndCenter; // chart wind center value position
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value
        static const int dfltRng = 40; // Default range for chart
        static int simTwd; // Simulation value for TWD
        static float simTws; // Simulation value for TWS

        int x, y; // x and y coordinates for drawing
        static int prevX, prevY; // Last x and y coordinates for drawing
        static float chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        static int chrtPrevVal; // Last wind value in chart area for check if value crosses 180 degree line

        LOG_DEBUG(GwLog::LOG, "Display page WindPlot");
        unsigned long start = millis();

        // Get config data
        simulation = config->getBool(config->useSimuData);
        holdValues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        if (!isInitialized) {

            if (simulation) {
                fillSimData(pageData); // Fill the buffer with some test data
            }

            width = getdisplay().width();
            height = getdisplay().height();
            xCenter = width / 2;
            cHeight = height - yOffset - 22;
            bufSize = pageData.boatHstry.twdHstry->getCapacity();
            numNoData = 0;
            simTwd = pageData.boatHstry.twdHstry->getLast() / 1000.0 * radToDeg;
            simTws = 0;
            twsValue = 0;
            bufStart = 0;
            linesToShow = 0;
            oldDataIntv = 0;
            numAddedBufVals, currIdx, lastIdx = 0;
            lastAddedIdx = pageData.boatHstry.twdHstry->getLastIdx();
            pageData.boatHstry.twdHstry->getMetaData(twdName, twdUnit, updFreq, twdLowest, twdHighest);
            wndCenter = INT_MIN;
            isInitialized = true; // Set flag to indicate that page is now initialized
            LOG_DEBUG(GwLog::ERROR, "PageWindPlot Start1: lastAddedIdx: %d, simTwd: %.1f, isInitialized: %d, SimData: %d", lastAddedIdx, simTwd / 1000 + radToDeg, isInitialized, simulation);
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (simulation) {
            simTwd += random(-20, 20);
            if (simTwd < 0.0)
                simTwd += 360.0;
            if (simTwd >= 360.0)
                simTwd -= 360.0;

            int16_t z = static_cast<int16_t>(DegToRad(simTwd) * 1000.0);
            pageData.boatHstry.twdHstry->add(z); // Fill the buffer with some test data

            simTws += random(-20, 20); // TWS value in knots
            simTws = constrain(simTws, 0.0f, 50.0f); // Ensure TWS is between 0 and 50 knots
            twsValue = simTws;
            LOG_DEBUG(GwLog::ERROR, "PageWindPlot Simulation: simTwd: %f, twsValue: %f", simTwd, twsValue);
        } else {
            twsValue = pageData.boatHstry.twsHstry->getLast() / 10.0 * 1.94384; // TWS value in knots
        }

        // Identify buffer size and buffer start position for chart
        intvBufSize = cHeight * dataIntv;
        count = pageData.boatHstry.twdHstry->getCurrentSize();
        numWndVals = min(count, intvBufSize);
        currIdx = pageData.boatHstry.twdHstry->getLastIdx();
        numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display
        if (dataIntv != oldDataIntv) {
            // new data interval selected by user
            linesToShow = min(numWndVals / dataIntv, cHeight - 60);
            bufStart = max(0, count - (linesToShow * dataIntv));
            currIdx++; // eliminate current added value for bufStart calculation
            oldDataIntv = dataIntv;
        } else {
            if (numAddedBufVals >= dataIntv) {
                linesToShow += (numAddedBufVals / dataIntv); // Number of lines to show on chart
                LOG_DEBUG(GwLog::ERROR, "PageWindPlot bufStart: bufStart: %d, numAddedBufVals: %d, linesToShow: %d, count %d, bufSize %d, Cnt=Size: %d", bufStart, numAddedBufVals, linesToShow, count, bufSize, count == bufSize);
                lastAddedIdx = currIdx;
            }
            if (count == bufSize && currIdx != lastIdx) {
                int numVals = (currIdx - lastIdx + bufSize) % bufSize;
                bufStart = ((bufStart - numVals) % bufSize + bufSize) % bufSize; // keep 1st chart value constant in a rolling buffer when new data is added
                lastIdx = currIdx;
            }
        }
        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Dataset: TWD: %.1f, TWS: %.1f, DBT: %.1f, count: %d, intvBufSize: %d, numWndVals: %d, bufStart: %d, linesToShow: %d, numAddedBufVals: %d, lastIdx: %d",
            pageData.boatHstry.twdHstry->getLast() / 1000.0 * radToDeg, pageData.boatHstry.twsHstry->getLast() / 10.0 * 1.94384, pageData.boatHstry.dbtHstry->getLast() / 10.0,
            count, intvBufSize, numWndVals, bufStart, linesToShow, numAddedBufVals, lastIdx);

        // initialize chart range values
        if (wndCenter == INT_MIN) {
            wndCenter = max(0, int(pageData.boatHstry.twdHstry->get(numWndVals - intvBufSize) / 1000.0 * radToDeg)); // get 1st value of current data interval
            wndCenter = (int((wndCenter + (wndCenter >= 0 ? 5 : -5)) / 10) * 10) % 360; // Set new center value; round to nearest 10 degree value; 360° -> 0°
            diffRng = dfltRng;
            chrtRng = dfltRng;
        } else {
            // check and adjust range between left, center, and right chart limit
            diffRng = getRng(*pageData.boatHstry.twdHstry, wndCenter, numWndVals);
            diffRng = (diffRng == INT16_MIN ? 0 : diffRng);
            if (diffRng > chrtRng) {
                chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 degree value
            } else if (diffRng + 10 < chrtRng) { // Reduce chart range for higher resolution if possible
                chrtRng = max(dfltRng, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10);
            }
        }
        chrtScl = float(width) / float(chrtRng) / 2.0; // Chart scale: pixels per degree
        wndLeft = wndCenter - chrtRng;
        if (wndLeft < 0)
            wndLeft += 360;
        wndRight = (chrtRng < 180 ? wndCenter + chrtRng : wndCenter + chrtRng - 1);
        if (wndRight >= 360)
            wndRight -= 360;
        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot FirstVal: %f, LastVal: %d, count: %d, diffRng: %d, chartRng: %d, Center: %d, scale: %f", pageData.boatHstry.twdHstry->getFirst() / 1000.0 * radToDeg,
            pageData.boatHstry.twdHstry->get(linesToShow) / 1000.0 * radToDeg, count, diffRng, chrtRng, wndCenter, chrtScl);

        // Draw page
        //***********************************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // chart lines
        getdisplay().fillRect(0, yOffset, width, 2, commonData->fgcolor);
        getdisplay().fillRect(xCenter, yOffset, 1, cHeight, commonData->fgcolor);

        // chart labels
        char sWndLbl[4]; // char buffer for Wind angle label
        getdisplay().setFont(&Ubuntu_Bold12pt7b);
        getdisplay().setCursor(xCenter - 88, yOffset - 3);
        getdisplay().print("TWD"); // Wind name
        // getdisplay().setCursor(xCenter - 20, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndCenter < 0) ? (wndCenter + 360) : wndCenter);
        drawTextCenter(xCenter, yOffset - 11, sWndLbl);
        // getdisplay().print(sWndLbl); // Wind center value
        getdisplay().drawCircle(xCenter + 25, yOffset - 17, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(xCenter + 25, yOffset - 17, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(1, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndLeft < 0) ? (wndLeft + 360) : wndLeft);
        getdisplay().print(sWndLbl); // Wind left value
        getdisplay().drawCircle(46, yOffset - 17, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(46, yOffset - 17, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 50, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndRight < 0) ? (wndRight + 360) : wndRight);
        getdisplay().print(sWndLbl); // Wind right value
        getdisplay().drawCircle(width - 5, yOffset - 17, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 5, yOffset - 17, 3, commonData->fgcolor); // <degree> symbol

        if (pageData.boatHstry.twdHstry->getMax() == twdLowest) {
            // only <INT16_MIN> values in buffer -> no valid wind data available
            wndDataValid = false;
        } else {
            wndDataValid = true; // At least some wind data available
        }
        // Draw wind values in chart
        //***********************************************************************
        if (wndDataValid) {
            for (int i = 0; i < linesToShow; i++) {
                chrtVal = static_cast<int>(pageData.boatHstry.twdHstry->get((bufStart + (i * dataIntv)) % bufSize)); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
                if (chrtVal == INT16_MIN) {
                    chrtPrevVal = INT16_MIN;
                    /* if (i == linesToShow - 1) {
                        numNoData++;
                        // If more than 4 invalid values in a row, reset chart
                    } else {
                        numNoData = 0; // Reset invalid value counter
                    }
                    if (numNoData > 4) {
                        // If more than 4 invalid values in a row, send message
                        getdisplay().setFont(&Ubuntu_Bold10pt7b);
                        getdisplay().fillRect(xCenter - 66, height / 2 - 20, 146, 24, commonData->bgcolor); // Clear area for TWS value
                        drawTextCenter(xCenter, height / 2 - 10, "No sensor data");
                    } */
                } else {
                    chrtVal = (chrtVal / 1000.0 * radToDeg) + 0.5; // Convert to degrees and round
                    x = ((chrtVal - wndLeft + 360) % 360) * chrtScl;
                    y = yOffset + cHeight - i; // Position in chart area
                    if (i > linesToShow - 30)
                        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Chart: i: %d, chrtVal: %d, bufStart: %d count: %d, linesToShow: %d", i, chrtVal, bufStart, count, linesToShow);

                    if ((i == 0) || (chrtPrevVal == INT16_MIN)) {
                        // just a dot for 1st chart point or after some invalid values
                        prevX = x;
                        prevY = y;
                    } else {
                        // cross borders check; shift values to [-180..0..180]; when crossing borders, range is 2x 180 degrees
                        int wndLeftDlt = -180 - ((wndLeft >= 180) ? (wndLeft - 360) : wndLeft);
                        int chrtVal180 = ((chrtVal + wndLeftDlt + 180) % 360 + 360) % 360 - 180;
                        int chrtPrevVal180 = ((chrtPrevVal + wndLeftDlt + 180) % 360 + 360) % 360 - 180;
                        if (((chrtPrevVal180 >= -180) && (chrtPrevVal180 < -90) && (chrtVal180 > 90)) || ((chrtPrevVal180 <= 179) && (chrtPrevVal180 > 90) && chrtVal180 <= -90)) {
                            // If current value crosses chart borders compared to previous value, split line
                            int xSplit = (((chrtPrevVal180 > 0 ? wndRight : wndLeft) - wndLeft + 360) % 360) * chrtScl;
                            getdisplay().drawLine(prevX, prevY, xSplit, y, commonData->fgcolor);
                            getdisplay().drawLine(prevX, prevY - 1, ((xSplit != prevX) ? xSplit : xSplit - 1), ((xSplit != prevX) ? y - 1 : y), commonData->fgcolor);
                            prevX = (((chrtVal180 > 0 ? wndRight : wndLeft) - wndLeft + 360) % 360) * chrtScl;
                        }
                    }

                    // Draw line with 2 pixels width + make sure vertical line are drawn correctly
                    getdisplay().drawLine(prevX, prevY, x, y, commonData->fgcolor);
                    getdisplay().drawLine(prevX, prevY - 1, ((x != prevX) ? x : x - 1), ((x != prevX) ? y - 1 : y), commonData->fgcolor);
                    chrtPrevVal = chrtVal;
                    prevX = x;
                    prevY = y;
                }
                // Reaching chart area top end
                if (i >= (cHeight - 1)) {
                    linesToShow -= min(60, cHeight); // free top 40 lines of chart for new values
                    if (count >= numWndVals) {
                        bufStart = (bufStart + (60 * dataIntv)) % bufSize; // next start value in buffer to show
                    }
                    int minWndDir = pageData.boatHstry.twdHstry->getMin(numWndVals) / 1000.0 * radToDeg;
                    int maxWndDir = pageData.boatHstry.twdHstry->getMax(numWndVals) / 1000.0 * radToDeg;
                    LOG_DEBUG(GwLog::ERROR, "PageWindPlot FreeTop: Minimum: %d, Maximum: %d, OldwndCenter: %d", minWndDir, maxWndDir, wndCenter);
                    if ((minWndDir > wndCenter) || (maxWndDir < wndCenter)) {
                        // Check if all wind value are left or right of center value -> optimize chart range
                        int midWndDir = pageData.boatHstry.twdHstry->getMid(numWndVals) / 1000.0 * radToDeg;
                        if (midWndDir != INT16_MIN) {
                            wndCenter = int((midWndDir + (midWndDir >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
                        }
                    }
                    LOG_DEBUG(GwLog::ERROR, "PageWindPlot FreeTop: cHeight: %d, LinesToShow: %d, bufStart: %d, numWndVals: %d, wndCenter: %d", cHeight, linesToShow, bufStart, numWndVals, wndCenter);
                    break;
                }
            }

        } else {
            // No valid data available
            LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
            getdisplay().setFont(&Ubuntu_Bold10pt7b);
            getdisplay().fillRect(xCenter - 66, height / 2 - 20, 146, 24, commonData->bgcolor); // Clear area for TWS value
            drawTextCenter(xCenter, height / 2 - 10, "No sensor data");
        }

        // Print TWS value
        if (showTWS) {
            int currentZone;
            static int lastZone = 0;
            static bool flipTws = false;
            int xPosTws;
            static const int yPosTws = yOffset + 40;

            //            xPosTws = flipTws ? 30 : width - 145;
            xPosTws = flipTws ? 20 : width - 138;
            currentZone = (y >= yPosTws - 38) && (y <= yPosTws + 6) && (x >= xPosTws - 4) && (x <= xPosTws + 146) ? 1 : 0; // Define current zone for TWS value
            //            currentZone = (x >= xPosTws - 4) && (x <= xPosTws + 142) ? 1 : 0; // Define current zone for TWS value
            //            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot TWS: xPos: %d, yPos: %d, x: %d y: %d, currZone: %d, lastZone: %d", xPosTws, yPosTws, x, y, currentZone, lastZone);
            if (currentZone != lastZone) {
                // Only flip when x moves to a different zone
                if ((y >= yPosTws - 38) && (y <= yPosTws + 6) && (x >= xPosTws - 4) && (x <= xPosTws + 146)) {
                    flipTws = !flipTws;
                    xPosTws = flipTws ? 20 : width - 145;
                }
            }
            lastZone = currentZone;

            getdisplay().fillRect(xPosTws - 4, yPosTws - 38, 142, 44, commonData->bgcolor); // Clear area for TWS value
            getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
            getdisplay().setCursor(xPosTws, yPosTws);
            //            twsValue = pageData.boatHstry.twsHstry->getLast() / 10.0 * 1.94384; // TWS value in knots
            if (twsValue < 0 || twsValue >= 100) {
                getdisplay().print("--.-");
            } else {
                getdisplay().printf("%2.1f", twsValue); // Value
            }
            getdisplay().setFont(&Ubuntu_Bold12pt7b);
            getdisplay().setCursor(xPosTws + 82, yPosTws - 14);
            getdisplay().print("TWS"); // Name
            getdisplay().setFont(&Ubuntu_Bold8pt7b);
            getdisplay().setCursor(xPosTws + 78, yPosTws + 1);
            getdisplay().printf(" kn"); // Unit
        }

        // chart Y axis labels; print at last to overwrite potential chart lines in label area
        int yPos;
        int chrtLbl;
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        for (int i = 1; i <= 3; i++) {
            yPos = yOffset + (i * 60);
            getdisplay().fillRect(0, yPos, width, 1, commonData->fgcolor);
            getdisplay().fillRect(0, yPos - 8, 26, 16, commonData->bgcolor); // Clear small area to remove potential chart lines
            getdisplay().setCursor(1, yPos + 4);
            if (count >= intvBufSize) {
                // Calculate minute value for label
                chrtLbl = ((i - 1 + (prevY < yOffset + 30)) * dataIntv) * -1; // change label if last data point is more than 30 lines (= seconds) from chart line
            } else {
                int j = 3 - i;
                //                chrtLbl = (int((((count / dataIntv) - 50) * dataIntv / 60) + 1) - ((j - 1) * dataIntv)) * -1; // 50 lines left below last chart line
                chrtLbl = (int(((linesToShow - 50) * dataIntv / 60) + 1) - (j * dataIntv)) * -1; // 50 lines left below last chart line
            }
            //            if (chrtLbl <= 0) {
            getdisplay().printf("%3d", chrtLbl); // Wind value label
            //            }
        }

        unsigned long finish = millis() - start;
        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Time: %lu", finish);
        // Update display
        getdisplay().nextPage(); // Partial update (fast)
    };
};

static Page* createPage(CommonData& common)
{
    return new PageWindPlot(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageWindPlot(
    "WindPlot", // Page name
    createPage, // Action
    0, // Number of bus values depends on selection in Web configuration
    { "TWD", "TWA", "TWS", "HDM", "AWA", "AWS", "STW", "COG", "SOG" }, // Bus values we need in the page
    true // Show display header on/off
);

#endif
