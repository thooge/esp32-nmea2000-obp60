// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "BoatDataCalibration.h"

class PageOneValue : public Page
{
private:
    String lengthformat;
public:
    PageOneValue(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageOneValue");

        // Get config data
        lengthformat = config->getString(config->lengthFormat);
    }

    int handleKey(int key) {
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData) {

        // Old values for hold function
        static String svalue1old = "";
        static String unit1old = "";

        
        // Get boat values
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue1, logger); // Check if boat data value is to be calibrated
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information
        String svalue1 = formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        logger->logDebug(GwLog::LOG, "Drawing at PageOneValue, %s: %f", name1.c_str(), value1);

        // Draw page
        //***********************************************************

        /// Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        // Show name
        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold32pt8b);
        epd->setCursor(20, 100);
        epd->print(name1);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(270, 100);
        if(holdvalues == false){
            epd->print(unit1);                       // Unit
        }
        else{
            epd->print(unit1old);
        }

        // Switch font if format for any values
        if(bvalue1->getFormat() == "formatLatitude" || bvalue1->getFormat() == "formatLongitude"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(20, 180);
        }
        else if(bvalue1->getFormat() == "formatTime" || bvalue1->getFormat() == "formatDate"){
            epd->setFont(&Ubuntu_Bold32pt8b);
            epd->setCursor(20, 200);
        }
        else{
            epd->setFont(&DSEG7Classic_BoldItalic60pt7b);
            epd->setCursor(20, 240);
        }

        // Show bus data
        if(holdvalues == false){
            epd->print(svalue1);                                     // Real value as formated string
        }
        else{
            epd->print(svalue1old);                                  // Old value as formated string
        }
        if(valid1 == true){
            svalue1old = svalue1;                                       // Save the old value
            unit1old = unit1;                                           // Save the old unit
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData &common){
    return new PageOneValue(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageOneValue(
    "OneValue",     // Page name
    createPage,     // Action
    1,              // Number of bus values depends on selection in Web configuration
    true            // Show display header on/off
);

#endif
