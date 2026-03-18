// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include <PCF8574.h>                  // PCF8574 modules from Horter
#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageDigitalOut : public Page
{
private:
    // Status values
    bool button1 = false;
    bool button2 = false;
    bool button3 = false;
    bool button4 = false;
    bool button5 = false;

    // Button labels
    String name1;
    String name2;
    String name3;
    String name4;
    String name5;

public:
    PageDigitalOut(CommonData &common) : Page(common) 
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageDigitalOut");

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        name1 = config->getString(config->mod1Out1);
        name2 = config->getString(config->mod1Out2);
        name3 = config->getString(config->mod1Out3);
        name4 = config->getString(config->mod1Out4);
        name5 = config->getString(config->mod1Out5);
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
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        // Code for button 1 
        if (key == 1) {
            button1 = !button1;
            setPCF8574PortPinModul1(0, button1 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;
        }
        // Code for button 2
        if (key == 2) {
            button2 = !button2;
            setPCF8574PortPinModul1(1, button2 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;
        }
        // Code for button 3
        if (key == 3) {
            button3 = !button3;
            setPCF8574PortPinModul1(2, button3 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;
        }
        // Code for button 4
        if (key == 4) {
            button4 = !button4;
            setPCF8574PortPinModul1(3, button4 ? 0 : 1); // Attention! Inverse logic for PCF8574
            return 0;
        }
        // Code for button 5
        if (key == 5) {
            button5 = !button5;
            setPCF8574PortPinModul1(4, button5 ? 0 : 1); // Attention! Inverse logic for PCF8574
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

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageDigitalOut");

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);

        // Draw labels
        epd->setCursor(100, 50 + 8);
        epd->print(name1);
        epd->setCursor(100, 100 + 8);
        epd->print(name2);
        epd->setCursor(100, 150 + 8);
        epd->print(name3);
        epd->setCursor(100,200 + 8);
        epd->print(name4);
        epd->setCursor(100, 250 + 8);
        epd->print(name5);

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
