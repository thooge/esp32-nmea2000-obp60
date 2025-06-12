#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "Pagedata.h"

#include <vector>

// ****************************************************************
class wndHistory {
    // provides a FiFo circular buffer to store wind history values
private:
    int SIZE;
    std::vector<int> buffer;
    int first = 0; // points to the first valid element
    int head = 0; // points to the next insertion index
    int count = 0; // number of valid elements

public:
    bool begin(int size)
    // specifies buffer size
    {
        if (size <= 0 || size > 1000) {
            return false;
        }
        SIZE = size;
        buffer.resize(size, INT_MIN); // allocate buffer
        return true;
    }

    void add(int value)
    // Add a new value; store in [0..360 deg] format
    {
        buffer[head] = value;
        head = (head + 1) % SIZE;
        if (count < SIZE) {
            count++;
        } else {
            first = head - 1; // When buffer is full, first points to the oldest value
        }
    }

    int get(int index) const
    // Get value by index in [0..360 deg] format (0 = oldest, count-1 = newest)
    {
        int realIndex;

        if (index < 0 || index >= count) {
            return -1; // Invalid index
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
            // Return value in [-180..180 deg] format
            return -1;
        }
    }

    int getSize() const
    // Get number of valid elements
    {
        return count;
    }

    int getMin() const
    // Get minimum value in the buffer
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

    int getMax() const
    // Get maximum value in the buffer
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

    int getMid() const
    // Get middle value in the buffer
    {
        if (count == 0) {
            return -1; // Buffer is empty
        }
        return (getMin() + getMax()) / 2;
    }

    int getRng(int center)
    // Get range of values in the buffer relative to a center value
    {
        if (count == 0) {
            return -1; // Buffer is empty
        }

        int min = getMin();
        int max = getMax();
        //        int rng = std::max(abs((min - center + 540) % 360 - 180), abs((max - center + 540) % 360 - 180));
        int rng = std::max(abs(min - center), abs(max - center));
        if (rng > 180) { // should never happen, but just in case
            rng = 180;
        }
        return rng;
    }

    void mvStart(int start)
    // Move the start index of buffer forward by <start> positions
    {
        first = (first + start) % SIZE;
        if (count > start)
            count -= start;
        else
            count = 0;
    }

    // TWA, TWS, HDM, AWA, AWS, STW
    bool calcTWD(int* twd, float twa, float tws, float hdm, float awa, float aws, float stw)
    // Calculate TWD based on other boat data values
    {
        if (count == 0) {
            return false; // Buffer is empty
        }

        int twdMin = getMin();
        int twdMax = getMax();
        int twdMid = getMid();

        // Calculate TWD based on TWA and HDM
        return true;
    }
};

// ****************************************************************
class PageWindPlot : public Page {

    bool keylock = false; // Keylock
    //    int16_t lp = 80; // Pointer length
    char mode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS
    int updTime = 1; // Update interval for wind history chart:
                     // (1)|(2)|(3)|(5) seconds for 3, 7, 10, 15 min. history chart
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
            if (mode == 'D') {
                mode = 'S';
            } else {
                mode = 'D';
            }
            // setupKeys(); // Update key labels
            return 0; // Commit the key
        }

        // Set interval for wind history chart update time
        if (key == 2) {
            if (updTime == 1) {
                updTime = 2;
            } else if (updTime == 2) {
                updTime = 3;
            } else if (updTime == 3) {
                updTime = 5;
            } else {
                updTime = 1;
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

        static wndHistory windValues; // Circular buffer to store wind values

        GwApi::BoatValue* bvalue;
        const int numCfgValues = 9;
        String dataName[numCfgValues];
        double dataValue[numCfgValues];
        bool dataValid[numCfgValues];
        String dataSValue[numCfgValues];
        String dataUnit[numCfgValues];
        String dataSValueOld[numCfgValues];
        String dataUnitOld[numCfgValues];
        bool wndDataValid = false; // Flag to indicate if wind data is valid
        bool simulation = false;
        bool holdValues = false;

        int width = getdisplay().width(); // Get screen width
        int height = getdisplay().height(); // Get screen height
        static const int yOffset = 48; // Offset for y coordinates of chart area
        int cHeight = height - yOffset - 22; // height of chart area
        // int cHeight = 60; // height of chart area
        int xCenter = width / 2; // Center of screen in x direction
        // static bool plotShift = false; // Flag to indicate if chartplot data have been shifted
        static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

        static int wndCenter = -400; // chart wind center value position; init value indicates that wndCenter is not set yet
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value
        bool rngFlipped = false; // Flag to indicate if range exceeds 180 degrees
        static int simWnd = 0; // Simulation value for wind data
        static float simTWS = 0; // Simulation value for TWS data
        static const int simStep = 10; // Simulation step for wind data

        int x, y; // x and y coordinates for drawing
        static int prevX, prevY; // Last x and y coordinates for drawing
        static float chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        static int chrtPrevVal; // Last wind value in chart area for check if value crosses 180 degree line
        int count; // index for next wind value in buffer

        static int updCnt = 0; // update counter for wind history chart in seconds
        bool isTimeforUpd = true; // Flag to indicate if it is time for chart update
        bool TwsFlipped = false; // Flag to indicate if TWS value flipped

        LOG_DEBUG(GwLog::LOG, "Display page WindPlot");

        if (windValues.getSize() == 0) {
            if (!windValues.begin(cHeight)) {
                logger->logDebug(GwLog::ERROR, "Failed to initialize wind values buffer");
                return;
            }
        }

        if (updCnt < updTime) {
            // Next update interval not reached yet
            updCnt++;
            isTimeforUpd = false;
        } else {
            isTimeforUpd = true;
            updCnt = 1; // Data update is now; reset counter
        }

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        simulation = config->getBool(config->useSimuData);
        holdValues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        // Read boatdata values for TWD, TWA, TWS, HDM, AWA, AWS, STW, if available
        for (int i = 0; i < numCfgValues; i++) {
            bvalue = pageData.values[i];
            dataName[i] = xdrDelete(bvalue->getName());
            dataName[i] = dataName[i].substring(0, 6); // String length limit for value name
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

        // Store TWD wind value in buffer
        int twdValue = 0;
        if (dataValid[0]) { // TWD data existing
            twdValue = int((dataValue[0] * radToDeg) + 0.5); // Read TWD value in degrees and round to integer
            wndDataValid = true;
        } else {
            // Try to calculate TWD value from other data, if available
            //    wndDataValid = windValues.calcTWD(&twdValue, dataValue[1], dataValue[2], dataValue[3], dataValue[4], dataValue[5], dataValue[6]);
        }

        // ************* falsche Position <isTimeforUpd> ****************
        if (isTimeforUpd) {
            if (wndDataValid) {
                windValues.add(twdValue);
            }

            if (simulation) {
                // Simulate data if simulation is enabled; use default simulation values for TWS
                simWnd += random(simStep * -1, simStep); // random value between -simStep and +simStep
                if (simWnd < 0)
                    simWnd += 360;
                simWnd = simWnd % 360;
                windValues.add(simWnd);
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot simulation data: windValue: %d, windSpeed: %s", simWnd, dataSValue[2].c_str());
            }
        }
        count = windValues.getSize(); // Get number of valid elements in buffer; maximum is cHeight
        LOG_DEBUG(GwLog::ERROR, "PageWindPlot: Data 0 valid - dataValue[0]: %f, TWD: %d, cnt: %d, valid0: %d", dataValue[0] * radToDeg, twdValue, count, dataValid[0]);

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // if (bvalue == NULL)
        //     return;
        LOG_DEBUG(GwLog::LOG, "PageWindPlot, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f, %s:%f, %s:%f, %s:%f, %s:%f, cnt: %d, valid0: %d", dataName[0].c_str(), dataValue[0],
            dataName[1].c_str(), dataValue[1], dataName[2].c_str(), dataValue[2], dataName[3].c_str(), dataValue[3], dataName[4].c_str(), dataValue[4],
            dataName[5].c_str(), dataValue[5], dataName[6].c_str(), dataValue[6], dataName[7].c_str(), dataValue[7], dataName[8].c_str(), dataValue[8], count, dataValid[0]);

        // initialize chart range values
        if (wndCenter == -400) {
            wndCenter = (windValues.get(0) < 0 ? 0 : windValues.get(0));
            wndCenter = int((wndCenter + (wndCenter >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
            diffRng = 30;
            chrtRng = 30;
            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot initialized. wndCenter: %d, chrtRng: %d ", wndCenter, chrtRng);

        } else {
            diffRng = windValues.getRng(wndCenter);
            diffRng = (diffRng < 0 ? 0 : diffRng); // If no data in buffer, set range to 0
            if (diffRng > chrtRng) {
                chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 degree value
            } else if (diffRng + 10 < chrtRng) { // Reduce chart range for higher resolution if possible
                chrtRng = max(30, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10);
            }
            LOG_DEBUG(GwLog::ERROR, "PageWindPlot range adjusted. wndCenter: %d, chrtRng: %d ", wndCenter, chrtRng);
        }
        chrtScl = float(width) / float(chrtRng) / 2.0; // chart scale: pixels per degree
        wndLeft = wndCenter - chrtRng;
        if (wndLeft < 0)
            wndLeft += 360;
        wndRight = wndCenter + chrtRng;
        if (wndRight >= 360)
            wndRight -= 360;
        LOG_DEBUG(GwLog::ERROR, "PageWindPlot dataValue[0]: %f, windValue: %d, count: %d, diffRng: %d, chartRng: %d, Center: %d, scale: %f", double(dataValue[0] * radToDeg),
            (!windValues.get(count - 1) < 0 ? 0 : windValues.get(count - 1)), count, diffRng, chrtRng, wndCenter, chrtScl);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // Horizontal top line for orientation -> to be deleted
        // getdisplay().fillRect(0, 20, width, 1, commonData->fgcolor);

        // chart lines
        getdisplay().fillRect(0, yOffset, width, 2, commonData->fgcolor);
        getdisplay().fillRect(xCenter - 1, yOffset, 2, cHeight, commonData->fgcolor);

        // chart labels
        char sWndLbl[4]; // char buffer for Wind angle label
        getdisplay().setFont(&Ubuntu_Bold12pt7b);
        getdisplay().setCursor(xCenter - 88, yOffset - 3);
        getdisplay().print("TWD"); // Wind name
        getdisplay().setCursor(xCenter - 20, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndCenter < 0) ? (wndCenter + 360) : wndCenter);
        getdisplay().print(sWndLbl); // Wind center value
        getdisplay().drawCircle(xCenter + 25, yOffset - 16, 2, commonData->fgcolor); // <degree> symbol  63
        getdisplay().drawCircle(xCenter + 25, yOffset - 16, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(2, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndLeft < 0) ? (wndLeft + 360) : wndLeft);
        getdisplay().print(sWndLbl); // Wind left value
        getdisplay().drawCircle(47, yOffset - 16, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(47, yOffset - 16, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 51, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndRight < 0) ? (wndRight + 360) : wndRight);
        getdisplay().print(sWndLbl); // Wind right value
        getdisplay().drawCircle(width - 5, yOffset - 16, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 5, yOffset - 16, 3, commonData->fgcolor); // <degree> symbol

        // Draw wind values in chart
        //***********************************************************
        if (wndDataValid || holdValues || simulation) {
            LOG_DEBUG(GwLog::ERROR, "PageWindPlot Draw: prevX: %d, chrtPrevVal: %d, wndLeft: %d, chrtScl: %f, count: %d", prevX, chrtPrevVal, wndLeft, chrtScl, count);

            for (int i = 0; i < count; i++) {
                chrtVal = windValues.get(i);
                x = ((chrtVal - wndLeft + 360) % 360) * chrtScl;
                y = yOffset + cHeight - i; // Position in chart area

                if ((abs(chrtVal - wndCenter) > 180) && !rngFlipped) { // If range exceeds 180 degrees, value plotted on other side of chart
                    rngFlipped = true;
                    prevX = x; // don't print connecting line to previous value
                    prevY = y;
                }
                if (i == 0) { // just a dot for 1st chart point
                    prevX = x;
                    prevY = y;
                }
//                if (i < 30)
//                    LOG_DEBUG(GwLog::ERROR, "PageWindPlot Chart: x: %d, y: %d prevX: %d, prevY: %d, loop-Counter: %d", x, y, prevX, prevY, count);
                // Draw line with 2 pixels width + make sure vertical line are drawn correctly
                getdisplay().drawLine(prevX, prevY, x, y, commonData->fgcolor);
                getdisplay().drawLine(prevX, prevY - 1, ((x != prevX) ? x : x - 1), ((x != prevX) ? y - 1 : y), commonData->fgcolor);
                // chrtPrevVal = chrtVal;
                prevX = x;
                prevY = y;

                if (i == (cHeight - 1)) { // Reaching chart area top end
                    windValues.mvStart(40); // virtually delete 40 values from buffer
                    if ((windValues.getMin() > wndCenter) || (windValues.getMax() < wndCenter)) {
                        // Check if all wind value are left or right of center value -> optimize chart range
                        int mid = windValues.getMid();
                        wndCenter = int((mid + (mid >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
                        rngFlipped = false; // chart value within standard 180 degree range
                        chrtPrevVal = (windValues.get(0) < 0 ? 0 : windValues.get(0));
                    }
                    LOG_DEBUG(GwLog::ERROR, "PageWindPlot Shift: Min: %d, Max: %d, Mid: %d, new Center: %d", windValues.getMin(), windValues.getMax(), windValues.getMid(), wndCenter);
                    break;
                }
            }
            LOG_DEBUG(GwLog::ERROR, "PageWindPlot chart end: x: %d, y: %d prevX: %d, prevY: %d, chrtPrevVal: %d, loop-Counter: %d", x, y, prevX, prevY, chrtPrevVal, count);

        } else if (!wndDataValid) {
            // No valid data available
            LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
            getdisplay().setFont(&Ubuntu_Bold10pt7b);
            getdisplay().fillRect(xCenter - 66, height / 2 - 18, 146, 24, commonData->bgcolor); // Clear area for TWS value
            getdisplay().setCursor(xCenter - 66, height / 2);
            getdisplay().print("No sensor data");
        }

        // Print TWS value
        if (showTWS) {
            int xPosTws = width - 145;
            int yPosTws = yOffset + 40;
            if ((prevY > yPosTws - 36) && (prevY < yPosTws) && (prevX > xPosTws) && !TwsFlipped) {
                // If chart line enters TWS value area, move TWS value to the other side
                xPosTws = (xPosTws == width - 145) ? 30 : width - 145;
                TwsFlipped = true;
            }
            TwsFlipped = false; // Reset flag for next display update
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
        for (int i = 3; i > 0; i--) {
            yPos = yOffset + cHeight - (i * 60) + 14; // Y position for label
            getdisplay().fillRect(0, yPos, width, 1, commonData->fgcolor);
            getdisplay().fillRect(0, yPos - 6, 26, 15, commonData->bgcolor); // Clear small area to remove potential chart lines
            getdisplay().fillRect(0, yPos, 8, 2, commonData->fgcolor);
            getdisplay().setCursor(9, yPos + 4);
            snprintf(sWndYAx, 4, "%2d", i * updTime);
            getdisplay().print(sWndYAx); // Wind value label
        }

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
