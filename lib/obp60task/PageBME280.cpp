// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageBME280 : public Page
{
    public:
    PageBME280(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageBME280");
    }

    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        double value1 = 0;
        double value2 = 0;
        double value3 = 0;
        String svalue1 = "";
        String svalue2 = "";
        String svalue3 = "";

        // Get config data
        String tempformat = config->getString(config->tempFormat);
        bool simulation = config->getBool(config->useSimuData);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
        String useenvsensor = config->getString(config->useEnvSensor);
        
        // Get sensor values #1
        String name1 = "Temp";                          // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        if(simulation == false){
            value1 = commonData->data.airTemperature;    // Value as double in SI unit
        }
        else{
            value1 = 23.0 + float(random(0, 10)) / 10.0;
        }
        // Display data when sensor activated
        if((useenvsensor == "BME280") or (useenvsensor == "BMP280") or (useenvsensor == "BMP180")){
            svalue1 = String(value1, 1);                // Formatted value as string including unit conversion and switching decimal places
        }
        else{
            svalue1 = "---";
        }
        String unit1 = "Deg C";                         // Unit of value

        // Get sensor values #2
        String name2 = "Humid";                         // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        if(simulation == false){
            value2 = commonData->data.airHumidity;       // Value as double in SI unit
        }
        else{
            value2 = 43 + float(random(0, 4));
        }
        // Display data when sensor activated
        if(useenvsensor == "BME280"){
            svalue2 = String(value2, 0);                // Formatted value as string including unit conversion and switching decimal places
        }
        else{
            svalue2 = "---";
        }
        String unit2 = "%";                             // Unit of value

        // Get sensor values #3
        String name3 = "Press";                         // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
         if(simulation == false){
            value3 = commonData->data.airPressure;       // Value as double in SI unit
        }
        else{
            value3 = 1006 + float(random(0, 5));
        }
        // Display data when sensor activated
        if((useenvsensor == "BME280") or (useenvsensor == "BMP280") or (useenvsensor == "BMP180")){
            svalue3 = String(value3 / 100, 1);          // Formatted value as string including unit conversion and switching decimal places
        }
        else{
            svalue3 = "---";
        }
        String unit3 = "hPa";                          // Unit of value

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageBME280, %s: %f, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        epd->setTextColor(commonData->fgcolor);

        // ############### Value 1 ################

        // Show name
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(20, 55);
        epd->print(name1);                           // Page name

        // Show unit
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 90);
        epd->print(unit1);                           // Unit

        // Switch font if format for any values
        epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
        epd->setCursor(180, 90);

        // Show bus data
        epd->print(svalue1);                         // Real value as formated string

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
        epd->print(unit2);                           // Unit

        // Switch font if format for any values
        epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
        epd->setCursor(180, 180);

        // Show bus data
        epd->print(svalue2);                         // Real value as formated string

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
        epd->print(unit3);                           // Unit

        // Switch font if format for any values
        epd->setFont(&DSEG7Classic_BoldItalic30pt7b);
        epd->setCursor(140, 270);

        // Show bus data
        epd->print(svalue3);                         // Real value as formated string

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageBME280(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageBME280(
    "BME280",  // Page name
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {},             // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true            // Show display header on/off
);

#endif
