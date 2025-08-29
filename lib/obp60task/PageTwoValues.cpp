// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

#ifdef ENABLE_CALIBRATION
#include "BoatDataCalibration.h"
#endif

class PageTwoValues : public Page
{
private:
    String lengthformat;

public:
    PageTwoValues(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageTwoValue");

        // Get config data
        lengthformat = config->getString(config->lengthFormat);
    }

    int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
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

        // Old values for hold function
        static String svalue1old = "";
        static String unit1old = "";
        static String svalue2old = "";
        static String unit2old = "";

        
        // Get boat values #1
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bvalue1, logger); // Check if boat data value is to be calibrated
#endif
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = commonData->fmt->formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = commonData->fmt->formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Get boat values #2
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list
        String name2 = xdrDelete(bvalue2->getName());   // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bvalue2, logger); // Check if boat data value is to be calibrated
#endif
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = commonData->fmt->formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = commonData->fmt->formatValue(bvalue2, *commonData).unit;        // Unit of value

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        logger->logDebug(GwLog::LOG, "Drawing at PageTwoValues, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        // ############### Value 1 ################

        // Show name
        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 80);
        epd->print(name1);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 130);
        if(holdvalues == false){
            epd->print(unit1);                       // Unit
        }
        else{
            epd->print(unit1old);
        }

        // Switch font if format for any values
        if(bvalue1->getFormat() == "formatLatitude" || bvalue1->getFormat() == "formatLongitude"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(50, 130);
        }
        else if(bvalue1->getFormat() == "formatTime" || bvalue1->getFormat() == "formatDate"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(170, 105);
        }
        else{
            epd->setFont(&DSEG7Classic_BoldItalic42pt7b);
            epd->setCursor(180, 130);
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

        // ############### Horizontal Line ################

        // Horizontal line 3 pix
        epd->fillRect(0, 145, 400, 3, commonData->fgcolor);

        // ############### Value 2 ################

        // Show name
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 190);
        epd->print(name2);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 240);
        if(holdvalues == false){
            epd->print(unit2);                       // Unit
        }
        else{
            epd->print(unit2old);
        }

        // Switch font if format for any values
        if(bvalue2->getFormat() == "formatLatitude" || bvalue2->getFormat() == "formatLongitude"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(50, 240);
        }
        else if(bvalue2->getFormat() == "formatTime" || bvalue2->getFormat() == "formatDate"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(170, 215);
        }
        else{
            epd->setFont(&DSEG7Classic_BoldItalic42pt7b);
            epd->setCursor(180, 240);
        }

        // Show bus data
        if(holdvalues == false){
            epd->print(svalue2);                                     // Real value as formated string
        }
        else{
            epd->print(svalue2old);                                  // Old value as formated string
        }
        if(valid2 == true){
            svalue2old = svalue2;                                       // Save the old value
            unit2old = unit2;                                           // Save the old unit
        }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageTwoValues(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageTwoValues(
    "TwoValues",    // Page name
    createPage,     // Action
    2,              // Number of bus values depends on selection in Web configuration
    true            // Show display header on/off
);

#endif
