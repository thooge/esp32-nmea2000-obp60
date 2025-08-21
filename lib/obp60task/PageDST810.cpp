// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageDST810 : public Page
{
private:
    String lengthformat;

public:
    PageDST810(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageDST810");

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
        static String svalue2old = "";
        static String unit2old = "";
        static String svalue3old = "";
        static String unit3old = "";
        static String svalue4old = "";
        static String unit4old = "";

        // Get boat values #1
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = commonData->fmt->formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = commonData->fmt->formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Get boat values #2
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list (only one value by PageOneValue)
        String name2 = xdrDelete(bvalue2->getName());   // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = commonData->fmt->formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = commonData->fmt->formatValue(bvalue2, *commonData).unit;        // Unit of value

        // Get boat values #3
        GwApi::BoatValue *bvalue3 = pageData.values[2]; // Second element in list (only one value by PageOneValue)
        String name3 = xdrDelete(bvalue3->getName());   // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        double value3 = bvalue3->value;                 // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information 
        String svalue3 = commonData->fmt->formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = commonData->fmt->formatValue(bvalue3, *commonData).unit;        // Unit of value

        // Get boat values #4
        GwApi::BoatValue *bvalue4 = pageData.values[3]; // Second element in list (only one value by PageOneValue)
        String name4 = xdrDelete(bvalue4->getName());   // Value name
        name4 = name4.substring(0, 6);                  // String length limit for value name
        double value4 = bvalue4->value;                 // Value as double in SI unit
        bool valid4 = bvalue4->valid;                   // Valid information 
        String svalue4 = commonData->fmt->formatValue(bvalue4, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit4 = commonData->fmt->formatValue(bvalue4, *commonData).unit;        // Unit of value

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        logger->logDebug(GwLog::LOG, "Drawing at PageDST810, %s: %f, %s: %f, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3, name4.c_str(), value4);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        epd->setTextColor(commonData->fgcolor);

        // ############### Value 1 ################

        // Show name
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 55);
        epd->print("Depth");                         // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 90);
        if(holdvalues == false){
            epd->print(unit1);                       // Unit
        }
        else{
            epd->print(unit1old);
        }

        // Set font
        epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
        epd->setCursor(180, 90);

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
        epd->print("Speed");                         // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 180);
        if(holdvalues == false){
            epd->print(unit2);                       // Unit
        }
        else{
            epd->print(unit2old);
        }

        // Setfont
        epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
        epd->setCursor(180, 180);

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
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 220);
        epd->print("Log");                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(20, 240);
        if(holdvalues == false){
            epd->print(unit3);                       // Unit
        }
        else{
            epd->print(unit3old);
        }

        // Set font
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(80, 270);

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

        // ############### Vertical Line ################

        // Vertical line 3 pix
        epd->fillRect(200, 195, 3, 75, commonData->fgcolor);

        // ############### Value 4 ################

        // Show name
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(220, 220);
        epd->print("Temp");                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(220, 240);
        if(holdvalues == false){
            epd->print(unit4);                       // Unit
        }
        else{
            epd->print(unit4old);
        }

        // Set font
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(280, 270);

        // Show bus data
        if(holdvalues == false){
            epd->print(svalue4);                                     // Real value as formated string
        }
        else{
            epd->print(svalue4old);                                  // Old value as formated string
        }
        if(valid4 == true){
            svalue4old = svalue4;                                       // Save the old value
            unit4old = unit4;                                           // Save the old unit
        }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageDST810(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageDST810(
    "DST810",           // Page name
    createPage,         // Action
    0,                  // Number of bus values depends on selection in Web configuration
    {"DBT","STW","Log","WTemp"},      // Bus values we need in the page
    true                // Show display header on/off
);

#endif
