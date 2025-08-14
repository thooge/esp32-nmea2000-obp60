// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

#include "images/OBP_400x300.xbm"     // OBP Logo
#ifdef BOARD_OBP60S3
#include "images/OBP60_400x300.xbm"   // MFD with logo
#endif
#ifdef BOARD_OBP40S3
#include "images/OBP40_400x300.xbm"   // MFD with logo
#endif

class PageWhite : public Page
{
private:
    char mode = 'W';  // display mode (W)hite | (L)ogo | (M)FD logo

public:
    PageWhite(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageWhite");
        refreshtime = 15000;
    }

    int handleKey(int key) {
         // Change display mode
        if (key == 1) {
            if (mode == 'W') {
                mode = 'L';
            } else if (mode == 'L') {
                mode = 'M';
            } else {
                mode = 'W';
            }
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

    int displayPage(PageData &pageData) {

        // Logging boat values
        logger->logDebug(GwLog::LOG, "Drawing at PageWhite");

        // Draw page
        //***********************************************************

        // Set background color
        int bgcolor = GxEPD_WHITE;

        // Set display in partial refresh mode
        if (mode == 'W') {
            epd->setFullWindow();
        } else {
            epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
        }

        if (mode == 'L') {
            epd->drawXBitmap(0, 0, OBP_400x300_bits, OBP_400x300_width, OBP_400x300_height, commonData->fgcolor);
        } else if (mode == 'M') {
#ifdef BOARD_OBP60S3
            epd->drawXBitmap(0, 0, OBP60_400x300_bits, OBP60_400x300_width, OBP60_400x300_height, commonData->fgcolor);
#endif
#ifdef BOARD_OBP40S3
            epd->drawXBitmap(0, 0, OBP40_400x300_bits, OBP40_400x300_width, OBP40_400x300_height, commonData->fgcolor);
#endif
        }

        int ret = PAGE_UPDATE;
        if (mode == 'W') {
            ret |= PAGE_HIBERNATE;
        }
        return ret;
    };
};

static Page* createPage(CommonData &common){
    return new PageWhite(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageWhite(
    "WhitePage",    // Page name
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    false           // Show display header on/off
);

#endif
