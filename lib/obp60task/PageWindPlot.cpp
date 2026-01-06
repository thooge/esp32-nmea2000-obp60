#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPcharts.h"

// ****************************************************************
class PageWindPlot : public Page {

private:
    GwLog* logger;

    static constexpr char SHOW_WIND_DIR = 'D';
    static constexpr char SHOW_WIND_SPEED = 'S';
    static constexpr char SHOW_BOTH = 'B';
    static constexpr char HORIZONTAL = 'H';
    static constexpr char VERTICAL = 'V';
    static constexpr int8_t FULL_SIZE = 0;
    static constexpr int8_t HALF_SIZE_TOP = 1;
    static constexpr int8_t HALF_SIZE_BOTTOM = 2;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both
    bool showTruW = true; // Show true wind or apparent wind in chart area
    bool oldShowTruW = false; // remember recent user selection of wind data type

    int8_t dataIntv = 1; // Update interval for wind history chart:
                         // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart
    bool useSimuData;
    // bool holdValues;
    String flashLED;
    String backlightMode;

#ifdef BOARD_OBP40S3
    String wndSrc; // Wind source true/apparent wind - preselection for OBP40
#endif

    // Data buffers pointers (owned by HstryBuffers)
    RingBuffer<uint16_t>* twdHstry = nullptr;
    RingBuffer<uint16_t>* twsHstry = nullptr;
    RingBuffer<uint16_t>* awdHstry = nullptr;
    RingBuffer<uint16_t>* awsHstry = nullptr;

    // Chart objects
    std::unique_ptr<Chart> twdChart, awdChart; // Chart object for wind direction, full size
    std::unique_ptr<Chart> twsChart, awsChart; // Chart object for wind speed, full size

    // Active charts and values
    Chart* wdChart = nullptr;
    Chart* wsChart = nullptr;
    GwApi::BoatValue* wdBVal = nullptr;
    GwApi::BoatValue* wsBVal = nullptr;

public:
    PageWindPlot(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageWindPlot");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height

        // Get config data
        useSimuData = common.config->getBool(common.config->useSimuData);
        // holdValues = common.config->getBool(common.config->holdvalues);
        flashLED = common.config->getString(common.config->flashLED);
        backlightMode = common.config->getString(common.config->backlight);

        oldShowTruW = !showTruW; // makes wind source being initialized at initial page call
    }

    virtual void setupKeys()
    {
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
#if defined BOARD_OBP60S3
        commonData->keydata[1].label = "SRC";
        commonData->keydata[4].label = "ZOOM";
#elif defined BOARD_OBP40S3
        commonData->keydata[1].label = "ZOOM";
#endif
    }

    // Key functions
    virtual int handleKey(int key)
    {
        // Set chart mode TWD | TWS
        if (key == 1) {
            if (chrtMode == SHOW_WIND_DIR) {
                chrtMode = SHOW_WIND_SPEED;
            } else if (chrtMode == SHOW_WIND_SPEED) {
                chrtMode = SHOW_BOTH;
            } else {
                chrtMode = SHOW_WIND_DIR;
            }
            return 0; // Commit the key
        }

#if defined BOARD_OBP60S3
        // Set data source TRUE | APP
        if (key == 2) {
            showTruW = !showTruW;
            return 0; // Commit the key
        }

        // Set interval for wind history chart update time (interval)
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
#ifdef BOARD_OBP40S3
        // we can only initialize user defined wind source here, because "pageData" is not available at object instantiation
        wndSrc = commonData->config->getString("page" + String(pageData.pageNumber) + "wndsrc");
        if (wndSrc == "True wind") {
            showTruW = true;
        } else {
            showTruW = false; // Wind source is apparent wind
        }
        oldShowTruW = !showTruW; // Force chart update in displayPage
#endif

        // With chart object initialization being performed here, PageWindPlot won't properly work as default page,
        // because <displayNew> is not executed at system start for default page
        if (!twdChart) { // Create true wind charts if they don't exist
            twdHstry = pageData.hstryBuffers->getBuffer("TWD");
            twsHstry = pageData.hstryBuffers->getBuffer("TWS");

            if (twdHstry) {
                twdChart.reset(new Chart(*twdHstry, Chart::dfltChrtDta["formatCourse"].range, *commonData, useSimuData));
            }
            if (twsHstry) {
                twsChart.reset(new Chart(*twsHstry, Chart::dfltChrtDta["formatKnots"].range, *commonData, useSimuData));
            }
        }

        if (!awdChart) { // Create apparent wind charts if they don't exist
            awdHstry = pageData.hstryBuffers->getBuffer("AWD");
            awsHstry = pageData.hstryBuffers->getBuffer("AWS");

            if (awdHstry) {
                awdChart.reset(new Chart(*awdHstry, Chart::dfltChrtDta["formatCourse"].range, *commonData, useSimuData));
            }
            if (awsHstry) {
                awsChart.reset(new Chart(*awsHstry, Chart::dfltChrtDta["formatKnots"].range, *commonData, useSimuData));
            }
            if (twdHstry && twsHstry && awdHstry && awsHstry) {
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Created wind charts");
            } else {
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Some/all chart objects for wind data missing");
            }
        }
    }

    int displayPage(PageData& pageData)
    {
        LOG_DEBUG(GwLog::LOG, "Display PageWindPlot");
        ulong pageTime = millis();

        /*        if (!twdChart) { // Create true wind charts if they don't exist
                    twdHstry = pageData.hstryBuffers->getBuffer("TWD");
                    twsHstry = pageData.hstryBuffers->getBuffer("TWS");

                    if (twdHstry) {
                        twdChart.reset(new Chart(*twdHstry, Chart::dfltChrtDta["formatCourse"].range, *commonData, useSimuData));
                    }
                    if (twsHstry) {
                        twsChart.reset(new Chart(*twsHstry, Chart::dfltChrtDta["formatKnots"].range, *commonData, useSimuData));
                    }
                }

                if (!awdChart) { // Create apparent wind charts if they don't exist
                    awdHstry = pageData.hstryBuffers->getBuffer("AWD");
                    awsHstry = pageData.hstryBuffers->getBuffer("AWS");

                    if (awdHstry) {
                        awdChart.reset(new Chart(*awdHstry, Chart::dfltChrtDta["formatCourse"].range, *commonData, useSimuData));
                    }
                    if (awsHstry) {
                        awsChart.reset(new Chart(*awsHstry, Chart::dfltChrtDta["formatKnots"].range, *commonData, useSimuData));
                    }
                    if (twdHstry && twsHstry && awdHstry && awsHstry) {
                        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Created wind charts");
                    } else {
                        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Some/all chart objects for wind data missing");
                    }
                } */

        if (showTruW != oldShowTruW) {

            // Switch active charts based on showTruW
            if (showTruW) {
                wdChart = twdChart.get();
                wsChart = twsChart.get();
                wdBVal = pageData.values[0];
                wsBVal = pageData.values[1];
            } else {
                wdChart = awdChart.get();
                wsChart = awsChart.get();
                wdBVal = pageData.values[2];
                wsBVal = pageData.values[3];
            }

            oldShowTruW = showTruW;
        }
        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: draw with data %s: %.2f, %s: %.2f", wdBVal->getName().c_str(), wdBVal->value, wsBVal->getName().c_str(), wsBVal->value);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        if (chrtMode == SHOW_WIND_DIR) {
            if (wdChart) {
                wdChart->showChrt(VERTICAL, FULL_SIZE, dataIntv, true, *wdBVal);
            }

        } else if (chrtMode == SHOW_WIND_SPEED) {
            if (wsChart) {
                wsChart->showChrt(HORIZONTAL, FULL_SIZE, dataIntv, true, *wsBVal);
            }

        } else if (chrtMode == SHOW_BOTH) {
            if (wdChart) {
                wdChart->showChrt(VERTICAL, HALF_SIZE_TOP, dataIntv, true, *wdBVal);
            }
            if (wsChart) {
                wsChart->showChrt(VERTICAL, HALF_SIZE_BOTTOM, dataIntv, true, *wsBVal);
            }
        }

        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: page time %ldms", millis() - pageTime);
        return PAGE_UPDATE;
    }
};

static Page* createPage(CommonData& common)
{
    return new PageWindPlot(common);
}

/* with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need */
PageDescription registerPageWindPlot(
    "WindPlot", // Page name
    createPage, // Action
    0, // Number of bus values depends on selection in Web configuration
    { "TWD", "TWS", "AWD", "AWS" }, // Bus values we need in the page
    true // Show display header on/off
);

#endif