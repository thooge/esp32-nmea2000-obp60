#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "BoatDataCalibration.h"
#include "OBPcharts.h"

class PageOneValue : public Page {
private:
    GwLog* logger;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    char pageMode = 'V'; // Page mode: 'V' for value, 'C' for chart, 'B' for both
    int dataIntv = 1; // Update interval for wind history chart:
                      // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart

    String lengthformat;
    bool useSimuData;
    bool holdValues;
    String flashLED;
    String backlightMode;

    // Old values for hold function
    String sValue1Old = "";
    String unit1Old = "";

    // Data buffer pointer (owned by HstryBuffers)
    RingBuffer<uint16_t>* dataHstryBuf = nullptr;
    std::unique_ptr<Chart<uint16_t>> dataFlChart, dataHfChart; // Chart object, full and half size
    // Active chart and value
    Chart<uint16_t>* dataChart = nullptr;

    void showData(GwApi::BoatValue* bValue1, char size)
    {
        int nameXoff, nameYoff, unitXoff, unitYoff, value1Xoff, value1Yoff;
        const GFXfont *nameFnt, *unitFnt, *valueFnt1, *valueFnt2, *valueFnt3;

        if (size == 'F') { // full size data display
            nameXoff = 0;
            nameYoff = 0;
            nameFnt = &Ubuntu_Bold32pt8b;
            unitXoff = 0;
            unitYoff = 0;
            unitFnt = &Ubuntu_Bold20pt8b;
            value1Xoff = 0;
            value1Yoff = 0;
            valueFnt1 = &Ubuntu_Bold20pt8b;
            valueFnt2 = &Ubuntu_Bold32pt8b;
            valueFnt3 = &DSEG7Classic_BoldItalic60pt7b;
        } else { // half size data and chart display
            nameXoff = 105;
            nameYoff = -40;
            nameFnt = &Ubuntu_Bold20pt8b;
            unitXoff = -33;
            unitYoff = -40;
            unitFnt = &Ubuntu_Bold12pt8b;
            valueFnt1 = &Ubuntu_Bold12pt8b;
            value1Xoff = 105;
            value1Yoff = -105;
            valueFnt2 = &Ubuntu_Bold20pt8b;
            valueFnt3 = &DSEG7Classic_BoldItalic30pt7b;
        }

        String name1 = xdrDelete(bValue1->getName()); // Value name
        name1 = name1.substring(0, 6); // String length limit for value name
        calibrationData.calibrateInstance(bValue1, logger); // Check if boat data value is to be calibrated
        double value1 = bValue1->value; // Value as double in SI unit
        bool valid1 = bValue1->valid; // Valid information
        String sValue1 = formatValue(bValue1, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bValue1, *commonData).unit; // Unit of value 

        // Show name
        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(nameFnt);
        getdisplay().setCursor(20 + nameXoff, 100 + nameYoff);
        getdisplay().print(name1); // name

        // Show unit
        getdisplay().setFont(unitFnt);
        getdisplay().setCursor(270 + unitXoff, 100 + unitYoff);
        if (holdValues == false) {
            //getdisplay().print(unit1); // Unit
            drawTextRalign(298 + unitXoff, 100 + unitYoff, unit1); // Unit

        } else {
            // getdisplay().print(unit1Old);
            drawTextRalign(298 + unitXoff, 100 + unitYoff, unit1Old);
        }

        // Switch font if format for any values
        if (bValue1->getFormat() == "formatLatitude" || bValue1->getFormat() == "formatLongitude") {
            getdisplay().setFont(valueFnt1);
            getdisplay().setCursor(20 + value1Xoff, 180 + value1Yoff);
        } else if (bValue1->getFormat() == "formatTime" || bValue1->getFormat() == "formatDate") {
            getdisplay().setFont(valueFnt2);
            getdisplay().setCursor(20 + value1Xoff, 200 + value1Yoff);
        } else {
            getdisplay().setFont(valueFnt3);
            getdisplay().setCursor(20 + value1Xoff, 240 + value1Yoff);
        }

        // Show bus data
        if (holdValues == false) {
            getdisplay().print(sValue1); // Real value as formated string
        } else {
            getdisplay().print(sValue1Old); // Old value as formated string
        }
        if (valid1 == true) {
            sValue1Old = sValue1; // Save the old value
            unit1Old = unit1; // Save the old unit
        }
    }

public:
    PageOneValue(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageOneValue");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height

        // Get config data
        lengthformat = common.config->getString(common.config->lengthFormat);
        useSimuData = common.config->getBool(common.config->useSimuData);
        holdValues = common.config->getBool(common.config->holdvalues);
        flashLED = common.config->getString(common.config->flashLED);
        backlightMode = common.config->getString(common.config->backlight);
    }

    virtual void setupKeys()
    {
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
#if defined BOARD_OBP60S3
        commonData->keydata[4].label = "ZOOM";
#elif defined BOARD_OBP40S3
        commonData->keydata[1].label = "ZOOM";
#endif
    }

    // Key functions
    virtual int handleKey(int key)
    {
        // Set page mode value | full chart | value/half chart
        if (key == 1) {
            if (pageMode == 'V') {
                pageMode = 'C';
            } else if (pageMode == 'C') {
                pageMode = 'B';
            } else {
                pageMode = 'V';
            }
            return 0; // Commit the key
        }

        // Set interval for history chart update time (interval)
#if defined BOARD_OBP60S3
        if (key == 5) {
#elif defined BOARD_OBP40S3
        if (key == 2) {
#endif
            if (dataIntv == 1) {
                dataIntv = 2;
            } else if (dataIntv == 2) {
                dataIntv = 3;
            } else if (dataIntv == 3) {
                dataIntv = 4;
            } else if (dataIntv == 4) {
                dataIntv = 8;
            } else {
                dataIntv = 1;
            }
            return 0; // Commit the key
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayNew(PageData& pageData)
    {
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        if (!dataFlChart) { // Create chart objects if they don't exist
            GwApi::BoatValue* bValue1 = pageData.values[0]; // Page boat data element
            String bValName1 = bValue1->getName(); // Value name
            String bValFormat = bValue1->getFormat(); // Value format

            dataHstryBuf = pageData.hstryBuffers->getBuffer(bValName1);

            dataFlChart.reset(new Chart<uint16_t>(*dataHstryBuf, 'H', 0, Chart<uint16_t>::dfltChartRng[bValFormat], *commonData, useSimuData));
            dataHfChart.reset(new Chart<uint16_t>(*dataHstryBuf, 'H', 2, Chart<uint16_t>::dfltChartRng[bValFormat], *commonData, useSimuData));
            LOG_DEBUG(GwLog::DEBUG, "PageOneValue: Created chart objects for %s", bValName1);
        }
    }

    int displayPage(PageData& pageData)
    {

        LOG_DEBUG(GwLog::LOG, "Display PageOneValue");
        GwConfigHandler* config = commonData->config;
        GwLog* logger = commonData->logger;

        // Get boat value for page
        GwApi::BoatValue* bValue1 = pageData.values[0]; // Page boat data element

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Logging boat values
        if (bValue1 == NULL)
            return PAGE_OK; // WTF why this statement?
        LOG_DEBUG(GwLog::LOG, "Drawing at PageOneValue, %s: %f", bValue1->getName().c_str(), bValue1->value);

        // Draw page
        //***********************************************************

        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update

        if (pageMode == 'V') { // show only data value
            showData(bValue1, 'F');

        } else if (pageMode == 'C') { // show only data chart
            dataFlChart->showChrt(dataIntv, *bValue1, true);

        } else if (pageMode == 'B') { // show data value and chart
            showData(bValue1, 'H');
            dataHfChart->showChrt(dataIntv, *bValue1, false);
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageOneValue(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageOneValue(
    "OneValue", // Page name
    createPage, // Action
    1, // Number of bus values depends on selection in Web configuration
    true // Show display header on/off
);

#endif
