#include "Pagedata.h"
#include "OBP60ExtensionPort.h"

class PageVoltage : public Page
{
    int dummy=0; //an example on how you would maintain some status
                 //for a page
public:
    PageVoltage(CommonData &common){
        common.logger->logDebug(GwLog::LOG,"created PageApparentWind");
        dummy=1;
    }
    virtual int handleKey(int key){
        if(key == 3){
            dummy++;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayPage(CommonData &commonData, PageData &pageData)
    {
        GwConfigHandler *config = commonData.config;
        GwLog *logger=commonData.logger;
        
        // Get config data
        bool simulation = config->getBool(config->useSimuData);
        String displaycolor = config->getString(config->displaycolor);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String batVoltage = config->getString(config->batteryVoltage);
        String batType = config->getString(config->batteryType);
        String backlightMode = config->getString(config->backlight);
        
        // Get voltage value
        String name1 = "VBat";
        double value1 = (float(analogRead(OBP_ANALOG0)) * 3.3 / 4096 + 0.17) * 20;   // Vin = 1/20 
        bool valid1 = true;

        // Optical warning by limit violation
        if(String(flashLED) == "Limit Violation"){
            // Limits for Pb battery
            if(String(batType) == "Pb" && (value1 < 11.0 || value1 > 14.5)){
                setBlinkingLED(true);
            }
            if(String(batType) == "Pb" && (value1 >= 11.0 && value1 <= 14.5)){
                setBlinkingLED(false);
                setPortPin(OBP_FLASH_LED, false);
            }      
        }
        
        // Logging voltage value
        if (value1 == NULL) return;
        LOG_DEBUG(GwLog::LOG,"Drawing at PageVoltage, p=%s, v=%f", name1, value1);

        // Draw page
        //***********************************************************

        // Clear display, set background color and text color
        int textcolor = GxEPD_BLACK;
        int pixelcolor = GxEPD_BLACK;
        int bgcolor = GxEPD_WHITE;
        if(displaycolor == "Normal"){
            textcolor = GxEPD_BLACK;
            pixelcolor = GxEPD_BLACK;
            bgcolor = GxEPD_WHITE;
        }
        else{
            textcolor = GxEPD_WHITE;
            pixelcolor = GxEPD_WHITE;
            bgcolor = GxEPD_BLACK;
        }
        //Clear display in obp60task.cpp in main loop

        // Show name
        display.setFont(&Ubuntu_Bold32pt7b);
        display.setCursor(20, 100);
        display.print(name1);                           // Page name
        
        // Show unit
        display.setFont(&Ubuntu_Bold20pt7b);
        display.setCursor(270, 100);
        display.print("V");        

        // Reading bus data or using simulation data
        display.setFont(&DSEG7Classic_BoldItalic60pt7b);
        display.setCursor(20, 240);
        if(simulation == true){
            if(batVoltage == "12V"){
                value1 = 12.0;
            }
            if(batVoltage == "24V"){
                value1 = 24.0;
            }
            value1 += float(random(0, 5)) / 10;         // Simulation data
            display.print(value1,1);
        }
        else{
            // Check vor valid real data, display also if hold values activated
            if(valid1 == true || holdvalues == true){
                // Resolution switching
                if(value1 < 10){
                    display.print(value1,2);
                }
                if(value1 >= 10 && value1 < 100){
                    display.print(value1,1);
                }
                if(value1 >= 100){
                    display.print(value1,0);
                }
            }
            else{
            display.print("---");                       // Missing bus data
            }  
        }

        // Key Layout
        display.setFont(&Ubuntu_Bold8pt7b);
        display.setCursor(115, 290);
        display.print(" [  <<<<<<      >>>>>>  ]");
        if(String(backlightMode) == "Control by Key"){
            display.setCursor(343, 290);
            display.print("[ILUM]");
        }

        // Update display
        display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, true);    // Partial update (fast)
    };
};

static Page *createPage(CommonData &common){
    return new PageVoltage(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageVoltage(
    "Voltage",      // Name of page
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {},             // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true            // Show display header on/off
);