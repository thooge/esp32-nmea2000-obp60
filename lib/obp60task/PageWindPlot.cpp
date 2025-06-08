#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "Pagedata.h"

#include <vector>

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
        buffer.resize(size); // allocate buffer
        return true;
    }

    void add(int value)
    // Add a new value
    {
        if (value > 180) {
            value -= 360; // Normalize value to -180..180 to make min/max calculations working
        }
        buffer[head] = value;
        head = (head + 1) % SIZE;
        if (count < SIZE) {
            count++;
        } else {
            first = head - 1; // When buffer is full, first points to the oldest value
        }
    }

    int get(int index) const
    // Get value by index in [-180..180 deg] format (0 = oldest, count-1 = newest)
    {
        int realIndex;

        if (index < 0 || index >= count) {
            return -1; // Invalid index
        }
        realIndex = (first + index) % SIZE;
        return buffer[realIndex];
        //        }
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

    void mvStart(int start)
    // Move the start index of buffer forward by <start> positions
    {
        first = (first + start) % SIZE;
        if (count > start)
            count -= start;
        else
            count = 0;
    }
};

class PageWindPlot : public Page {
public:
    PageWindPlot(CommonData& common)
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG, "Instantiate PageWindPlot");
    }

    // Key functions
    virtual int handleKey(int key)
    {
        // Code for keylock
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayPage(PageData& pageData)
    {
        GwConfigHandler* config = commonData->config;
        GwLog* logger = commonData->logger;

        const int numCfgValues = 5;
        GwApi::BoatValue* bvalue;
        String dataName[numCfgValues];
        double dataValue[numCfgValues];
        bool dataValid[numCfgValues];
        String dataSValue[numCfgValues];
        String dataUnit[numCfgValues];
        String dataSValueOld[numCfgValues];
        String dataUnitOld[numCfgValues];
        //        String dataFormat[numCfgValues];

        int width = getdisplay().width(); // Get screen width
        int height = getdisplay().height(); // Get screen height
        //        int cHeight = height - 98; // height of chart area
        int cHeight = 80; // height of chart area
        int xCenter = width / 2; // Center of screen in x direction
        static const int yOffset = 76; // Offset for y coordinates of chart area
        static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

        static int wndCenter = -400; // chart wind center value position; init value indicates that wndCenter is not set yet
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value
        static bool plotShift = false; // Flag to indicate if chartplot data have been shifted

        int x, y; // x and y coordinates for drawing
        static int lastX, lastY; // Last x and y coordinates for drawing
        static int chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        int count; // index for next wind value in buffer

        // Circular buffer to store wind values
        static wndHistory windValues;
        if (windValues.getSize() == 0) {
            if (!windValues.begin(cHeight)) { // buffer holds as many values as the height of the chart area
                //                if (!windValues.begin(60)) { // buffer holds as many values as the height of the chart area
                logger->logDebug(GwLog::ERROR, "Failed to initialize wind values buffer");
                return;
            }
        }

        LOG_DEBUG(GwLog::LOG, "Display page WindPlot");

        /*        // Fill windValues buffer with sequential values from 1 to 60 and back to 1 for cHeight values
                int num = 60;
                windValues.begin(num + 20);
                int seq = 1;
                int dir = 1;
                for (int i = 0; i < num; i++) {
                    windValues.add(seq);
                    seq += dir;
                    if (seq == 60)
                        dir = -1;
                    if (seq == 1)
                        dir = 1;
                }

                for (int i = 0; i < num; i += 4) {
                    LOG_DEBUG(GwLog::LOG, "WindValues[%d]: %d;  [%d]: %d;    [%d]: %d    [%d]: %d", i, windValues.get(i), i + 1, windValues.get(i + 1), i + 2, windValues.get(i + 2), i + 3, windValues.get(i + 3));
                }
                windValues.mvStart(20);
                for (int i = 0; i < num; i += 4) {
                    LOG_DEBUG(GwLog::LOG, "Shifted WindValues[%d]: %d;  [%d]: %d;    [%d]: %d    [%d]: %d", i, windValues.get(i), i + 1, windValues.get(i + 1), i + 2, windValues.get(i + 2), i + 3, windValues.get(i + 3));
                }
                return; // Return early for testing purposes
        */

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

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

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Store TWD wind value in buffer
        windValues.add(int((dataValue[0] * radToDeg) + 0.5)); // Store TWD value (degree) in buffer (rounded to integer)
        count = windValues.getSize(); // Get number of valid elements in buffer; maximum is cHeight

        // initialize chart range values
        if (wndCenter == -400) {
            wndCenter = windValues.get(0);
            wndCenter = int((wndCenter + (wndCenter >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
//            wndCenter = int((windValues.get(0) + 5) / 10) * 10; // Round to nearest 10 degree value
            diffRng = 30;
            chrtRng = 30;
            LOG_DEBUG(GwLog::LOG, "PageWindPlot initialized. wndCenter: %d, chrtRng: %d ", wndCenter, chrtRng);
        } else {
            diffRng = max(abs(((windValues.getMax() - wndCenter + 540) % 360) - 180), abs(((windValues.getMin() - wndCenter + 540) % 360) - 180)); // check necessary range size
            if (diffRng > chrtRng) {
                chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 degree value
//                chrtRng = int(ceil(diffRng / 10.0) * 10); // Round to next 10 degree value
            } else if (diffRng + 10 < chrtRng) {
                chrtRng = max(30, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10); // Round up to next 10 degree value
//                chrtRng = max(30, int(ceil(diffRng / 10.0) * 10)); // Round to next 10 degree value, but mimimum range is 30 degrees
            }
        }
        wndLeft = wndCenter - chrtRng;
        if (wndLeft < -180)
            wndLeft += 360;
        wndRight = wndCenter + chrtRng;
        if (wndRight >= 180)
            wndRight -= 360;
        LOG_DEBUG(GwLog::LOG, "PageWindPlot dataValue[0]: %f, windValue: %d, count: %d, diffRng: %d, chartRng: %d", float(dataValue[0] * radToDeg), windValues.get(count - 1), count, diffRng, chrtRng);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // Horizontal top line for orientation -> to be deleted
        // getdisplay().fillRect(0, 20, width, 1, commonData->fgcolor);
        // Show TWS value on top right
        getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
        getdisplay().setCursor(252, 52);
        getdisplay().print(dataSValue[1]); // Value
        getdisplay().setFont(&Ubuntu_Bold12pt7b);
        getdisplay().setCursor(334, 38);
        getdisplay().print(dataName[1]); // Name
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(330, 53);
        getdisplay().print(" ");
        if (holdvalues == false) {
            getdisplay().print(dataUnit[1]); // Unit
        } else {
            getdisplay().print(dataUnitOld[1]); // Unit
        }

        // chart lines
        getdisplay().fillRect(0, yOffset, width, 2, commonData->fgcolor);
        getdisplay().fillRect(xCenter - 1, yOffset, 2, cHeight, commonData->fgcolor);

        // chart labels
        char sWndLbl[4]; // Wind label
        getdisplay().setFont(&Ubuntu_Bold10pt7b);
        getdisplay().setCursor(xCenter - 80, yOffset - 3);
        getdisplay().print("TWD"); // Wind name
        getdisplay().setCursor(xCenter - 16, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndCenter < 0) ? (wndCenter + 360) : wndCenter);
        getdisplay().print(sWndLbl); // Wind center value
        getdisplay().drawCircle(xCenter + 21, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(xCenter + 21, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(2, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndLeft < 0) ? (wndLeft + 360) : wndLeft);
        getdisplay().print(sWndLbl); // Wind left value
        getdisplay().drawCircle(39, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(39, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 43, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndRight < 0) ? (wndRight + 360) : wndRight);
        getdisplay().print(sWndLbl); // Wind right value
        getdisplay().drawCircle(width - 6, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 6, 63, 3, commonData->fgcolor); // <degree> symbol

        // Draw wind values in chart
        //***********************************************************
        if (dataValid[0] || holdvalues || simulation == true) {

            lastX = xCenter + ((windValues.get(0) - wndCenter) * chrtScl);
            lastY = yOffset + cHeight; // Reset lastY to bottom of chart

            for (int i = 0; i < count; i++) {
                chrtVal = windValues.get(i); // Get value from buffer
                chrtScl = xCenter / chrtRng; // current scale: pixels per degree
                x = xCenter + ((chrtVal - wndCenter) * chrtScl); // Scale to chart width
                //            x = xCenter + ((((chrtVal - wndCenter + 540) % 360) - 180) * chrtScl); // Scale to chart width
                y = yOffset + cHeight - i; // Position in chart area
                // Draw line with 2 pixels width; make sure vertical line are drawn correctly
                getdisplay().drawLine(lastX, lastY, x, y, commonData->fgcolor);
                getdisplay().drawLine(lastX, lastY - 1, (x != lastX) ? x : x-1, (x != lastX) ? y - 1 : y, commonData->fgcolor);
                lastX = x;
                lastY = y;
                //            LOG_DEBUG(GwLog::LOG, "PageWindPlot: loop-Counter: %d, X: %d, Y: %d, lastX: %d, lastY: %d", count, x, y, lastX, lastY);
                LOG_DEBUG(GwLog::LOG, "PageWindPlot Shift: Min: %d, Max: %d, Mid: %d", windValues.getMin(), windValues.getMax(), windValues.getMid());
                if (i == (cHeight - 1)) { // Reaching chart area top end
                    windValues.mvStart(40); // virtually delete 40 values from buffer
                    LOG_DEBUG(GwLog::LOG, "PageWindPlot Shift: Min: %d, Max: %d, Mid: %d", windValues.getMin(), windValues.getMax(), windValues.getMid());
                    if ((windValues.getMin() > wndCenter) || (windValues.getMax() < wndCenter)) {
                        int mid = windValues.getMid();
                        wndCenter = int((mid + (mid >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
                        LOG_DEBUG(GwLog::LOG, "PageWindPlot Shift: Min: %d, Max: %d, new Center: %d", windValues.getMin(), windValues.getMax(), wndCenter);
                    }
                    continue; // Will leave loop
                }
            }
        } else {
            getdisplay().setFont(&Ubuntu_Bold8pt7b);
            getdisplay().setCursor(xCenter - 55, height / 2);
            getdisplay().print("No sensor data");
            return;
        }
        LOG_DEBUG(GwLog::LOG, "PageWindPlot End: lastX: %d, lastY: %d, loop-Counter: %d", lastX, lastY, count);

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
    { "TWD", "TWS", "TWA", "AWA", "HDM" }, // Bus values we need in the page
    true // Show display header on/off
);

#endif
