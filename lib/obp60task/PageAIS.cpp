// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

/*
  AIS Overview
    - circle with certain range, e.g. 5nm
    - AIS-Targets in range with speed and heading
    - perhaps collision alarm
  Data: LAT LON SOG HDT

  Feature possibilities
    - switch between North up / Heading up

*/

class PageAIS : public Page
{
private:
    bool simulation = false;
    bool holdvalues = false;
    String flashLED;
    String backlightMode;

    int scale = 5; // Radius of display circle in nautical miles

    bool alarm = false;
    bool alarm_enabled = false;
    int alarm_range = 3;

    char mode = 'N'; // (N)ormal, (C)onfig

    void displayModeNormal(PageData &pageData) {

        // TBD Boatvalues: ...

        LOG_DEBUG(GwLog::DEBUG,"Drawing at PageAIS");

        Point c = {200, 150}; // center = current boat position
        uint16_t r = 125;

        const std::vector<Point> pts_boat = { // polygon lines
            {c.x - 5, c.y},
            {c.x - 5, c.y - 10},
            {c.x, c.y - 16},
            {c.x + 5, c.y - 10},
            {c.x + 5, c.y}
        };
        drawPoly(pts_boat, commonData->fgcolor);

        // Title and corner value headings
        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("AIS");

        // zoom scale
        epd->drawLine(c.x + 10, c.y, c.x + r - 4, c.y, commonData->fgcolor);
        // arrow left
        epd->drawLine(c.x + 10, c.y, c.x + 16, c.y - 4, commonData->fgcolor);
        epd->drawLine(c.x + 10, c.y, c.x + 16, c.y + 4, commonData->fgcolor);
        // arrow right
        epd->drawLine(c.x + r - 4, c.y, c.x + r - 10, c.y - 4, commonData->fgcolor);
        epd->drawLine(c.x + r - 4, c.y, c.x + r - 10, c.y + 4, commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold8pt8b);
        drawTextCenter(c.x + r / 2, c.y + 8, String(scale) + "nm");
 
    }

    void displayModeConfig() {

        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("AIS configuration");

        epd->setFont(&Ubuntu_Bold8pt8b);
        // TODO menu

    }

public:
    PageAIS(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG,"Instantiate PageAIS");

        // preload configuration data
        simulation = config->getBool(config->useSimuData);
        holdvalues = config->getBool(config->holdvalues);
        flashLED = config->getString(config->flashLED);
        backlightMode = config->getString(config->backlight);

        alarm_range = 3;

     }

    void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
        commonData->keydata[1].label = "ALARM";
    }

#ifdef BOARD_OBP60S3
    int handleKey(int key){
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

    void displayNew(PageData &pageData){
    };

    int displayPage(PageData &pageData){

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageAIS; Mode=%c", mode);

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
    return new PageAIS(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageAIS(
    "AIS",      // Page name
    createPage, // Action
    0,          // Number of bus values depends on selection in Web configuration
    {"LAT", "LON", "SOG", "HDT"}, // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true        // Show display header on/off
);

#endif
