// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

/*
  Tracker
    - standalone with SD card backend
    - standalone with server backend
    - Regatta Hero integration

  In start phase big timer.
  Eventually after start other display because timer not needed any 
  more. Race timer smaller on top right corner. Reserved space for
  message area. 

    4 Positions for flags
    +-----+-----+
    |  1  |  2  |
    +-----+-----+
    |  3  |  4  |
    +-----+-----+


*/

#include "Pagedata.h"
#include "OBP60Extensions.h"

// Flags
#include "images/alpha.xbm"
#include "images/answer.xbm"
#include "images/black.xbm"
#include "images/blue.xbm"
#include "images/charlie.xbm"
#include "images/class.xbm"
#include "images/finish.xbm"
#include "images/hotel.xbm"
#include "images/india.xbm"
#include "images/november.xbm"
#include "images/orange.xbm"
#include "images/papa.xbm"
#include "images/repeat_one.xbm"
#include "images/sierra.xbm"
#include "images/start.xbm"
#include "images/uniform.xbm"
#include "images/xray.xbm"
#include "images/yankee.xbm"
#include "images/zulu.xbm"

class PageTracker : public Page
{
private:
    char mode = 'N'; // (N)ormal, (C)onfig
    bool simulation = false;
    String flashLED;

    String trackerType;
    String trackerOrganisation;
    String trackerTeam;
    String sailClub;
    String boatName;
    String boatClass;
    String boatSailNumber;
    float boatHandicap;

    void displayModeNormal(PageData &pageData) {

        // TBD Boatvalues: ...

        // Title
/*        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 48);
        getdisplay().print("Tracker"); */

        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(8, 42);
        if (trackerType == "NONE") {
            getdisplay().print("Disabled!");
        } else {
            getdisplay().print(trackerType);
        }

        // Timer
        getdisplay().setCursor(16, 120);
        getdisplay().setFont(&DSEG7Classic_BoldItalic42pt7b);
        getdisplay().print("-00:00");

        getdisplay().drawXBitmap(4, 140, class_bits, class_width, class_height, commonData->fgcolor);
        getdisplay().drawXBitmap(102, 140, start_bits, start_width, start_height, commonData->fgcolor);
        getdisplay().drawXBitmap(202, 140, finish_bits, finish_width, finish_height, commonData->fgcolor);

        getdisplay().drawXBitmap(4, 210, alpha_bits, alpha_width, alpha_height, commonData->fgcolor);
        getdisplay().drawXBitmap(102, 210, answer_bits, answer_width, answer_height, commonData->fgcolor);
        getdisplay().drawXBitmap(202, 210, papa_bits, papa_width, papa_height, commonData->fgcolor);
        getdisplay().drawXBitmap(302, 210, yankee_bits, yankee_width, yankee_height, commonData->fgcolor);

    }

    void displayModeConfig() {

        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 42);
        getdisplay().print("Tracker configuration");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        // TODO

        // Boat data
        uint16_t y = 80;
        uint16_t x = 16;
        uint16_t x1 = 100;
        getdisplay().setFont(&Ubuntu_Bold10pt8b);
        getdisplay().setCursor(8, 64);
        getdisplay().print("Boat data");
        
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(x, y);
        getdisplay().print("Boat name");
        getdisplay().setCursor(x1, y);
        getdisplay().print(boatName);

        getdisplay().setCursor(x, y+20);
        getdisplay().print("Boat class");
        getdisplay().setCursor(x1, y+20);
        getdisplay().print(boatClass);

        getdisplay().setCursor(x, y+40);
        getdisplay().print("Handicap");
        getdisplay().setCursor(x1, y+40);
        getdisplay().print(boatHandicap, 1);

        getdisplay().setCursor(x, y+60);
        getdisplay().print("Sail club");
        getdisplay().setCursor(x1, y+60);
        getdisplay().print(sailClub);

        // Tracker data
        y = 80;
        x = 208;
        getdisplay().setFont(&Ubuntu_Bold10pt8b);
        getdisplay().setCursor(x, 64);
        getdisplay().print("Tracker info");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(x, y);
        if (trackerType == "NONE") {
            getdisplay().print("Disabled");
        } else {
            getdisplay().printf("Type: %s", trackerType);
        }
        getdisplay().setCursor(x, y + 20);
        getdisplay().print("Team");
        getdisplay().setCursor(x, y + 40);
        getdisplay().print("Tracker organisation");

        // Ragatta selection
        y = 200;
        getdisplay().setFont(&Ubuntu_Bold10pt8b);
        getdisplay().setCursor(x, y);
        getdisplay().print("Regattas");

        // A) Regatta Hero:
        // Server Hostname, IP, Port
        // Organisation
        // Boat name
        //

        // B) SD-Card: check if card available
        // log interval: via config in seconds. min=?

        // C) Server: e.g. Raspi in Boat WLAN, own Internet-Server
        // display connection state to server here

    }

public:
    PageTracker(CommonData &common)
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG, "Instantiate PageTracker");
        flashLED = common.config->getString(common.config->flashLED);
        simulation = common.config->getBool(common.config->useSimuData);

        trackerType = common.config->getString(common.config->trackerType);
        trackerOrganisation = common.config->getString(common.config->trackerOrg);
        trackerTeam = common.config->getString(common.config->trackerTeam);

        boatName = common.config->getString(common.config->boatName);
        boatClass = common.config->getString(common.config->boatClass);
        boatSailNumber = common.config->getString(common.config->boatSailnumber);
        sailClub = common.config->getString(common.config->sailClub);
        boatHandicap = common.config->getString(common.config->boatHandicap).toFloat();

    }

    void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
    }

    int handleKey(int key){
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
            } else {
                mode = 'N';
            }
            return 0;
        }
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }

    void displayNew(PageData &pageData) {
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
    };

    int displayPage(PageData &pageData){
        GwLog *logger = commonData->logger;

        // Logging boat values
        logger->logDebug(GwLog::LOG, "Drawing at PageTracker; Mode=%c", mode);

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0,getdisplay().width(), getdisplay().height());

        if (mode == 'N') {
            displayModeNormal(pageData);
        } else if (mode == 'C') {
            displayModeConfig();
        }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageTracker(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageTracker(
    "Tracker",  // Page name
    createPage, // Action
    0,          // Number of bus values depends on selection in Web configuration
    {"LAT", "LON", "SOG"}, // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true        // Show display header on/off
);

#endif
