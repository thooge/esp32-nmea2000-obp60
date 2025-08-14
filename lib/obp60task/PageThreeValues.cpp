// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "BoatDataCalibration.h"

class PageThreeValues : public Page
{
private:
    String lengthformat;

public:
    PageThreeValues(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageThreeValue");

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
        static String svalue3old = "";
        static String unit3old = "";

        // Get boat values #1
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue1, logger); // Check if boat data value is to be calibrated
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Get boat values #2
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list
        String name2 = xdrDelete(bvalue2->getName());   // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue2, logger); // Check if boat data value is to be calibrated
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = formatValue(bvalue2, *commonData).unit;        // Unit of value

        // Get boat values #3
        GwApi::BoatValue *bvalue3 = pageData.values[2]; // Third element in list
        String name3 = xdrDelete(bvalue3->getName());      // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue3, logger); // Check if boat data value is to be calibrated
        double value3 = bvalue3->value;                 // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information 
        String svalue3 = formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = formatValue(bvalue3, *commonData).unit;        // Unit of value

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        logger->logDebug(GwLog::LOG, "Drawing at PageThreeValues, %s: %f, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3);

        // Draw page
        //***********************************************************

        /// Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        // ############### Value 1 ################

        // Show name
        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 55);
        epd->print(name1);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 90);
        if(holdvalues == false){
            epd->print(unit1);                       // Unit
        }
        else{
            epd->print(unit1old);
        }

        // Switch font if format for any values
        if(bvalue1->getFormat() == "formatLatitude" || bvalue1->getFormat() == "formatLongitude"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(50, 90);
        }
        else if(bvalue1->getFormat() == "formatTime" || bvalue1->getFormat() == "formatDate"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(170, 68);
        }
        else{
            epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
            epd->setCursor(180, 90);
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
        epd->fillRect(0, 105, 400, 3, commonData->fgcolor);

        // ############### Value 2 ################

        // Show name
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 145);
        epd->print(name2);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 180);
        if(holdvalues == false){
            epd->print(unit2);                       // Unit
        }
        else{
            epd->print(unit2old);
        }

        // Switch font if format for any values
        if(bvalue2->getFormat() == "formatLatitude" || bvalue2->getFormat() == "formatLongitude"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(50, 180);
        }
        else if(bvalue2->getFormat() == "formatTime" || bvalue2->getFormat() == "formatDate"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(170, 158);
        }
        else{
            epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
            epd->setCursor(180, 180);
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

        // ############### Horizontal Line ################

        // Horizontal line 3 pix
        epd->fillRect(0, 195, 400, 3, commonData->fgcolor);

        // ############### Value 3 ################

        // Show name
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 235);
        epd->print(name3);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 270);
        if(holdvalues == false){
            epd->print(unit3);                       // Unit
        }
        else{
            epd->print(unit3old);
        }

        // Switch font if format for any values
        if(bvalue3->getFormat() == "formatLatitude" || bvalue3->getFormat() == "formatLongitude"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(50, 270);
        }
        else if(bvalue3->getFormat() == "formatTime" || bvalue3->getFormat() == "formatDate"){
            epd->setFont(&Ubuntu_Bold20pt8b);
            epd->setCursor(170, 248);
        }
        else{
            epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
            epd->setCursor(180, 270);
        }

        // Show bus data
        if(holdvalues == false){
            epd->print(svalue3);                                     // Real value as formated string
        }
        else{
            epd->print(svalue3old);                                  // Old value as formated string
        }
        if(valid3 == true){
            svalue3old = svalue3;                                       // Save the old value
            unit3old = unit3;                                           // Save the old unit
        }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageThreeValues(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageThreeValues(
    "ThreeValues",  // Page name
    createPage,     // Action
    3,              // Number of bus values depends on selection in Web configuration
    true            // Show display header on/off
);

#endif
