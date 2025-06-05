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
        if (count < SIZE)
            count++;
    }

    int get(int index) const
    // Get value by index (0 = oldest, count-1 = newest)
    {
        if (index < 0 || index >= count) {
            return -1; // Invalid index
        }
        int realIndex = (head + SIZE - count + index) % SIZE;
        return buffer[realIndex];
    }

    int size() const
    // Get number of valid elements
    {
        return count;
    }

    int getSIZE() const
    // Get number of SIZE
    {
        return SIZE;
    }

    void mvStart(int start)
    // Move the start index for the buffer forward by <start> positions
    {
        head = (head + start) % SIZE;
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

        static String svalue1old = "";
        static String unit1old = "";
        static String svalue2old = "";
        static String unit2old = "";
        static String svalue3old = "";
        static String unit3old = "";
        static String svalue4old = "";
        static String unit4old = "";
        static String svalue5old = "";
        static String unit5old = "";

        int width = getdisplay().width(); // Get screen width
        int height = getdisplay().height(); // Get screen height
        int cHeight = height - 98; // height of chart area
        int xCenter = width / 2; // Center of screen in x direction
        static const int yOffset = 76; // Offset for y coordinate to center chart area vertically
        static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

        static int wndCenter = -400; // chart wind center value position; init value indicates that wndCenter is not set yet
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value
        static bool plotShift = false; // Flag to indicate if the plot has been shifted

        int x, y; // x and y coordinates for drawing
        static int lastX, lastY; // Last x and y coordinates for drawing
        int chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        int count; // index for next wind value in buffer

        static CircularBuffer windValues; // Circular buffer to store wind values
        if (windValues.size() == 0) {
            if (!windValues.begin(cHeight)) { // buffer holds as many values as the height of the chart area
                logger->logDebug(GwLog::ERROR, "Failed to initialize wind values buffer");
                return;
            }
        }

        LOG_DEBUG(GwLog::LOG, "Display page WindPlot, SIZE = %d", windValues.getSIZE());

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        // Get boat values for TWD
        GwApi::BoatValue* bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName()); // Value name
        name1 = name1.substring(0, 6); // String length limit for value name
        calibrationData.calibrateInstance(name1, bvalue1, logger); // Check if boat data value is to be calibrated
        double value1 = bvalue1->value; // Value as double in SI unit
        bool valid1 = bvalue1->valid; // Valid information
        value1 = formatValue(bvalue1, *commonData).value; // Format only nesaccery for simulation data for pointer
        String svalue1 = formatValue(bvalue1, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, *commonData).unit; // Unit of value
        if (valid1 == true) {
            svalue1old = svalue1; // Save old value
            unit1old = unit1; // Save old unit
        }

        // Get boat values for TWS
        GwApi::BoatValue* bvalue2 = pageData.values[1]; // First element in list (only one value by PageOneValue)
        String name2 = xdrDelete(bvalue2->getName()); // Value name
        name2 = name2.substring(0, 6); // String length limit for value name
        calibrationData.calibrateInstance(name2, bvalue2, logger); // Check if boat data value is to be calibrated
        double value2 = bvalue2->value; // Value as double in SI unit
        bool valid2 = bvalue2->valid; // Valid information
        String svalue2 = formatValue(bvalue2, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit2 = formatValue(bvalue2, *commonData).unit; // Unit of value
        if (valid2 == true) {
            svalue2old = svalue2; // Save old value
            unit2old = unit2; // Save old unit
        }

        // Get boat values TWA
        GwApi::BoatValue* bvalue3 = pageData.values[2]; // Second element in list (only one value by PageOneValue)
        String name3 = xdrDelete(bvalue3->getName()); // Value name
        name3 = name3.substring(0, 6); // String length limit for value name
        calibrationData.calibrateInstance(name3, bvalue3, logger); // Check if boat data value is to be calibrated
        double value3 = bvalue3->value; // Value as double in SI unit
        bool valid3 = bvalue3->valid; // Valid information
        String svalue3 = formatValue(bvalue3, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit3 = formatValue(bvalue3, *commonData).unit; // Unit of value
        if (valid3 == true) {
            svalue3old = svalue3; // Save old value
            unit3old = unit3; // Save old unit
        }

        // Get boat values AWA
        GwApi::BoatValue* bvalue4 = pageData.values[3]; // Second element in list (only one value by PageOneValue)
        String name4 = xdrDelete(bvalue4->getName()); // Value name
        name4 = name4.substring(0, 6); // String length limit for value name
        calibrationData.calibrateInstance(name4, bvalue4, logger); // Check if boat data value is to be calibrated
        double value4 = bvalue4->value; // Value as double in SI unit
        bool valid4 = bvalue4->valid; // Valid information
        String svalue4 = formatValue(bvalue4, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit4 = formatValue(bvalue4, *commonData).unit; // Unit of value
        if (valid4 == true) {
            svalue4old = svalue4; // Save old value
            unit4old = unit4; // Save old unit
        }

        // Get boat values HDM
        GwApi::BoatValue* bvalue5 = pageData.values[4]; // Second element in list (only one value by PageOneValue)
        String name5 = xdrDelete(bvalue5->getName()); // Value name
        name5 = name5.substring(0, 6); // String length limit for value name
        calibrationData.calibrateInstance(name5, bvalue5, logger); // Check if boat data value is to be calibrated
        double value5 = bvalue5->value; // Value as double in SI unit
        bool valid5 = bvalue5->valid; // Valid information
        String svalue5 = formatValue(bvalue5, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit5 = formatValue(bvalue5, *commonData).unit; // Unit of value
        if (valid5 == true) {
            svalue5old = svalue5; // Save old value
            unit5old = unit5; // Save old unit
        }

        // Store wind value in buffer
        windValues.add(int((value3 * radToDeg) + 0.5)); // Store TWA value (degree) in buffer (rounded to integer)
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
        LOG_DEBUG(GwLog::LOG, "PageWindPlot Value3: %f, windValue: %d, count: %d, Range: %d, ChartRng: %d", float(value3 * radToDeg), windValues.get(count - 1), count, diffRng, chrtRng);

        // was, wenn alle Werte kleiner als current wind range sind?
        // passe wndCenter an, wenn chrtRng > std und alle Werte > oder < wndCenter sind

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Logging boat values
        if (bvalue1 == NULL)
            return;
        LOG_DEBUG(GwLog::LOG, "PageWindPlot, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f, cnt: %d, wind: %f", name1.c_str(), value1, name2.c_str(),
            value2, name3.c_str(), value3, name4.c_str(), value4, name5.c_str(), value5, count, windValues.get(count - 1));

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        getdisplay().fillRect(0, 20, width, 1, commonData->fgcolor); // Horizontal top line for orientation -> to be deleted

        // Show TWS value on top right
        getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
        getdisplay().setCursor(252, 58);
        getdisplay().print(svalue2); // Value
        getdisplay().setFont(&Ubuntu_Bold12pt7b);
        getdisplay().setCursor(334, 48);
        getdisplay().print(name2); // Name
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(330, 59);
        getdisplay().print(" ");
        if (holdvalues == false) {
            getdisplay().print(unit2); // Unit
        } else {
            getdisplay().print(unit2old); // Unit
        }

        // chart lines
        getdisplay().fillRect(0, yOffset, width, 2, commonData->fgcolor);
        getdisplay().fillRect(xCenter - 1, yOffset, 2, cHeight, commonData->fgcolor);

        // chart labels
        getdisplay().setFont(&Ubuntu_Bold10pt7b);
        getdisplay().setCursor(xCenter - 68, yOffset - 3);
        getdisplay().print(name3); // Wind name
        getdisplay().setCursor(xCenter - 18, yOffset - 3);
        getdisplay().print(wndCenter); // Wind center value
        getdisplay().drawCircle(xCenter + 19, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(xCenter + 19, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(5, yOffset - 3);
        getdisplay().print(wndLeft); // Wind left value
        getdisplay().drawCircle(44, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(44, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 45, yOffset - 3);
        getdisplay().print(wndRight); // Wind right value
        getdisplay().drawCircle(width - 6, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 6, 63, 3, commonData->fgcolor); // <degree> symbol

        // Draw wind values in chart
        //***********************************************************
        if (plotShift) { // If plot was shifted, set lastX to 1st chart wind value
            lastX = windValues.get(0); // set to new start of buffer
            plotShift = false;
        } else {
            lastX = xCenter;
        }
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
            //            LOG_DEBUG(GwLog::LOG, "PageWindPlot: loop-Counter: %d, X: %d, Y: %d", count, x, y);
            if (i == (cHeight - 1)) { // Reaching chart area top end
                windValues.mvStart(40); // virtually delete 40 values from buffer
                plotShift = true; // Set flag to shift plot
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
