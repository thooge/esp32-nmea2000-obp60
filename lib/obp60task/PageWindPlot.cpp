#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "Pagedata.h"

#include <vector>

// ****************************************************************
class wndHistory {
    // provides a circular buffer to store wind history values
private:
    int SIZE;
    std::vector<int> buffer;
    int first; // points to the first (oldest) valid element
    int last; // points to the last (newest) valid element
    int head; // points to the next insertion index
    int count; // number of valid elements

public:
    bool begin(int size)
    // start buffer
    {
        if (size <= 0 || size > 10000) {
            return false;
        }
        SIZE = size;
        buffer.resize(size, INT_MIN); // allocate buffer
        head = 0;
        first = 0;
        last = 0;
        count = 0;
        return true;
    }

    void add(int value)
    // Add a new value; store in [0..360 deg] format
    {
        if (value < 0 || value > 360)
            value = INT_MIN;
        buffer[head] = value;
        last = head;
        head = (head + 1) % SIZE;
        if (count < SIZE) {
            count++;
        } else {
            first = head - 1; // When buffer is full, first points to the oldest value
            if (first < 0)
                first += SIZE;
        }
    }

    int get(int index) const
    // Get value by index in [0..360 deg] format (0 = oldest, count-1 = newest)
    // INT_MIN indicates missing value or wrong index
    {
        int realIndex;

        if (index < 0 || index >= count) {
            return INT_MIN; // Invalid index
        }
        realIndex = (first + index) % SIZE;
        return buffer[realIndex];
    }

    int get(int index, int deg) const
    // Get value by index in [-180..180 deg] or [0..360 deg] format (0 = oldest, count-1 = newest)
    {
        switch (deg) {
        case 180:
            // Return value in [-180..180 deg] format
            return get(index);
        case 360: {
            // Return value in [0..360 deg] format
            int value = get(index);
            if (value < 0) {
                value += 360;
            };
            return value;
        }
        default:
            return -1;
        }
    }

    int getSize() const
    // Get number of valid elements
    {
        return count;
    }

    int getMin() const
    // Get minimum value of buffer
    {
        if (count == 0) {
            return -1; // Buffer is empty
        } else if (first + count <= SIZE) {
            // No wrap-around
            return *std::min_element(buffer.begin() + first, buffer.begin() + first + count);
        } else {
            // Wrap-around: check [first, end) and [begin, (first+count)%SIZE)
            int min1 = *std::min_element(buffer.begin() + first, buffer.end());
            int min2 = *std::min_element(buffer.begin(), buffer.begin() + ((first + count) % SIZE));
            return std::min(min1, min2);
        }
    }

    int getMin(int amount) const
    // Get minimum value of the last <amount> values of buffer
    // INT_MIN indicates missing value or wrong index

    {
        if (count == 0 || amount <= 0)
            return INT_MIN;
        if (amount > count)
            amount = count;

        int minVal = INT_MAX;
        int value = 0;
        // Start from the newest value (last) and go backwards x times
        for (int i = 0; i < amount; ++i) {
            value = get(i);
            if (value < minVal)
                minVal = value;
        }
        return minVal;
    }

    int getMax() const
    // Get maximum value of buffer
    {
        if (count == 0) {
            return -1; // Buffer is empty
        } else if (first + count <= SIZE) {
            // No wrap-around
            return *std::max_element(buffer.begin() + first, buffer.begin() + first + count);
        } else {
            // Wrap-around: check [first, end) and [begin, (first+count)%SIZE)
            int max1 = *std::max_element(buffer.begin() + first, buffer.end());
            int max2 = *std::max_element(buffer.begin(), buffer.begin() + ((first + count) % SIZE));
            return std::max(max1, max2);
        }
    }

    int getMax(int amount) const
    // Get maximum value of the last <amount> values of buffer
    // INT_MIN indicates missing value or wrong index

    {
        if (count == 0 || amount <= 0)
            return -1;
        if (amount > count)
            amount = count;

        int maxVal = INT_MIN;
        int value = 0;
        // Start from the newest value (last) and go backwards x times
        for (int i = 0; i < amount; ++i) {
            value = get(i);
            if (value > maxVal)
                maxVal = value;
        }
        return maxVal;
    }

    int getMid(int amount) const
    // Get middle value in the buffer
    {
        if (count == 0) {
            return INT_MIN; // Buffer is empty
        }
        return (getMin(amount) + getMax(amount)) / 2;
    }

    int getRng(int center, int amount) const
    // Get maximum difference of last <amount> of buffer values to center value
    {
        if (count == 0 || amount <= 0)
            return INT_MIN;
        if (amount > count)
            amount = count;

        int value = 0;
        int maxRng = INT_MIN;
        int rng = 0;
        // Start from the newest value (last) and go backwards x times
        for (int i = 0; i < amount; ++i) {
            value = get(i);
            if (value == INT_MIN) {
                continue;
            }
            rng = abs(((value - center + 540) % 360) - 180);
            if (rng > maxRng)
                maxRng = rng;
        }
        if (maxRng > 180) {
            maxRng = 180;
        }
        return maxRng;
    }
};

// ****************************************************************
class PageWindPlot : public Page {

    bool keylock = false; // Keylock
    //    int16_t lp = 80; // Pointer length
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both06121990
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

        static wndHistory windDirHstry; // Circular buffer to store wind direction values
        static wndHistory windSpdHstry; // Circular buffer to store wind speed values

        GwApi::BoatValue* bvalue;
        const int numCfgValues = 9;
        String dataName[numCfgValues];
        double dataValue[numCfgValues];
        bool dataValid[numCfgValues];
        String dataSValue[numCfgValues];
        String dataUnit[numCfgValues];
        String dataSValueOld[numCfgValues];
        String dataUnitOld[numCfgValues];
        int twdValue;
        static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

        bool wndDataValid = false; // Flag to indicate if wind data is valid
        bool simulation = false;
        bool holdValues = false;

        int width = getdisplay().width(); // Get screen width
        int height = getdisplay().height(); // Get screen height
        int xCenter = width / 2; // Center of screen in x direction
        static const int yOffset = 48; // Offset for y coordinates of chart area
        int cHeight = height - yOffset - 22; // height of chart area
        //        cHeight = 60;
        int bufSize = cHeight * 4; // Buffer size: 920 values for appox. 16 min. history chart
        int intvBufSize; // Buffer size used for currently selected time interval
        int count; // current size of buffer
        int numWndValues; // number of wind values available for current interval selection
        static int linesToShow; // current number of lines to display on chart
        static int bufStart; // 1st data value in buffer to show
        static int oldDataIntv; // remember recent user selection of data interval
        static int newDate; // indicates for higher time intervals that new date is available

        static int wndCenter = INT_MIN; // chart wind center value position; init value indicates that wndCenter is not set yet
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value
        static const int dfltRng = 40; // Default range for chart
        static int simWnd = 0; // Simulation value for wind data
        static float simTWS = 0; // Simulation value for TWS data
        static const int simStep = 10; // Simulation step for wind data

        int x, y; // x and y coordinates for drawing
        static int prevX, prevY; // Last x and y coordinates for drawing
        static float chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        static int chrtPrevVal; // Last wind value in chart area for check if value crosses 180 degree line
        int distVals; // helper to check wndCenter crossing
        int distMid; // helper to check wndCenter crossing

        LOG_DEBUG(GwLog::LOG, "Display page WindPlot");
        unsigned long start = millis();

        // Data initialization
        if (windDirHstry.getSize() == 0) {
            if (!windDirHstry.begin(bufSize)) {
                logger->logDebug(GwLog::ERROR, "Failed to initialize wind direction history buffer");
                return;
            }
            simWnd = 0;
            simTWS = 0;
            twdValue = 0;
            bufStart = 0;
            linesToShow = 0;
            oldDataIntv = dataIntv;
            newDate = 0;
        }

        // Get config data
        // String lengthformat = config->getString(config->lengthFormat);
        simulation = config->getBool(config->useSimuData);
        holdValues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Read boatdata values for TWD, TWA, TWS, HDM, AWA, AWS, STW, COG, SOG, if available
        for (int i = 0; i < numCfgValues; i++) {
            bvalue = pageData.values[i];
            dataName[i] = xdrDelete(bvalue->getName());
            dataName[i] = dataName[i].substring(0, 6); // String length limit for value name
            calibrationData.calibrateInstance(dataName[i], bvalue, logger); // Check if boat data value is to be calibrated
            dataValue[i] = bvalue->value; // Value as double in SI unit
            calibrationData.calibrateInstance(dataName[i], bvalue, logger); // Check if boat data value is to be calibrated
            dataValid[i] = bvalue->valid;
            dataSValue[i] = formatValue(bvalue, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
            dataUnit[i] = formatValue(bvalue, *commonData).unit;
            if (dataValid[i]) {
                dataSValueOld[i] = dataSValue[i]; // Save old value
                dataUnitOld[i] = dataUnit[i]; // Save old unit
            }
        }

        // Store TWD wind value in buffer, regardless of validity -> one value per second (if delivered in that frequency)
        twdValue = int((dataValue[0] * radToDeg) + 0.5); // Read TWD value in degrees and round to integer
        if (dataValid[0]) { // TWD data existing
            wndDataValid = true;
        } else {
            // Try to calculate TWD value from other data, if available
            //    wndDataValid = windValues.calcTWD(&twdValue, dataValue[1], dataValue[2], dataValue[3], dataValue[4], dataValue[5], dataValue[6]);
        }

        if (simulation) {
            // Simulate data if simulation is enabled; use default simulation values for TWS
            simWnd += random(simStep * -1, simStep); // random value between -simStep and +simStep
            if (simWnd < 0)
                simWnd += 360;
            simWnd = simWnd % 360;
            windDirHstry.add(simWnd);
            //            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot simulation data: windValue: %d, windSpeed: %s", simWnd, dataSValue[2].c_str());
        } else if (wndDataValid) {
            windDirHstry.add(twdValue);
        }

        // Identify buffer sizes and buffer position to print on the chart
        intvBufSize = cHeight * dataIntv;
        count = windDirHstry.getSize();
        numWndValues = min(count, intvBufSize);
        newDate++;
        if (dataIntv != oldDataIntv) {
            linesToShow = min(numWndValues / dataIntv, max(0, cHeight - 40));
            bufStart = max(0, (count - (linesToShow * dataIntv)));
            oldDataIntv = dataIntv;
        } else if (newDate >= dataIntv) {
            linesToShow = min(numWndValues / dataIntv, linesToShow + 1);
            newDate = 0;
        }
        if (count == bufSize) {
            bufStart--; // show the latest wind values in buffer; keep 1st value constant in a rolling buffer when new data is added
        }
        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Dataset: TWD: %d, count: %d, intvBufSize: %d, numWndValues: %d, bufStart: %d, linesToShow: %d, newDate: %d", twdValue, count, intvBufSize, numWndValues, bufStart, linesToShow, newDate);

        if (bvalue == NULL)
            return;
        LOG_DEBUG(GwLog::LOG, "PageWindPlot, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f, %s:%f, %s:%f, %s:%f, %s:%f, cnt: %d, valid0: %d", dataName[0].c_str(), dataValue[0],
            dataName[1].c_str(), dataValue[1], dataName[2].c_str(), dataValue[2], dataName[3].c_str(), dataValue[3], dataName[4].c_str(), dataValue[4],
            dataName[5].c_str(), dataValue[5], dataName[6].c_str(), dataValue[6], dataName[7].c_str(), dataValue[7], dataName[8].c_str(), dataValue[8], count, dataValid[0]);

        // initialize chart range values
        if (wndCenter == INT_MIN) {
            wndCenter = max(0, windDirHstry.get(numWndValues - intvBufSize)); // get 1st value of current data interval
            wndCenter = (int((wndCenter + (wndCenter >= 0 ? 5 : -5)) / 10) * 10) % 360; // Set new center value; round to nearest 10 degree value; 360° -> 0°
            diffRng = dfltRng;
            chrtRng = dfltRng;
        } else {
            // check and adjust range between left, center, and right chart limit
            diffRng = windDirHstry.getRng(wndCenter, numWndValues);
            diffRng = (diffRng == INT_MIN ? 0 : diffRng);
            if (diffRng > chrtRng) {
                chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 degree value
            } else if (diffRng + 10 < chrtRng) { // Reduce chart range for higher resolution if possible
                chrtRng = max(dfltRng, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10);
            }
            int debugMin = windDirHstry.getMin(numWndValues);
            int debugMax = windDirHstry.getMax(numWndValues);
            LOG_DEBUG(GwLog::ERROR, "PageWindPlot Range. wndCenter: %d, numWndValues: %d, min: %d, max: %d, diffrng: %d, chrtRng: %d ", wndCenter, numWndValues, debugMin, debugMax, diffRng, chrtRng);
        }
        chrtScl = float(width) / float(chrtRng) / 2.0; // Chart scale: pixels per degree
        wndLeft = wndCenter - chrtRng;
        if (wndLeft < 0)
            wndLeft += 360;
        wndRight = (chrtRng < 180 ? wndCenter + chrtRng : wndCenter + chrtRng - 1);
        if (wndRight >= 360)
            wndRight -= 360;
        //        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot dataValue[0]: %f, windValue: %d, count: %d, diffRng: %d, chartRng: %d, Center: %d, scale: %f", double(dataValue[0] * radToDeg),
        //            (!windDirHstry.get(count - 1) < 0 ? 0 : windDirHstry.get(count - 1)), count, diffRng, chrtRng, wndCenter, chrtScl);

        // Draw page
        //***********************************************************

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

        // Draw wind values in chart
        //***********************************************************
        if (wndDataValid || holdValues || simulation) {
            //            if (count == bufSize)
            //                bufStart--; // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
            for (int i = 0; i < linesToShow; i++) {
                chrtVal = windDirHstry.get(bufStart + (i * dataIntv)); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
                x = ((chrtVal - wndLeft + 360) % 360) * chrtScl;
                y = yOffset + cHeight - i; // Position in chart area
                if (i > linesToShow - 15)
                    LOG_DEBUG(GwLog::ERROR, "PageWindPlot Chart: i: %d, chrtVal: %d, chrtPrevVal: %d, bufStart: %d count: %d, linesToShow: %d", i, chrtVal, chrtPrevVal, bufStart, count, linesToShow);

                if (i == 0) {
                    prevX = x; // just a dot for 1st chart point
                    prevY = y;
                } else {
                    // cross borders check; shift values to [-180..0..180]; when crossing borders, range is 2x 180 degrees
                    int wndLeftDlt = -180 - ((wndLeft >= 180) ? (wndLeft - 360) : wndLeft);
                    int chrtVal180 = ((chrtVal + wndLeftDlt + 180) % 360 + 360) % 360 - 180;
                    int chrtPrevVal180 = ((chrtPrevVal + wndLeftDlt + 180) % 360 + 360) % 360 - 180;
                    //                    if (i > linesToShow - 15)
                    //                        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Chart: i: %d, chrtVal: %d, chrtVal180: %d, chrtPrevVal: %d, chrtPrevVal180: %d, wndLeftDlt: %d", i, chrtVal, chrtVal180, chrtPrevVal, chrtPrevVal180, wndLeftDlt);
                    if (((chrtPrevVal180 >= -180) && (chrtPrevVal180 < -90) && (chrtVal180 > 90)) || ((chrtPrevVal180 <= 179) && (chrtPrevVal180 > 90) && chrtVal180 <= -90)) {
                        // If current value crosses chart borders, compared to previous value, split line
                        int xSplit = (((chrtPrevVal180 > 0 ? wndRight : wndLeft) - wndLeft + 360) % 360) * chrtScl;
                        getdisplay().drawLine(prevX, prevY, xSplit, y, commonData->fgcolor);
                        getdisplay().drawLine(prevX, prevY - 1, ((xSplit != prevX) ? xSplit : xSplit - 1), ((xSplit != prevX) ? y - 1 : y), commonData->fgcolor);
                        prevX = (((chrtVal180 > 0 ? wndRight : wndLeft) - wndLeft + 360) % 360) * chrtScl;
                        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Cross: i: %d, chrtVal: %d, chrtPrevVal: %d, wndLeft: %d wndRight: %d, curr:{%d,%d} prev:{%d,%d}", i, chrtVal, chrtPrevVal, wndLeft, wndRight, x, y, prevX, prevY);
                    }
                }

                // Draw line with 2 pixels width + make sure vertical line are drawn correctly
                getdisplay().drawLine(prevX, prevY, x, y, commonData->fgcolor);
                getdisplay().drawLine(prevX, prevY - 1, ((x != prevX) ? x : x - 1), ((x != prevX) ? y - 1 : y), commonData->fgcolor);
                chrtPrevVal = chrtVal;
                prevX = x;
                prevY = y;

                if (i == (cHeight - 1)) { // Reaching chart area top end ()
                    linesToShow -= min(40, cHeight); // free top 40 lines of chart for new values
                    bufStart = max(0, count - (linesToShow * dataIntv)); // next start value in buffer to show
                    //    windDirHstry.mvStart(); // virtually delete 40 values from buffer
                    if ((windDirHstry.getMin(numWndValues) > wndCenter) || (windDirHstry.getMax(numWndValues) < wndCenter)) {
                        // Check if all wind value are left or right of center value -> optimize chart range
                        int mid = windDirHstry.getMid(numWndValues);
                        if (mid != INT_MIN) {
                            wndCenter = int((mid + (mid >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
                        }
                    }
                    LOG_DEBUG(GwLog::ERROR, "PageWindPlot FreeTop: cHeight: %d, LinesToShow: %d, numWndValues: %d, wndCenter: %d, bufStart: %d", cHeight, linesToShow, numWndValues, wndCenter, bufStart);
                    break;
                }
            }
            //            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot chart end: chrtVal: %d, x: %d, y: %d prevX: %d, prevY: %d, loop-Counter: %d", chrtVal, x, y, prevX, prevY, count);
        }

        else if (!wndDataValid) {
            // No valid data available
            LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
            getdisplay().setFont(&Ubuntu_Bold10pt7b);
            getdisplay().fillRect(xCenter - 66, height / 2 - 18, 146, 24, commonData->bgcolor); // Clear area for TWS value
            getdisplay().setCursor(xCenter - 66, height / 2);
            getdisplay().print("No sensor data");
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
            getdisplay().print(dataSValue[2]); // Value
            getdisplay().setFont(&Ubuntu_Bold12pt7b);
            getdisplay().setCursor(xPosTws + 82, yPosTws - 14);
            getdisplay().print(dataName[2]); // Name
            getdisplay().setFont(&Ubuntu_Bold8pt7b);
            getdisplay().setCursor(xPosTws + 78, yPosTws + 1);
            getdisplay().print(" ");
            if (holdValues == false) {
                getdisplay().print(dataUnit[2]); // Unit
            } else {
                getdisplay().print(dataUnitOld[2]); // Unit
            }
        }

        // chart Y axis labels; print last to overwrite potential chart lines in label area
        char sWndYAx[4]; // char buffer for wind Y axis labels
        int yPos;
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        for (int i = 1; i <= 3; i++) {
            if (numWndValues < intvBufSize) {
                // chart initially filled from botton to top -> revert minute labels
                yPos = yOffset + cHeight - (i * 60);
            } else {
                yPos = yOffset + (i * 60);
            }
            getdisplay().fillRect(0, yPos, width, 1, commonData->fgcolor);
            getdisplay().fillRect(0, yPos - 8, 26, 16, commonData->bgcolor); // Clear small area to remove potential chart lines
            getdisplay().setCursor(1, yPos + 4);
            snprintf(sWndYAx, 4, "%3d", i * dataIntv * -1);
            getdisplay().print(sWndYAx); // Wind value label
        }

        // Update display
        getdisplay().nextPage(); // Partial update (fast)
        unsigned long finish = millis() - start;
        LOG_DEBUG(GwLog::ERROR, "PageWindPlot Time: %lu", finish);
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
