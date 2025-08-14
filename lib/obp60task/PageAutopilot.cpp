// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

/*
  Autopilot

*/

class PageAutopilot : public Page
{
private:
    char mode = 'N'; // (N)ormal, (C)onfig

    void displayModeNormal(PageData &pageData) {

        // TBD Boatvalues: ...

        logger->logDebug(GwLog::DEBUG, "Drawing at PageAutopilot");

        // Title and corner value headings
        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("Autopilot");

    }

    void displayModeConfig() {

        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("Autopilot configuration");

        epd->setFont(&Ubuntu_Bold8pt8b);
        // TODO menu

    }

public:
    PageAutopilot(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageAutopilot");
    }

    void setupKeys() {
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
    }

#ifdef BOARD_OBP60S3
    int handleKey(int key) {
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
            } else {
                mode = 'N';
            }
            return 0;
        }
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }
#endif
#ifdef BOARD_OBP40S3
    int handleKey(int key){
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
                commonData->keydata[1].label = "EDIT";
            } else {
                mode = 'N';
                commonData->keydata[1].label = "ALARM";
            }
            return 0;
        }
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }
#endif

    void displayNew(PageData &pageData){
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
    };

    int displayPage(PageData &pageData){

        // Logging boat values
        logger->logDebug(GwLog::LOG, "Drawing at PageAutopilot; Mode=%c", mode);

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height());

       if (mode == 'N') {
           displayModeNormal(pageData);
       } else if (mode == 'C') {
           displayModeConfig();
       }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageAutopilot(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageAutopilot(
    "Autopilot",    // Page name
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {},             // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    false           // Show display header on/off
);

#endif
