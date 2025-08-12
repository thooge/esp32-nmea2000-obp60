// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "BoatDataCalibration.h"

const int SixValues_x1 = 5;
const int SixValues_DeltaX = 200;

const int SixValues_y1 = 23;
const int SixValues_DeltaY = 83;

const int HowManyValues = 6;

class PageSixValues : public Page
{
public:
    PageSixValues(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageSixValues");
    }

    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData) {
         
        // Old values for hold function
        static String OldDataText[HowManyValues] = {"", "", "", "", "", ""};
        static String OldDataUnits[HowManyValues] = {"", "", "", "", "", ""};
    
        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        // bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
            
        GwApi::BoatValue *bvalue;
        String DataName[HowManyValues];
        double DataValue[HowManyValues];
        bool DataValid[HowManyValues];
        String DataText[HowManyValues];
        String DataUnits[HowManyValues];
        String DataFormat[HowManyValues];
    
        for (int i = 0; i < HowManyValues; i++){
            bvalue = pageData.values[i];
            DataName[i] = xdrDelete(bvalue->getName());
            DataName[i] = DataName[i].substring(0, 6);                  // String length limit for value name
            calibrationData.calibrateInstance(bvalue, logger);          // Check if boat data value is to be calibrated
            DataValue[i] = bvalue->value;                 // Value as double in SI unit
            DataValid[i] = bvalue->valid;
            DataText[i] = formatValue(bvalue, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
            DataUnits[i] = formatValue(bvalue, *commonData).unit;   
            DataFormat[i] = bvalue->getFormat();     // Unit of value
        }
    
        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }
    
        if (bvalue == NULL) return PAGE_OK; // WTF why this statement?
           
        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
        epd->setTextColor(commonData->fgcolor);

        for (int i = 0; i < ( HowManyValues / 2 ); i++) {
            if (i < (HowManyValues / 2) - 1) { // Don't draw horizontal line after last line of values -> standard design
                // Horizontal line 3 pix
                epd->fillRect(0, SixValues_y1+(i+1)*SixValues_DeltaY, 400, 3, commonData->fgcolor);
            }
            for (int j = 0; j < 2; j++) {
                int ValueIndex = i * 2 + j;
                int x0 = SixValues_x1 + j * SixValues_DeltaX;
                int y0 = SixValues_y1 + i * SixValues_DeltaY;
                LOG_DEBUG(GwLog::LOG, "Drawing at PageSixValue: %d %s %f %s",  ValueIndex,  DataName[ValueIndex], DataValue[ValueIndex], DataFormat[ValueIndex]);
    
                // Show name
                epd->setFont(&Ubuntu_Bold12pt8b);
                epd->setCursor(x0, y0+25);
                epd->print(DataName[ValueIndex]);                           // Page name
    
                // Show unit
                epd->setFont(&Ubuntu_Bold8pt8b);
                epd->setCursor(x0, y0+72);
                if (holdvalues == false) {
                    epd->print(DataUnits[ValueIndex]);                       // Unit
                } else {
                    epd->print(OldDataUnits[ValueIndex]);
                }
    
                // Switch font if format for any values
                if (DataFormat[ValueIndex] == "formatLatitude" || DataFormat[ValueIndex] == "formatLongitude") {
                    epd->setFont(&Ubuntu_Bold12pt8b);
                    epd->setCursor(x0+10, y0+60);
                } 
                else if (DataFormat[ValueIndex] == "formatTime" || DataFormat[ValueIndex] == "formatDate") {
                    epd->setFont(&Ubuntu_Bold16pt8b);
                    epd->setCursor(x0+20,y0+55);
                } 
                 // pressure in hPa          
                else if (DataFormat[ValueIndex] == "formatXdr:P:P") {
                    epd->setFont(&DSEG7Classic_BoldItalic26pt7b);
                    epd->setCursor(x0+5, y0+70);
                }
                // RPM
                else if (DataFormat[ValueIndex] == "formatXdr:T:R") {
                    epd->setFont(&DSEG7Classic_BoldItalic16pt7b);
                    epd->setCursor(x0+25, y0+70);
                }
                else {
                    epd->setFont(&DSEG7Classic_BoldItalic26pt7b);
                    if (DataText[ValueIndex][0] == '-' ) {
                        epd->setCursor(x0+25, y0+70);
                    } else {
                        epd->setCursor(x0+65, y0+70);
                    }
                }
    
                // Show bus data
                if (holdvalues == false) {
                    epd->print(DataText[ValueIndex]); // Real value as formated string
                } else{
                    epd->print(OldDataText[ValueIndex]); // Old value as formated string
                }
                if (DataValid[ValueIndex] == true) {
                    OldDataText[ValueIndex] = DataText[ValueIndex];                                       // Save the old value
                    OldDataUnits[ValueIndex] = DataUnits[ValueIndex];                                           // Save the old unit
                }
            } // for j
            // Vertical line 3 pix
            epd->fillRect(SixValues_x1+SixValues_DeltaX-8, SixValues_y1+i*SixValues_DeltaY, 3, SixValues_DeltaY, commonData->fgcolor);
        } // for i
    
        return PAGE_UPDATE;
    };
    
};

static Page *createPage(CommonData &common){
    return new PageSixValues(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageSixValues(
    "SixValues",    // Page name
    createPage,     // Action
    6,              // Number of bus values depends on selection in Web configuration
    true            // Show display header on/off
);

#endif
