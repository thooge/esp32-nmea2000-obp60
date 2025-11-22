#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPcharts.h"

// ****************************************************************
class PageWindPlot : public Page {

private:
    GwLog* logger;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both
    bool showTruW = true; // Show true wind or apparant wind in chart area
    bool oldShowTruW = false; // remember recent user selection of wind data type

    int dataIntv = 1; // Update interval for wind history chart:
                      // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart
    bool useSimuData;
    String flashLED;
    String backlightMode;

public:
    PageWindPlot(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageWindPlot");

        // Get config data
        useSimuData = common.config->getBool(common.config->useSimuData);
        // holdValues = common.config->getBool(common.config->holdvalues);
        flashLED = common.config->getString(common.config->flashLED);
        backlightMode = common.config->getString(common.config->backlight);
    }

    virtual void setupKeys()
    {
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
#if defined BOARD_OBP60S3
        commonData->keydata[1].label = "SRC";
        commonData->keydata[4].label = "INTV";
#elif defined BOARD_OBP40S3
        commonData->keydata[1].label = "INTV";
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
#ifdef BOARD_OBP40S3
        String wndSrc; // Wind source true/apparant wind - preselection for OBP40

        wndSrc = commonData->config->getString("page" + String(pageData.pageNumber) + "wndsrc");
        if (wndSrc == "True wind") {
            showTruW = true;
        } else {
            showTruW = false; // Wind source is apparant wind
        }
        LOG_DEBUG(GwLog::LOG, "New PageWindPlot; wind source=%s", wndSrc);
        // commonData->logger->logDebug(GwLog::LOG, "New PageWindPlot: wind source=%s", wndSrc);
#endif
        oldShowTruW = !showTruW; // makes wind source being initialized at initial page call

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height
    }

    int displayPage(PageData& pageData)
    {
        GwConfigHandler* config = commonData->config;

        static RingBuffer<uint16_t>* wdHstry; // Wind direction data buffer
        static RingBuffer<uint16_t>* wsHstry; // Wind speed data buffer
        static String wdName, wdFormat; // Wind direction name and format
        static String wsName, wsFormat; // Wind speed name and format
        static std::unique_ptr<Chart<uint16_t>> twdFlChart; // chart object for wind direction chart, full size
        static std::unique_ptr<Chart<uint16_t>> twsFlChart; // chart object for wind speed chart, full size
        static std::unique_ptr<Chart<uint16_t>> twdHfChart; // chart object for wind direction chart, half size
        static std::unique_ptr<Chart<uint16_t>> twsHfChart; // chart object for wind speed chart, half size
        static GwApi::BoatValue* wdBVal = new GwApi::BoatValue("TWD"); // temp BoatValue for wind direction unit identification; required by OBP60Formater
        static GwApi::BoatValue* wsBVal = new GwApi::BoatValue("TWS"); // temp BoatValue for wind speed unit identification; required by OBP60Formater */
        double dfltRngWd = 60.0 * DEG_TO_RAD; // default range for course chart from min to max value in RAD
        double dfltRngWs = 7.5; // default range for wind speed chart from min to max value in m/s

        const int numBoatData = 4;
        GwApi::BoatValue* bvalue[numBoatData]; // current boat data values

        LOG_DEBUG(GwLog::LOG, "Display PageWindPlot");
        ulong pageTime = millis();

        // read boat data values
        for (int i = 0; i < numBoatData; i++) {
            bvalue[i] = pageData.values[i];
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (showTruW != oldShowTruW) {
            if (showTruW) {
                wdHstry = pageData.boatHstry->hstryBufList.twdHstry;
                wsHstry = pageData.boatHstry->hstryBufList.twsHstry;
            } else {
                wdHstry = pageData.boatHstry->hstryBufList.awdHstry;
                wsHstry = pageData.boatHstry->hstryBufList.awsHstry;
            }
            wdHstry->getMetaData(wdName, wdFormat);
            wsHstry->getMetaData(wsName, wsFormat);

            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: *wdHstry: %p, *wsHstry: %p", wdHstry, wsHstry);
            twdFlChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*wdHstry, 1, 0, dfltRngWd, *commonData, useSimuData));
            twsFlChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*wsHstry, 0, 0, dfltRngWs, *commonData, useSimuData));
            twdHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*wdHstry, 1, 1, dfltRngWd, *commonData, useSimuData));
            twsHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*wsHstry, 1, 2, dfltRngWs, *commonData, useSimuData));

            oldShowTruW = showTruW;
        }

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        if (chrtMode == 'D') {
            wdBVal->value = wdHstry->getLast();
            wdBVal->valid = wdBVal->value != wdHstry->getMaxVal();
            twdFlChart->showChrt(dataIntv, *bvalue[0]);

        } else if (chrtMode == 'S') {
            wsBVal->value = wsHstry->getLast();
            wsBVal->valid = wsBVal->value != wsHstry->getMaxVal();
            twsFlChart->showChrt(dataIntv, *bvalue[1]);

        } else if (chrtMode == 'B') {
            wdBVal->value = wdHstry->getLast();
            wdBVal->valid = wdBVal->value != wdHstry->getMaxVal();
            wsBVal->value = wsHstry->getLast();
            wsBVal->valid = wsBVal->value != wsHstry->getMaxVal();
            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot showChrt: wsBVal.name: %s, format: %s, wsBVal.value: %.1f, valid: %d, address: %p", wsBVal->getName(), wsBVal->getFormat(), wsBVal->value,
                wsBVal->valid, wsBVal);
            twdHfChart->showChrt(dataIntv, *bvalue[0]);
            twsHfChart->showChrt(dataIntv, *bvalue[1]);
        }

        LOG_DEBUG(GwLog::LOG, "PageWindPlot: runtime: %ldms", millis() - pageTime);
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