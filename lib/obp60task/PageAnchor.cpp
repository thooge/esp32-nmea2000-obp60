#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

/*
  Anchor overview with additional associated data
  This page is in experimental stage so be warned!

    DBS - Water depth
    HDT - Boat heading
    TWS - Wind strength
    TWD - Wind direction

  This is the fist page to contain a configuration page with 
  data entry option.
  Also it will make use of the new alarm function.

*/

#define anchor_width 16
#define anchor_height 16
static unsigned char anchor_bits[] = {
   0x80, 0x01, 0x40, 0x02, 0x40, 0x02, 0x80, 0x01, 0xf0, 0x0f, 0x80, 0x01,
   0x80, 0x01, 0x88, 0x11, 0x8c, 0x31, 0x8e, 0x71, 0x84, 0x21, 0x86, 0x61,
   0x86, 0x61, 0xfc, 0x3f, 0xf8, 0x1f, 0x80, 0x01 };

class PageAnchor : public Page
{
    bool simulation = false;
    bool holdvalues = false;
    String flashLED;
    String backlightMode;

    public:
    PageAnchor(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageAnchor");

        // preload configuration data
        simulation = common.config->getBool(common.config->useSimuData);
        holdvalues = common.config->getBool(common.config->holdvalues);
        flashLED = common.config->getString(common.config->flashLED);
        backlightMode = common.config->getString(common.config->backlight);
    }

    virtual int handleKey(int key){
        // Code for keylock
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    void displayNew(PageData &pageData){
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageAnchor");

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        uint16_t cx = 200; // center = anchor position
        uint16_t cy = 150; 

        // draw anchor symbol (as bitmap)
        getdisplay().drawXBitmap(cx - anchor_width / 2, cy - anchor_height / 2,
                                 anchor_bits, anchor_width, anchor_height, commonData->fgcolor);


        getdisplay().setTextColor(commonData->fgcolor);

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageAnchor(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageAnchor(
    "Anchor",       // Page name
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {"DBS", "HDT", "TWS", "TWD"}, // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true            // Show display header on/off
);

#endif
