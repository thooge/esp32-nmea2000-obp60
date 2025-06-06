#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "Pagedata.h"

#include <vector>

class CircularBuffer {
    // provides a circular buffer to store wind history values
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
        buffer[head] = value;
        head = (head + 1) % SIZE;
        if (count < SIZE) {
            count++;
        } else {
            first = head - 1; // When buffer is full, first points to the oldest value
        }
    }

    int get(int index) const
    // Get value by index (0 = oldest, count-1 = newest)
    {
        int realIndex;

        if (index < 0 || index >= count) {
            return -1; // Invalid index
        }
        realIndex = (first + index) % SIZE;
        return buffer[realIndex];
    }

    int size() const
    // Get number of valid elements
    {
        return count;
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
        int cHeight = height - 98; // height of chart area
        int xCenter = width / 2; // Center of screen in x direction
        static const int yOffset = 76; // Offset for y coordinates of chart area
        static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

        static int wndCenter = -400; // chart wind center value position; init value indicates that wndCenter is not set yet
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value

        int x, y; // x and y coordinates for drawing
        static int lastX, lastY; // Last x and y coordinates for drawing
        static int chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        int count; // index for next wind value in buffer

        // Circular buffer to store wind values
        static CircularBuffer windValues;
        if (windValues.size() == 0) {
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
            //            dataFormat[i] = bvalue->getFormat(); // Unit of value
            if (dataValid[i]) {
                dataSValueOld[i] = dataSValue[i]; // Save old value
                dataUnitOld[i] = dataUnit[i]; // Save old unit
            }
        }

        // Store TWD wind value in buffer
        windValues.add(int((dataValue[0] * radToDeg) + 0.5)); // Store TWD value (degree) in buffer (rounded to integer)
        count = windValues.size(); // Get number of valid elements in buffer; maximum is cHeight

        // specify and check chart border values
        if (wndCenter == -400) {
            wndCenter = windValues.get(0);
            chrtRng = 20;
            wndLeft = wndCenter - chrtRng;
            if (wndLeft < 0)
                wndLeft += 360;
            wndRight = wndCenter + chrtRng;
            if (wndRight >= 360)
                wndRight -= 360;
            LOG_DEBUG(GwLog::LOG, "PageWindPlot: wndCenter + chrtRng initialized");
        }
        diffRng = abs(windValues.get(count - 1) - wndCenter);
        if (diffRng > chrtRng) {
            chrtRng = int(ceil(diffRng / 10.0) * 10); // Round to next 10 degree value
            wndLeft = wndCenter - chrtRng;
            if (wndLeft < 0)
                wndLeft += 360;
            wndRight = wndCenter + chrtRng;
            if (wndRight >= 360)
                wndRight -= 360;
        }
        LOG_DEBUG(GwLog::LOG, "PageWindPlot dataValue[0]: %f, windValue: %d, count: %d, Range: %d, ChartRng: %d", float(dataValue[0] * radToDeg), windValues.get(count - 1), count, diffRng, chrtRng);

        // was, wenn alle Werte kleiner als current wind range sind?
        // passe wndCenter an, wenn chrtRng > std und alle Werte > oder < wndCenter sind

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Logging boat values
        if (bvalue == NULL)
            return;
        //        LOG_DEBUG(GwLog::LOG, "PageWindPlot, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f, cnt: %d, wind: %f", dataName[0].c_str(), dataValue[0], name2.c_str(),
        //            value2, name3.c_str(), value3, name4.c_str(), value4, name5.c_str(), value5, count, windValues.get(count - 1));

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
        getdisplay().setCursor(xCenter - 68, yOffset - 3);
        getdisplay().print(dataName[0]); // Wind name
        getdisplay().setCursor(xCenter - 16, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", wndCenter);
        getdisplay().print(sWndLbl); // Wind center value
        getdisplay().drawCircle(xCenter + 21, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(xCenter + 21, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(2, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", wndLeft);
        getdisplay().print(sWndLbl); // Wind left value
        getdisplay().drawCircle(39, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(39, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 43, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", wndRight);
        getdisplay().print(sWndLbl); // Wind right value
        getdisplay().drawCircle(width - 6, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 6, 63, 3, commonData->fgcolor); // <degree> symbol

        // Draw wind values in chart
        //***********************************************************
        lastX = xCenter + ((windValues.get(0) - wndCenter) * chrtScl);
        lastY = yOffset + cHeight; // Reset lastY to bottom of chart

        for (int i = 0; i < count; i++) {
            chrtVal = windValues.get(i); // Get value from buffer
            chrtScl = xCenter / chrtRng; // current scale
            x = xCenter + ((chrtVal - wndCenter) * chrtScl); // Scale to chart width
            y = yOffset + cHeight - i; // Position in chart area
            getdisplay().drawLine(lastX, lastY, x, y, commonData->fgcolor);
            getdisplay().drawLine(lastX, lastY - 1, x, y - 1, commonData->fgcolor);
            lastX = x;
            lastY = y;
            //            LOG_DEBUG(GwLog::LOG, "PageWindPlot: loop-Counter: %d, X: %d, Y: %d, lastX: %d, lastY: %d", count, x, y, lastX, lastY);
            if (i == (cHeight - 1)) { // Reaching chart area top end
                windValues.mvStart(40); // virtually delete 40 values from buffer
                continue;
            }
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
