// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "movingAvg.h"              // Lib for moving average building

class PageGenerator : public Page
{
private:
    String batVoltage;
    int genPower;
    String powerSensor;

public:
    PageGenerator(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageGenerator");

        // Get config data
        batVoltage = config->getString(config->batteryVoltage);
        genPower = config->getInt(config->genPower);
        powerSensor = config->getString(config->usePowSensor3);
    }

    int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData) {
        
        double value1 = 0;  // Solar voltage
        double value2 = 0;  // Solar current
        double value3 = 0;  // Solar output power
        double valueTrend = 0;  // Average over 10 values
        int genPercentage = 0;  // Power generator load
        
        // Get voltage value
        String name1 = "VGen";

        // Get raw value for trend indicator
        if(powerSensor != "off"){
            value1 = commonData->data.generatorVoltage;  // Use voltage from external sensor
        }
        else{
            value1 = commonData->data.batteryVoltage; // Use internal voltage sensor
        }
        value2 = commonData->data.generatorCurrent;
        value3 = commonData->data.generatorPower;
        genPercentage = value3 * 100 / (double)genPower;    // Load value
        // Limits for battery level
        if(genPercentage < 0) genPercentage = 0;
        if(genPercentage > 99) genPercentage = 99;

        bool valid1 = true;

        // Optical warning by limit violation
        if(String(flashLED) == "Limit Violation"){
            // Over voltage
            if(value1 > 14.8 && batVoltage == "12V"){
                setBlinkingLED(true);
            }
            if(value1 <= 14.8 && batVoltage == "12V"){
                setBlinkingLED(false);
            }
            if(value1 > 29.6 && batVoltage == "24V"){
                setBlinkingLED(true);
            }
            if(value1 <= 29.6 && batVoltage == "24V"){
                setBlinkingLED(false);
            }     
        }
        
        // Logging voltage value
        logger->logDebug(GwLog::LOG, "Drawing at PageGenerator, Type:%iW %s:=%f", genPower, name1.c_str(), value1);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        epd->setTextColor(commonData->fgcolor);

        // Show name
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(10, 65);
        epd->print("Power");
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(12, 82);
        epd->print("Generator");

        // Show voltage type
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(10, 140);
        int bvoltage = 0;
        if(String(batVoltage) == "12V") bvoltage = 12;
        else bvoltage = 24;
        epd->print(bvoltage);
        epd->setFont(&Ubuntu_Bold16pt8b);
        epd->print("V");

        // Show solar power
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(10, 200);
        if(genPower <= 999) epd->print(genPower, 0);
        if(genPower > 999) epd->print(float(genPower/1000.0), 1);
        epd->setFont(&Ubuntu_Bold16pt8b);
        if(genPower <= 999) epd->print("W");
        if(genPower > 999) epd->print("kW");

        // Show info
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 235);
        epd->print("Installed");
        epd->setCursor(10, 255);
        epd->print("Power Modul");

        // Show generator
        generatorGraphic(200, 95, commonData->fgcolor, commonData->bgcolor);

        // Show load level in percent
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(150, 200);
        epd->print(genPercentage);
        epd->setFont(&Ubuntu_Bold16pt8b);
        epd->print("%");
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(150, 235);
        epd->print("Load");

        // Show sensor type info
        String i2cAddr = "";
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(270, 60);
        if(powerSensor == "off") epd->print("Internal");
        if(powerSensor == "INA219"){
            epd->print("INA219");
            i2cAddr = " (0x" + String(INA219_I2C_ADDR3, HEX) + ")";
        }
        if(powerSensor == "INA226"){
            epd->print("INA226");
            i2cAddr = " (0x" + String(INA226_I2C_ADDR3, HEX) + ")";
        }
        epd->print(i2cAddr);
        epd->setCursor(270, 80);
        epd->print("Sensor Modul");

        // Reading bus data or using simulation data
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(260, 140);
        if(simulation == true){
            if(batVoltage == "12V"){
                value1 = 12.0;
            }
            if(batVoltage == "24V"){
                value1 = 24.0;
            }
            value1 += float(random(0, 5)) / 10;         // Simulation data
            epd->print(value1,1);
        }
        else{
            // Check for valid real data, display also if hold values activated
            if(valid1 == true || holdvalues == true){
                // Resolution switching
                if(value1 <= 9.9) epd->print(value1, 2);
                if(value1 > 9.9 && value1 <= 99.9)epd->print(value1, 1);
                if(value1 > 99.9) epd->print(value1, 0);
            }
            else{
            epd->print("---");                       // Missing bus data
            }
        }
        epd->setFont(&Ubuntu_Bold16pt8b);
        epd->print("V");

        // Show actual current in A
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(260, 200);
        if((powerSensor == "INA219" || powerSensor == "INA226") && simulation == false){
            if(value2 <= 9.9) epd->print(value2, 2);
            if(value2 > 9.9 && value2 <= 99.9)epd->print(value2, 1);
            if(value2 > 99.9) epd->print(value2, 0);
        }
        else  epd->print("---");
        epd->setFont(&Ubuntu_Bold16pt8b);
        epd->print("A");

        // Show actual consumption in W
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(260, 260);
        if((powerSensor == "INA219" || powerSensor == "INA226") && simulation == false){
            if(value3 <= 9.9) epd->print(value3, 2);
            if(value3 > 9.9 && value3 <= 99.9)epd->print(value3, 1);
            if(value3 > 99.9) epd->print(value3, 0);
        }
        else  epd->print("---");
        epd->setFont(&Ubuntu_Bold16pt8b);
        epd->print("W");

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageGenerator(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageGenerator(
    "Generator",    // Name of page
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {},             // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true            // Show display header on/off
);

#endif
