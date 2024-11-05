#ifdef BOARD_OBP60S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include <Adafruit_GFX.h> 

class PageOneValue : public Page{
    bool keylock = false;               // Keylock
    

    public:
    PageOneValue(CommonData &common){
        common.logger->logDebug(GwLog::LOG,"Show PageOneValue");
    }

    virtual int handleKey(int key){
        if(key == 11){                  // Code for keylock
            keylock = !keylock;         // Toggle keylock
            return 0;                   // Commit the key
        }
        return key;
    }

    virtual void displayPage(CommonData &commonData, PageData &pageData){
        GwConfigHandler *config = commonData.config;
        GwLog *logger=commonData.logger;
        GFXcanvas1 canvas(400, 300);

        // Old values for hold function
        static String svalue1old = "";
        static String unit1old = "";

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        // bool simulation = config->getBool(config->useSimuData);
        String displaycolor = config->getString(config->displaycolor);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
        
        // Get boat values
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = formatValue(bvalue1, commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, commonData).unit;        // Unit of value

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        if (bvalue1 == NULL) return;
        LOG_DEBUG(GwLog::LOG,"Drawing at PageOneValue, %s: %f", name1.c_str(), value1);

        // Draw page
        //***********************************************************

        // Set background color and text color
        int textcolor = GxEPD_BLACK;
        int pixelcolor = GxEPD_BLACK;
        int bgcolor = GxEPD_WHITE;
        if(displaycolor != "Normal"){
            textcolor = GxEPD_WHITE;
            pixelcolor = GxEPD_WHITE;
            bgcolor = GxEPD_BLACK;
        }
        /// Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        // Show name
        canvas.setTextColor(textcolor);
        canvas.setFont(&Ubuntu_Bold32pt7b);
        canvas.setCursor(20, 100);
        canvas.print(name1);                           // Page name

        // Show unit
        canvas.setTextColor(textcolor);
        canvas.setFont(&Ubuntu_Bold20pt7b);
        canvas.setCursor(270, 100);
        if(holdvalues == false){
            canvas.print(unit1);                       // Unit
        }
        else{
            canvas.print(unit1old);
        }

        // Switch font if format for any values
        if(bvalue1->getFormat() == "formatLatitude" || bvalue1->getFormat() == "formatLongitude"){
            canvas.setFont(&Ubuntu_Bold20pt7b);
            canvas.setCursor(20, 180);
        }
        else if(bvalue1->getFormat() == "formatTime" || bvalue1->getFormat() == "formatDate"){
            canvas.setFont(&Ubuntu_Bold32pt7b);
            canvas.setCursor(20, 200);
        }
        else{
            canvas.setFont(&DSEG7Classic_BoldItalic60pt7b);
            canvas.setCursor(20, 240);
        }

        // Show bus data
        if(holdvalues == false){
            canvas.print(svalue1);                                     // Real value as formated string
        }
        else{
            canvas.print(svalue1old);                                  // Old value as formated string
        }
        if(valid1 == true){
            svalue1old = svalue1;                                       // Save the old value
            unit1old = unit1;                                           // Save the old unit
        }

        // Key Layout
        canvas.setTextColor(textcolor);
        canvas.setFont(&Ubuntu_Bold8pt7b);
        if(keylock == false){
            canvas.setCursor(130, 290);
            canvas.print("[  <<<<  " + String(commonData.data.actpage) + "/" + String(commonData.data.maxpage) + "  >>>>  ]");
            if(String(backlightMode) == "Control by Key"){                  // Key for illumination
                canvas.setCursor(343, 290);
                canvas.print("[ILUM]");
            }
        }
        else{
            canvas.setCursor(130, 290);
            canvas.print(" [    Keylock active    ]");
        }

        // transfer framebuffer to display
        getdisplay().drawBitmap(0, 0, canvas.getBuffer(), 400, 300, pixelcolor);

        // Update display
        getdisplay().nextPage();    // Partial update (fast)

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
