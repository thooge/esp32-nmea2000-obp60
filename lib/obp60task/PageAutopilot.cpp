#ifdef BOARD_OBP60S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "images/unknown.xbm"

class PageAutopilot : public Page
{
bool keylock = false;               // Keylock
char mode = 'S';                    // display mode (S)tandby | (A)utopilot

public:
    PageAutopilot(CommonData &common){
        common.logger->logDebug(GwLog::LOG,"Instantiate PageAutopilot");
         if (hasFRAM) {
             mode = fram.read(FRAM_AUTOPILOT_MODE);
         }
    }

    virtual int handleKey(int key){
        // Switch display mode
        if (key == 1) {
            if (mode == 'S') {
                mode = 'A';
            } else {
                mode = 'S';
            }
            if (hasFRAM) fram.write(FRAM_AUTOPILOT_MODE, mode);
            return 0;
        }
        // Code for keylock
        if(key == 11){
            keylock = !keylock;         // Toggle keylock
            return 0;                   // Commit the key
        }
        return key;
    }

    virtual void displayPage(CommonData &commonData, PageData &pageData){
        GwConfigHandler *config = commonData.config;
        GwLog *logger = commonData.logger;

        // Get config data
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageAutopilot");

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        getdisplay().setTextColor(commonData.fgcolor);
        getdisplay().setFont(&Ubuntu_Bold20pt7b);
        getdisplay().setCursor(20, 55);
        getdisplay().print("Autopilot");
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(20, 75);
        getdisplay().print("This is just a template.");
        getdisplay().setCursor(20, 90);
        getdisplay().print("No function implemented actually.");

        // temporary unknown hint
        getdisplay().drawXBitmap(200-unknown_width/2, 170-unknown_height/2, unknown_bits, unknown_width, unknown_height, commonData.fgcolor);

        getdisplay().setCursor(140, 250);
        getdisplay().print("Here be dragons");

        // Key Layout
        getdisplay().setTextColor(commonData.fgcolor);
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        if(keylock == false){
            getdisplay().setCursor(10, 290);
            getdisplay().print("[MODE]");
            getdisplay().setCursor(130, 290);
            getdisplay().print("[  <<<<  " + String(commonData.data.actpage) + "/" + String(commonData.data.maxpage) + "  >>>>  ]");
            if(String(backlightMode) == "Control by Key"){
                getdisplay().setCursor(343, 290);
                getdisplay().print("[ILUM]");
            }
        }
        else{
            getdisplay().setCursor(130, 290);
            getdisplay().print(" [    Keylock active    ]");
        }

        // Update display
        getdisplay().nextPage();    // Partial update (fast)

    };
};

static Page* createPage(CommonData &common){
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
    true           // Show display header on/off
);

#endif
