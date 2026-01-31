#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include <PCF8574.h>                  // PCF8574 modules from Horter
#include "Pagedata.h"
#include "OBP60Extensions.h"

#include "images/OBP_400x300.xbm"     // OBP Logo
#ifdef BOARD_OBP60S3
#include "images/OBP60_400x300.xbm"   // MFD with logo
#endif
#ifdef BOARD_OBP40S3
#include "images/OBP40_400x300.xbm"   // MFD with logo
#endif

class PageDigitalOut : public Page
{

// Status values
bool button1 = false;
bool button2 = false;
bool button3 = false;
bool button4 = false;
bool button5 = false;

    public:
    PageDigitalOut(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageDigitalOut");
    }

    // Set botton labels
    virtual void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "1";
        commonData->keydata[1].label = "2";
        commonData->keydata[2].label = "3";
        commonData->keydata[3].label = "4";
        commonData->keydata[4].label = "5";
    }

    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                       // Commit the key
        }
        // Code for button 1 
        if(key == 1){
            button1 = !button1;
            setPCF8574PortPinModul1(0, button1 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;                       // Commit the key
        }
        // Code for button 2
        if(key == 2){
            button2 = !button2;
            setPCF8574PortPinModul1(1, button2 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;                       // Commit the key
        }
        // Code for button 3
        if(key == 3){
            button3 = !button3;
            setPCF8574PortPinModul1(2, button3 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;                       // Commit the key
        }
        // Code for button 4
        if(key == 4){
            button4 = !button4;
            setPCF8574PortPinModul1(3, button4 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;                       // Commit the key
        }
        // Code for button 5
        if(key == 5){
            button5 = !button5;
            setPCF8574PortPinModul1(4, button5 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;                       // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
        String name1 = config->getString(config->mod1Out1);
        String name2 = config->getString(config->mod1Out2);
        String name3 = config->getString(config->mod1Out3);
        String name4 = config->getString(config->mod1Out4);
        String name5 = config->getString(config->mod1Out5);

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageDigitalOut");

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        // Write text
        getdisplay().setCursor(100, 50 + 8);
        getdisplay().print(name1);
        getdisplay().setCursor(100, 100 + 8);
        getdisplay().print(name2);
        getdisplay().setCursor(100, 150 + 8);
        getdisplay().print(name3);
        getdisplay().setCursor(100,200 + 8);
        getdisplay().print(name4);
        getdisplay().setCursor(100, 250 + 8);
        getdisplay().print(name5);
        // Draw bottons
        drawButtonCenter(50, 50, 40, 27, "1", commonData->fgcolor, commonData->bgcolor, button1);
        drawButtonCenter(50, 100, 40, 27, "2", commonData->fgcolor, commonData->bgcolor, button2);
        drawButtonCenter(50, 150, 40, 27, "3", commonData->fgcolor, commonData->bgcolor, button3);
        drawButtonCenter(50, 200, 40, 27, "4", commonData->fgcolor, commonData->bgcolor, button4);
        drawButtonCenter(50, 250, 40, 27, "5", commonData->fgcolor, commonData->bgcolor, button5);

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData &common){
    return new PageDigitalOut(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageDigitalOut(
    "DigitalOut",   // Page name
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    true            // Show display header on/off
);

#endif
