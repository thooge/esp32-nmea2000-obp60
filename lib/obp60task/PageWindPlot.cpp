#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "Pagedata.h"

class CircularBuffer {
    // provides a circular buffer to store wind history values
private:
    static const int SIZE = 202;
    int buffer[SIZE];
    int head = 0; // points to the next insertion index
    int count = 0; // number of valid elements

public:
    bool begin(int size)
    // Constructor specifies buffer size
    {
        if (size <= 0 || size > 1000) {
            return false;
        }
//        SIZE = size;
//        buffer = new int[SIZE]; // allocate buffer
        if (!buffer) {
            return false;
        } else {
            return true;
        }
    }

    ~CircularBuffer()
    {
//        delete[] buffer; // Free allocated memory
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
};

class PageWindPlot : public Page {
    //    int16_t lp = 80; // Pointer length

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
//        int yOffset = 76; // Offset for y coordinate to center chart area vertically
        int yOffset = 275; // Offset for y coordinate to center chart area vertically
        int x, y, lastX, lastY; // x and y coordinates for drawing
        static const float radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

        static int chartRng = 40; // Range of wind values from mid wind value to min/max wind value in degrees
        static int chartMidVal; // Mid wind value in degrees
        int chartScl; // Scale for wind values in pixels per degree
        int chartVal; // Current wind value
        int count; // index for next wind value in buffer

        static CircularBuffer windValues; // Circular buffer to store wind values
/*        if (!windValues.begin(cHeight)) { // buffer holds as many values as the height of the chart area
            logger->logDebug(GwLog::ERROR, "Failed to initialize wind values buffer");
            return;
        }
*/
        LOG_DEBUG(GwLog::LOG, "Display page WindPlot");

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
        windValues.add(int(value3 * radToDeg)); // Store TWA value (degree) in buffer
        count = windValues.size(); // Get number of valid elements in buffer; maximum is cHeight

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Logging boat values
        if (bvalue1 == NULL)
            return;
        LOG_DEBUG(GwLog::LOG, "PageWindPlot, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f, cnt: %d, wind: %f", name1.c_str(), value1, name2.c_str(), 
            value2, name3.c_str(), value3, name4.c_str(), value4, name5.c_str(), value5, count, windValues.get(count-1));

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        //        getdisplay().fillRect(0, 20, width, 1, commonData->fgcolor);  // Horizontal top line for orientation -> to be deleted

        // Show TWS value on top right
        getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
        getdisplay().setCursor(262, 58);
        getdisplay().print(svalue2); // Value
        getdisplay().setFont(&Ubuntu_Bold12pt7b);
        getdisplay().setCursor(344, 48);
        getdisplay().print(name2); // Name
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(340, 59);
        //        getdisplay().print(" ");
        if (holdvalues == false) {
            getdisplay().print(unit2); // Unit
        } else {
            getdisplay().print(unit2old); // Unit
        }

        // chart lines
        getdisplay().fillRect(0, yOffset, width, 2, commonData->fgcolor);
        getdisplay().fillRect(xCenter - 1, yOffset, 2, cHeight, commonData->fgcolor);

        // initial chart labels
        int twdCenter = windValues.get(0); // TWD center value position
        int twdLeft = twdCenter - 40; // TWD left value position
        int twdRight = twdCenter + 40; // TWD right value position
        getdisplay().setFont(&Ubuntu_Bold10pt7b);
        getdisplay().setCursor(xCenter - 15, 73);
        getdisplay().print(twdCenter); // TWD center value
        getdisplay().drawCircle(xCenter + 22, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(xCenter + 22, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(2, 73);
        getdisplay().print(twdLeft); // TWD left value
        getdisplay().drawCircle(40, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(40, 63, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 35, 73);
        getdisplay().print(twdRight); // TWD right value
        getdisplay().drawCircle(width - 5, 63, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 5, 63, 3, commonData->fgcolor); // <degree> symbol

        // Draw wind values in chart
        //***********************************************************

        chartMidVal = windValues.get(0); // Get 1st value from buffer for specifcation of chart middle value
        lastX = xCenter;
        lastY = 275 + count;
        LOG_DEBUG(GwLog::LOG, "PageWindPlot Start: lastX: %d, lastY: %d", lastX, lastY); 

        for (int i = 0; i < count; i++) {
            chartVal = windValues.get(i); // Get value from buffer
            chartScl = xCenter / chartRng; // current scale
            // Calculate x and y position for the pointer
            x = xCenter + ((chartVal - chartMidVal) * chartScl); // Scale to chart width
//            y = yOffset + (count - i); // Position in chart area
            y = 275 - i; // Position in chart area
            // Draw the pointer
            getdisplay().drawLine(lastX, lastY, x, y, commonData->fgcolor);
            getdisplay().drawLine(lastX, lastY-1, x, y-1, commonData->fgcolor);
            lastX = x;
            lastY = y;
//        LOG_DEBUG(GwLog::LOG, "PageWindPlot: loop-Counter: %d, X: %d, Y: %d", count, x, y);
            if (count = 200) {
                count -= 40;    // move plotting area down by 40 pixels
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
