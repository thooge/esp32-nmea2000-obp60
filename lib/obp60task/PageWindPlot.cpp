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
    bool showTruW = true; // Show true wind or apparent wind in chart area
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
        String wndSrc; // Wind source true/apparent wind - preselection for OBP40

        wndSrc = commonData->config->getString("page" + String(pageData.pageNumber) + "wndsrc");
        if (wndSrc == "True wind") {
            showTruW = true;
        } else {
            showTruW = false; // Wind source is apparent wind
        }
        LOG_DEBUG(GwLog::LOG, "New PageWindPlot; wind source=%s", wndSrc);
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

        // Separate chart objects for true wind and apparent wind
        static std::unique_ptr<Chart<uint16_t>> twdFlChart, awdFlChart; // chart object for wind direction chart, full size
        static std::unique_ptr<Chart<uint16_t>> twsFlChart, awsFlChart; // chart object for wind speed chart, full size
        static std::unique_ptr<Chart<uint16_t>> twdHfChart, awdHfChart; // chart object for wind direction chart, half size
        static std::unique_ptr<Chart<uint16_t>> twsHfChart, awsHfChart; // chart object for wind speed chart, half size
        // Pointers to the currently active charts
        static Chart<uint16_t>* wdFlChart;
        static Chart<uint16_t>* wsFlChart;
        static Chart<uint16_t>* wdHfChart;
        static Chart<uint16_t>* wsHfChart;

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
            if (!twdFlChart) { // Create true wind charts if they don't exist

                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Creating true wind charts");
                auto* twdHstry = pageData.boatHstry->hstryBufList.twdHstry;
                auto* twsHstry = pageData.boatHstry->hstryBufList.twsHstry;
                // LOG_DEBUG(GwLog::DEBUG,"History Buffer addresses PageWindPlot: twdBuf: %p, twsBuf: %p", (void*)pageData.boatHstry->hstryBufList.twdHstry,
                //     (void*)pageData.boatHstry->hstryBufList.twsHstry);

                twdFlChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*twdHstry, 1, 0, dfltRngWd, *commonData, useSimuData));
                twsFlChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*twsHstry, 0, 0, dfltRngWs, *commonData, useSimuData));
                twdHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*twdHstry, 1, 1, dfltRngWd, *commonData, useSimuData));
                twsHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*twsHstry, 1, 2, dfltRngWs, *commonData, useSimuData));
                // twdHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*twdHstry, 0, 1, dfltRngWd, *commonData, useSimuData));
                // twsHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*twsHstry, 0, 2, dfltRngWs, *commonData, useSimuData));
                // LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: twdHstry: %p, twsHstry: %p", (void*)twdHstry, (void*)twsHstry);
            }

            if (!awdFlChart) { // Create apparent wind charts if they don't exist
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: Creating apparent wind charts");
                auto* awdHstry = pageData.boatHstry->hstryBufList.awdHstry;
                auto* awsHstry = pageData.boatHstry->hstryBufList.awsHstry;

                awdFlChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*awdHstry, 1, 0, dfltRngWd, *commonData, useSimuData));
                awsFlChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*awsHstry, 0, 0, dfltRngWs, *commonData, useSimuData));
                awdHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*awdHstry, 1, 1, dfltRngWd, *commonData, useSimuData));
                awsHfChart = std::unique_ptr<Chart<uint16_t>>(new Chart<uint16_t>(*awsHstry, 1, 2, dfltRngWs, *commonData, useSimuData));
            }
            
            // Switch active charts based on showTruW
            if (showTruW) {
                wdHstry = pageData.boatHstry->hstryBufList.twdHstry;
                wsHstry = pageData.boatHstry->hstryBufList.twsHstry;
                wdFlChart = twdFlChart.get();
                wsFlChart = twsFlChart.get();
                wdHfChart = twdHfChart.get();
                wsHfChart = twsHfChart.get();
            } else {
                wdHstry = pageData.boatHstry->hstryBufList.awdHstry;
                wsHstry = pageData.boatHstry->hstryBufList.awsHstry;
                wdFlChart = awdFlChart.get();
                wsFlChart = awsFlChart.get();
                wdHfChart = awdHfChart.get();
                wsHfChart = awsHfChart.get();
            }
            
            wdHstry->getMetaData(wdName, wdFormat);
            wsHstry->getMetaData(wsName, wsFormat);
            
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
            wdFlChart->showChrt(dataIntv, *bvalue[0]);

        } else if (chrtMode == 'S') {
            wsBVal->value = wsHstry->getLast();
            wsBVal->valid = wsBVal->value != wsHstry->getMaxVal();
            wsFlChart->showChrt(dataIntv, *bvalue[1]);

        } else if (chrtMode == 'B') {
            wdBVal->value = wdHstry->getLast();
            wdBVal->valid = wdBVal->value != wdHstry->getMaxVal();
            wsBVal->value = wsHstry->getLast();
            wsBVal->valid = wsBVal->value != wsHstry->getMaxVal();
            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot showChrt: wsBVal.name: %s, format: %s, wsBVal.value: %.1f, valid: %d, address: %p", wsBVal->getName(), wsBVal->getFormat(), wsBVal->value,
                wsBVal->valid, wsBVal);
            wdHfChart->showChrt(dataIntv, *bvalue[0]);
            wsHfChart->showChrt(dataIntv, *bvalue[1]);
        }

        LOG_DEBUG(GwLog::LOG, "PageWindPlot: page time %ldms", millis() - pageTime);
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