// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "BoatDataCalibration.h"

class PageWindRoseFlex : public Page
{
private:
    String lengthformat;
    int16_t lp = 80;         // Pointer length
    char source = 'A';       // data source (A)pparent | (T)rue
    String ssource="App.";   // String for Data Source 

public:
    PageWindRoseFlex(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageWindRoseFlex");

        // Get config data
        lengthformat = config->getString(config->lengthFormat);
    }

    void setupKeys() {
        Page::setupKeys();
        commonData->keydata[1].label = "SRC";
    }

    // Key functions
    int handleKey(int key) {
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;               // Commit the key
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

        static String svalue1old = "";
        static String unit1old = "";
        static String svalue2old = "";
        static String unit2old = "";
        static String svalue3old = "";
        static String unit3old = "";
        static String svalue4old = "";
        static String unit4old = "";
        static String svalue5old = "";
        static String unit5old = "";
        static String svalue6old = "";
        static String unit6old = "";

	GwApi::BoatValue *bvalue1; // Value 1 for angle
        GwApi::BoatValue *bvalue2; // Value 2 for speed

	// Get boat value for wind angle (AWA/TWA), shown by pointer
        if (source == 'A') {
            bvalue1 = pageData.values[4];
        } else {
            bvalue1 = pageData.values[6];
        }
        String name1 = bvalue1->getName().c_str();      // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue1, logger); // Check if boat data value is to be calibrated
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information
        String svalue1 = commonData->fmt->formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = commonData->fmt->formatValue(bvalue1, *commonData).unit;        // Unit of value
        if (valid1 == true) {
            svalue1old = svalue1;   	                // Save old value
            unit1old = unit1;                           // Save old unit
        }

	// Get boat value for wind speed (AWS/TWS), shown in top left corner
        if (source == 'A') {
            bvalue2 =pageData.values[5];
        } else {
            bvalue2 = pageData.values[7];
        }
        String name2 = bvalue2->getName().c_str();      // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue2, logger); // Check if boat data value is to be calibrated
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information
        if (simulation) {
            value2 = 0.62731; // some random value
        }
        String svalue2 = commonData->fmt->formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = commonData->fmt->formatValue(bvalue2, *commonData).unit;        // Unit of value
        if (valid2 == true) {
            svalue2old = svalue2;   	                // Save old value
            unit2old = unit2;                           // Save old unit
        }

        // Get boat value for bottom left corner
        GwApi::BoatValue *bvalue3 = pageData.values[0];
        String name3 = xdrDelete(bvalue3->getName());   // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue3, logger); // Check if boat data value is to be calibrated
        double value3 = bvalue3->value;                 // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information
        String svalue3 = commonData->fmt->formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = commonData->fmt->formatValue(bvalue3, *commonData).unit;        // Unit of value
        if(valid3 == true){
            svalue3old = svalue3;   	                // Save old value
            unit3old = unit3;                           // Save old unit
        }

        // Get boat value for top right corner
        GwApi::BoatValue *bvalue4 = pageData.values[1];
        String name4 = xdrDelete(bvalue4->getName());   // Value name
        name4 = name4.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue4, logger); // Check if boat data value is to be calibrated
        double value4 = bvalue4->value;                 // Value as double in SI unit
        bool valid4 = bvalue4->valid;                   // Valid information
        String svalue4 = commonData->fmt->formatValue(bvalue4, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit4 = commonData->fmt->formatValue(bvalue4, *commonData).unit;        // Unit of value
        if(valid4 == true){
            svalue4old = svalue4;   	                // Save old value
            unit4old = unit4;                           // Save old unit
        }

        // Get boat value for bottom right corner
        GwApi::BoatValue *bvalue5 = pageData.values[2];
        String name5 = xdrDelete(bvalue5->getName());   // Value name
        name5 = name5.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue5, logger); // Check if boat data value is to be calibrated
        double value5 = bvalue5->value;                 // Value as double in SI unit
        bool valid5 = bvalue5->valid;                   // Valid information
        String svalue5 = commonData->fmt->formatValue(bvalue5, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit5 = commonData->fmt->formatValue(bvalue5, *commonData).unit;        // Unit of value
        if(valid5 == true){
            svalue5old = svalue5;   	                // Save old value
            unit5old = unit5;                           // Save old unit
        }

        // Get boat value for center
        GwApi::BoatValue *bvalue6 = pageData.values[3];
        String name6 = xdrDelete(bvalue6->getName());   // Value name
        name6 = name6.substring(0, 6);                  // String length limit for value name
        calibrationData.calibrateInstance(bvalue6, logger); // Check if boat data value is to be calibrated
        double value6 = bvalue6->value;                 // Value as double in SI unit
        bool valid6 = bvalue6->valid;                   // Valid information
        String svalue6 = commonData->fmt->formatValue(bvalue6, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit6 = commonData->fmt->formatValue(bvalue6, *commonData).unit;        // Unit of value
        if(valid6 == true){
            svalue6old = svalue6;   	                // Save old value
            unit6old = unit6;                           // Save old unit
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        logger->logDebug(GwLog::LOG, "Drawing at PageWindRoseFlex, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3, name4.c_str(), value4, name5.c_str(), value5, name6.c_str(), value6);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        epd->setTextColor(commonData->fgcolor);

        // Show AWS or TWS top left
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(10, 65);
        epd->print(svalue2);                     // Value
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(10, 95);
        epd->print(name2);                       // Name
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 115);
        epd->print(" ");
        epd->print(holdvalues ? unit2old : unit2);

        // Horizintal separator left
        epd->fillRect(0, 149, 60, 3, commonData->fgcolor);

        // Show value 3 (=first user-configured parameter) at bottom left
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(10, 270);
        epd->print(svalue3);                     // Value
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(10, 220);
        epd->print(name3);                       // Name
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 190);
        epd->print(" ");
        epd->print(holdvalues ? unit3old : unit3);


        // Show value 4 (=second user-configured parameter) at top right
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(295, 65);
        if(valid3 == true){
            epd->print(svalue4);     // Value
        }
        else{
            epd->print("---");                   // Value
        }
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(335, 95);
        epd->print(name4);                       // Name
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(335, 115);
        epd->print(" ");
        epd->print(holdvalues ? unit4old : unit4);


        // Horizintal separator right
        epd->fillRect(340, 149, 80, 3, commonData->fgcolor);

        // Show value 5 (=third user-configured parameter) at bottom right
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(295, 270);
        epd->print(svalue5);                     // Value
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(335, 220);
        epd->print(name5);                       // Name
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(335, 190);
        epd->print(" ");
        epd->print(holdvalues ? unit5old : unit5);

//*******************************************************************************************

        // Draw wind rose
        int rInstrument = 110;     // Radius of grafic instrument

        epd->fillCircle(200, 150, rInstrument + 10, commonData->fgcolor);    // Outer circle
        epd->fillCircle(200, 150, rInstrument + 7, commonData->bgcolor);     // Outer circle
        epd->fillCircle(200, 150, rInstrument - 10, commonData->fgcolor);    // Inner circle
        epd->fillCircle(200, 150, rInstrument - 13, commonData->bgcolor);    // Inner circle

        for(int i=0; i<360; i=i+10)
        {
            // Scaling values
            float x = 200 + (rInstrument-30)*sin(i/180.0*M_PI);  //  x-coordinate dots
            float y = 150 - (rInstrument-30)*cos(i/180.0*M_PI);  //  y-coordinate dots
            const char *ii = "";
            switch (i) {
                case 0: ii="0"; break;
                case 30 : ii="30"; break;
                case 60 : ii="60"; break;
                case 90 : ii="90"; break;
                case 120 : ii="120"; break;
                case 150 : ii="150"; break;
                case 180 : ii="180"; break;
                case 210 : ii="210"; break;
                case 240 : ii="240"; break;
                case 270 : ii="270"; break;
                case 300 : ii="300"; break;
                case 330 : ii="330"; break;
                default: break;
            }

            // Print text centered on position x, y
            int16_t x1, y1;     // Return values of getTextBounds
            uint16_t w, h;      // Return values of getTextBounds
            epd->getTextBounds(ii, int(x), int(y), &x1, &y1, &w, &h); // Calc width of new string
            epd->setCursor(x-w/2, y+h/2);
            if(i % 30 == 0){
                epd->setFont(&Ubuntu_Bold8pt8b);
                epd->print(ii);
            }

            // Draw sub scale with dots
            float x1c = 200 + rInstrument*sin(i/180.0*M_PI);
            float y1c = 150 - rInstrument*cos(i/180.0*M_PI);
            epd->fillCircle((int)x1c, (int)y1c, 2, commonData->fgcolor);
            float sinx=sin(i/180.0*M_PI);
            float cosx=cos(i/180.0*M_PI);

            // Draw sub scale with lines (two triangles)
            if(i % 30 == 0){
                float dx=2;   // Line thickness = 2*dx+1
                float xx1 = -dx;
                float xx2 = +dx;
                float yy1 =  -(rInstrument-10);
                float yy2 =  -(rInstrument+10);
                epd->fillTriangle(200+(int)(cosx*xx1-sinx*yy1),150+(int)(sinx*xx1+cosx*yy1),
                        200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                        200+(int)(cosx*xx1-sinx*yy2),150+(int)(sinx*xx1+cosx*yy2),commonData->fgcolor);
                epd->fillTriangle(200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                        200+(int)(cosx*xx1-sinx*yy2),150+(int)(sinx*xx1+cosx*yy2),
                        200+(int)(cosx*xx2-sinx*yy2),150+(int)(sinx*xx2+cosx*yy2),commonData->fgcolor);
            }
        }

        // Draw wind pointer
        float startwidth = 8;       // Start width of pointer
        if(valid2 == true || holdvalues == true || simulation == true){
            float sinx=sin(value1);     // Wind direction
            float cosx=cos(value1);
            // Normal pointer
            // Pointer as triangle with center base 2*width
            float xx1 = -startwidth;
            float xx2 = startwidth;
            float yy1 = -startwidth;
            float yy2 = -(rInstrument-15);
            epd->fillTriangle(200+(int)(cosx*xx1-sinx*yy1),150+(int)(sinx*xx1+cosx*yy1),
                200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                200+(int)(cosx*0-sinx*yy2),150+(int)(sinx*0+cosx*yy2),commonData->fgcolor);
            // Inverted pointer
            // Pointer as triangle with center base 2*width
            float endwidth = 2;         // End width of pointer
            float ix1 = endwidth;
            float ix2 = -endwidth;
            float iy1 = -(rInstrument-15);
            float iy2 = -endwidth;
            epd->fillTriangle(200+(int)(cosx*ix1-sinx*iy1),150+(int)(sinx*ix1+cosx*iy1),
                200+(int)(cosx*ix2-sinx*iy1),150+(int)(sinx*ix2+cosx*iy1),
                200+(int)(cosx*0-sinx*iy2),150+(int)(sinx*0+cosx*iy2),commonData->fgcolor);
        }

        // Center circle
        epd->fillCircle(200, 150, startwidth + 6, commonData->bgcolor);
        epd->fillCircle(200, 150, startwidth + 4, commonData->fgcolor);

//*******************************************************************************************

        // Show value6 (=fourth user-configured parameter) and ssource, so that they do not collide with the wind pointer
    if (cos(value1) > 0) { 
        epd->setFont(&DSEG7Classic_BoldItalic16pt7b);
        epd->setCursor(160, 200);
        epd->print(svalue6);                     // Value
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(190, 215);
        epd->print(" ");
        epd->print(holdvalues ? unit6old : unit6);
        if (sin(value1) > 0) {
            epd->setCursor(160, 130);
        } else {
            epd->setCursor(220, 130);
        }
        epd->print(ssource); // true or app.
    }
    else { 
        epd->setFont(&DSEG7Classic_BoldItalic16pt7b);
        epd->setCursor(160, 130);
        epd->print(svalue6);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(190, 90);
        epd->print(" ");
        epd->print(holdvalues ? unit6old : unit6);
        if (sin(value1) > 0) {
            epd->setCursor(160, 130);
        } else {
            epd->setCursor(220, 130);
        }
        epd->print(ssource);		//true or app. 
    }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageWindRoseFlex(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (4 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageWindRoseFlex(
    "WindRoseFlex", // Page name
    createPage,     // Action
    4,              // Number of bus values depends on selection in Web configuration
    {"AWA", "AWS", "TWA", "TWS"},    // fixed  values we need in the page. They are inserted AFTER the web-configured values.	
    true            // Show display header on/off
);

#endif
