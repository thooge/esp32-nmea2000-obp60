#ifdef BOARD_OBP60S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "movingAvg.h"              // Lib for moving average building

class PageSolar : public Page
{
public:
    PageSolar(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageSolar");
    }
    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    virtual void displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;
        
        // Get config data
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String batVoltage = config->getString(config->batteryVoltage);
        int solPower = config->getInt(config->solarPower);
        String backlightMode = config->getString(config->backlight);
        String powerSensor = config->getString(config->usePowSensor2);

        double value1 = 0;  // Solar voltage
        double value2 = 0;  // Solar current
        double value3 = 0;  // Solar output power
        double valueTrend = 0;  // Average over 10 values
        int solPercentage = 0;  // Solar load
        
        // Get voltage value
        String name1 = "VSol";

        // Get raw value for trend indicator
        if(powerSensor != "off"){
            value1 = commonData->data.solarVoltage;  // Use voltage from external sensor
        }
        else{
            value1 = commonData->data.batteryVoltage; // Use internal voltage sensor
        }
        value2 = commonData->data.solarCurrent;
        value3 = commonData->data.solarPower;
        solPercentage = value3 * 100 / (double)solPower;    // Load value
        // Limits for battery level
        if(solPercentage < 0) solPercentage = 0;
        if(solPercentage > 99) solPercentage = 99;

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
        LOG_DEBUG(GwLog::LOG,"Drawing at PageSolar, Type:%iW %s:=%f", solPower, name1.c_str(), value1);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        getdisplay().setTextColor(commonData->fgcolor);

        // Show name
        getdisplay().setFont(&Ubuntu_Bold20pt7b);
        getdisplay().setCursor(10, 65);
        getdisplay().print("Solar");

        // Show voltage type
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(10, 140);
        int bvoltage = 0;
        if(String(batVoltage) == "12V") bvoltage = 12;
        else bvoltage = 24;
        getdisplay().print(bvoltage);
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        getdisplay().print("V");

        // Show solar power
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(10, 200);
        if(solPower <= 999) getdisplay().print(solPower, 0);
        if(solPower > 999) getdisplay().print(float(solPower/1000.0), 1);
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        if(solPower <= 999) getdisplay().print("W");
        if(solPower > 999) getdisplay().print("kW");

        // Show info
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(10, 235);
        getdisplay().print("Installed");
        getdisplay().setCursor(10, 255);
        getdisplay().print("Solar Modul");

        // Show solar panel
        solarGraphic(150, 45, commonData->fgcolor, commonData->bgcolor);

        // Show load level in percent
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(150, 200);
        getdisplay().print(solPercentage);
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        getdisplay().print("%");
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(150, 235);
        getdisplay().print("Load");

        // Show sensor type info
        String i2cAddr = "";
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(270, 60);
        if(powerSensor == "off") getdisplay().print("Internal");
        if(powerSensor == "INA219"){
            getdisplay().print("INA219");
            i2cAddr = " (0x" + String(INA219_I2C_ADDR2, HEX) + ")";
        }
        if(powerSensor == "INA226"){
            getdisplay().print("INA226");
            i2cAddr = " (0x" + String(INA226_I2C_ADDR2, HEX) + ")";
        }
        getdisplay().print(i2cAddr);
        getdisplay().setCursor(270, 80);
        getdisplay().print("Sensor Modul");

        // Reading bus data or using simulation data
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(260, 140);
        if(simulation == true){
            if(batVoltage == "12V"){
                value1 = 12.0;
            }
            if(batVoltage == "24V"){
                value1 = 24.0;
            }
            value1 += float(random(0, 5)) / 10;         // Simulation data
            getdisplay().print(value1,1);
        }
        else{
            // Check for valid real data, display also if hold values activated
            if(valid1 == true || holdvalues == true){
                // Resolution switching
                if(value1 <= 9.9) getdisplay().print(value1, 2);
                if(value1 > 9.9 && value1 <= 99.9)getdisplay().print(value1, 1);
                if(value1 > 99.9) getdisplay().print(value1, 0);
            }
            else{
            getdisplay().print("---");                       // Missing bus data
            }
        }
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        getdisplay().print("V");

        // Show actual current in A
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(260, 200);
        if((powerSensor == "INA219" || powerSensor == "INA226") && simulation == false){
            if(value2 <= 9.9) getdisplay().print(value2, 2);
            if(value2 > 9.9 && value2 <= 99.9)getdisplay().print(value2, 1);
            if(value2 > 99.9) getdisplay().print(value2, 0);
        }
        else  getdisplay().print("---");
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        getdisplay().print("A");

        // Show actual consumption in W
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(260, 260);
        if((powerSensor == "INA219" || powerSensor == "INA226") && simulation == false){
            if(value3 <= 9.9) getdisplay().print(value3, 2);
            if(value3 > 9.9 && value3 <= 99.9)getdisplay().print(value3, 1);
            if(value3 > 99.9) getdisplay().print(value3, 0);
        }
        else  getdisplay().print("---");
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        getdisplay().print("W");

        // Update display
        getdisplay().nextPage();    // Partial update (fast)
    };
};

static Page *createPage(CommonData &common){
    return new PageSolar(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageSolar(
    "Solar",        // Name of page
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {},             // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true            // Show display header on/off
);

#endif
