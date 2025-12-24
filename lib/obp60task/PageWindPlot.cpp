#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPcharts.h"

// ****************************************************************
class PageWindPlot : public Page {

private:
    GwLog* logger;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both
    bool showTruW = true; // Show true wind or apparent wind in chart area
    bool oldShowTruW = false; // remember recent user selection of wind data type

    int dataIntv = 1; // Update interval for wind history chart:
                      // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart
    bool useSimuData;
    //bool holdValues;
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
    std::unique_ptr<Chart<uint16_t>> twdFlChart, awdFlChart; // Chart object for wind direction, full size
    std::unique_ptr<Chart<uint16_t>> twsFlChart, awsFlChart; // Chart object for wind speed, full size
    std::unique_ptr<Chart<uint16_t>> twdHfChart, awdHfChart; // Chart object for wind direction, half size
    std::unique_ptr<Chart<uint16_t>> twsHfChart, awsHfChart; // Chart object for wind speed, half size

    // Active charts and values
    Chart<uint16_t>* wdFlChart = nullptr;
    Chart<uint16_t>* wsFlChart = nullptr;
    Chart<uint16_t>* wdHfChart = nullptr;
    Chart<uint16_t>* wsHfChart = nullptr;
    GwApi::BoatValue* wdBVal = nullptr;
    GwApi::BoatValue* wsBVal = nullptr;

    const double dfltRngWd = 60.0 * DEG_TO_RAD; // default range for course chart from min to max value in RAD
    const double dfltRngWs = 7.5; // default range for wind speed chart from min to max value in m/s

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
        //holdValues = common.config->getBool(common.config->holdvalues);
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
            if (chrtMode == 'D') {
                chrtMode = 'S';
            } else if (chrtMode == 'S') {
                chrtMode = 'B';
            } else {
                chrtMode = 'D';
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
        wndSrc = commonData->config->getString("page" + String(pageData.pageNumber) + "wndsrc");
        if (wndSrc == "True wind") {
            showTruW = true;
        } else {
            showTruW = false; // Wind source is apparent wind
        }
        oldShowTruW = !showTruW; // Force chart update in displayPage
#endif
        // buffer initialization cannot be performed here, because <displayNew> is not executed at system start for default page

        /* if (!twdFlChart) { // Create true wind charts if they don't exist
            twdHstry = pageData.hstryBuffers->getBuffer("TWD");
            twsHstry = pageData.hstryBuffers->getBuffer("TWS");

            if (twdHstry) {
                twdFlChart.reset(new Chart<uint16_t>(*twdHstry, 'V', 0, dfltRngWd, *commonData, useSimuData));
                twdHfChart.reset(new Chart<uint16_t>(*twdHstry, 'V', 1, dfltRngWd, *commonData, useSimuData));
            }
            if (twsHstry) {
                twsFlChart.reset(new Chart<uint16_t>(*twsHstry, 'H', 0, dfltRngWs, *commonData, useSimuData));
                twsHfChart.reset(new Chart<uint16_t>(*twsHstry, 'V', 2, dfltRngWs, *commonData, useSimuData));
            }
        }

        if (!awdFlChart) { // Create apparent wind charts if they don't exist
            awdHstry = pageData.hstryBuffers->getBuffer("AWD");
            awsHstry = pageData.hstryBuffers->getBuffer("AWS");

            if (awdHstry) {
                awdFlChart.reset(new Chart<uint16_t>(*awdHstry, 'V', 0, dfltRngWd, *commonData, useSimuData));
                awdHfChart.reset(new Chart<uint16_t>(*awdHstry, 'V', 1, dfltRngWd, *commonData, useSimuData));
            }
            if (awsHstry) {
                awsFlChart.reset(new Chart<uint16_t>(*awsHstry, 'H', 0, dfltRngWs, *commonData, useSimuData));
                awsHfChart.reset(new Chart<uint16_t>(*awsHstry, 'V', 2, dfltRngWs, *commonData, useSimuData));
            }
            if (twdHstry && twsHstry && awdHstry && awsHstry) {
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Created wind charts");
            } else {
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Some/all chart objects for wind data missing");
            }
        } */
    }

    int displayPage(PageData& pageData)
    {
        LOG_DEBUG(GwLog::LOG, "Display PageWindPlot");
        ulong pageTime = millis();

        if (!twdFlChart) { // Create true wind charts if they don't exist
            twdHstry = pageData.hstryBuffers->getBuffer("TWD");
            twsHstry = pageData.hstryBuffers->getBuffer("TWS");

            if (twdHstry) {
                twdFlChart.reset(new Chart<uint16_t>(*twdHstry, 'V', 0, dfltRngWd, *commonData, useSimuData));
                twdHfChart.reset(new Chart<uint16_t>(*twdHstry, 'V', 1, dfltRngWd, *commonData, useSimuData));
            }
            if (twsHstry) {
                twsFlChart.reset(new Chart<uint16_t>(*twsHstry, 'H', 0, dfltRngWs, *commonData, useSimuData));
                twsHfChart.reset(new Chart<uint16_t>(*twsHstry, 'V', 2, dfltRngWs, *commonData, useSimuData));
            }
        }

        if (!awdFlChart) { // Create apparent wind charts if they don't exist
            awdHstry = pageData.hstryBuffers->getBuffer("AWD");
            awsHstry = pageData.hstryBuffers->getBuffer("AWS");

            if (awdHstry) {
                awdFlChart.reset(new Chart<uint16_t>(*awdHstry, 'V', 0, dfltRngWd, *commonData, useSimuData));
                awdHfChart.reset(new Chart<uint16_t>(*awdHstry, 'V', 1, dfltRngWd, *commonData, useSimuData));
            }
            if (awsHstry) {
                awsFlChart.reset(new Chart<uint16_t>(*awsHstry, 'H', 0, dfltRngWs, *commonData, useSimuData));
                awsHfChart.reset(new Chart<uint16_t>(*awsHstry, 'V', 2, dfltRngWs, *commonData, useSimuData));
            }
            if (twdHstry && twsHstry && awdHstry && awsHstry) {
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Created wind charts");
            } else {
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Some/all chart objects for wind data missing");
            }
        }

        if (showTruW != oldShowTruW) {

            // Switch active charts based on showTruW
            if (showTruW) {
                wdFlChart = twdFlChart.get();
                wsFlChart = twsFlChart.get();
                wdHfChart = twdHfChart.get();
                wsHfChart = twsHfChart.get();
                wdBVal = pageData.values[0];
                wsBVal = pageData.values[1];
            } else {
                wdFlChart = awdFlChart.get();
                wsFlChart = awsFlChart.get();
                wdHfChart = awdHfChart.get();
                wsHfChart = awsHfChart.get();
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

        if (chrtMode == 'D') {
            if (wdFlChart) {
                wdFlChart->showChrt(dataIntv, *wdBVal, true);
            }

        } else if (chrtMode == 'S') {
            if (wsFlChart) {
                wsFlChart->showChrt(dataIntv, *wsBVal, true);
            }

        } else if (chrtMode == 'B') {
            if (wdHfChart) {
                wdHfChart->showChrt(dataIntv, *wdBVal, true);
            }
            if (wsHfChart) {
                wsHfChart->showChrt(dataIntv, *wsBVal, true);
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