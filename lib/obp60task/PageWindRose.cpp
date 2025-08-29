// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

#ifdef ENABLE_CALIBRATION
#include "BoatDataCalibration.h"
#endif

class PageWindRose : public Page
{
private:
    String lengthformat;
    int16_t lp = 80; // Pointer length

public:
    PageWindRose(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageWindRose");

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
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

        // storage for hold valued
        static FormattedData bvf_awa_old;
        static FormattedData bvf_aws_old;
        static FormattedData bvf_twd_old;
        static FormattedData bvf_tws_old;
        static FormattedData bvf_dbt_old;
        static FormattedData bvf_stw_old;
        /* static String svalue1old = "";
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
        static String unit6old = ""; */

        // Get boat value for AWA
        GwApi::BoatValue *bv_awa = pageData.values[0];     // First element in list
        String name_awa = xdrDelete(bv_awa->getName(), 6); // get name without prefix and limit length
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bv_awa, logger); // Check if boat data value is to be calibrated
#endif
        FormattedData bvf_awa = commonData->fmt->formatValue(bv_awa, *commonData);
        if (bv_awa->valid) { // Save formatted data for hold feature
            bvf_awa_old = bvf_awa;
        }

        // Get boat value for AWS
        GwApi::BoatValue *bv_aws = pageData.values[1]; // Second element in list
        String name_aws = xdrDelete(bv_aws->getName(), 6); // get name without prefix and limit length
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bv_aws, logger); // Check if boat data value is to be calibrated
#endif
        FormattedData bvf_aws = commonData->fmt->formatValue(bv_aws, *commonData);
        if (bv_aws->valid) { // Save formatted data for hold feature
            bvf_aws_old = bvf_aws;
        }

        // Get boat value for TWD
        GwApi::BoatValue *bv_twd = pageData.values[2];  // Third element in list
        String name_twd = xdrDelete(bv_twd->getName(), 6); // get name without prefix and limit length
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bv_twd, logger); // Check if boat data value is to be calibrated
#endif
        FormattedData bvf_twd = commonData->fmt->formatValue(bv_twd, *commonData);
        if (bv_twd->valid) { // Save formatted data for hold feature
            bvf_twd_old = bvf_twd;
        }

        // Get boat value for TWS
        GwApi::BoatValue *bv_tws = pageData.values[3];      // Fourth element in list
        String name_tws = xdrDelete(bv_tws->getName(), 6);  // get name without prefix and limit length
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bv_tws, logger); // Check if boat data value is to be calibrated
#endif
        FormattedData bvf_tws = commonData->fmt->formatValue(bv_tws, *commonData);
        if (bv_tws->valid) { // Save formatted data for hold feature
            bvf_tws_old = bvf_tws;
        }

        // Get boat value for DBT
        GwApi::BoatValue *bv_dbt = pageData.values[4];      // Fifth element in list
        String name_dbt = xdrDelete(bv_dbt->getName(), 6);  // get name without prefix and limit length
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bv_dbt, logger); // Check if boat data value is to be calibrated
#endif
        FormattedData bvf_dbt = commonData->fmt->formatValue(bv_dbt, *commonData);
        if (bv_dbt->valid) { // Save formatted data for hold feature
            bvf_dbt_old = bvf_dbt;
        }

        // Get boat value for STW
        GwApi::BoatValue *bv_stw = pageData.values[5];      // Sixth element in list
        String name_stw = xdrDelete(bv_stw->getName(), 6);  // get name without prefix and limit length
#ifdef ENABLE_CALIBRATION
        calibrationData.calibrateInstance(bv_stw, logger); // Check if boat data value is to be calibrated
#endif
        FormattedData bvf_stw = commonData->fmt->formatValue(bv_stw, *commonData);
        if (bv_stw->valid) { // Save formatted data for hold feature
            bvf_stw_old = bvf_stw;
        }

        // Log boat values
        logger->logDebug(GwLog::LOG, "Drawing at PageWindRose, %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f,  %s:%f",
            name_awa.c_str(), bv_awa->value,
            name_aws.c_str(), bv_aws->value,
            name_twd.c_str(), bv_twd->value,
            name_tws.c_str(), bv_tws->value,
            name_dbt.c_str(), bv_dbt->value,
            name_stw.c_str(), bv_stw->value);

        // Draw page
        // *********************************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height());

        epd->setTextColor(commonData->fgcolor);

        // Show values AWA
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(10, 65);
        epd->print(holdvalues ? bvf_awa_old.value : bvf_awa.value);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(10, 95);
        epd->print(name_awa);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 115);
        epd->print(" ");
        epd->print(holdvalues ? bvf_awa_old.unit : bvf_awa.unit);

        // Horizintal separator left
        epd->fillRect(0, 149, 60, 3, commonData->fgcolor);

        // Show values AWS
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(10, 270);
        epd->print(holdvalues ? bvf_aws_old.value : bvf_aws.value);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(10, 220);
        epd->print(name_aws);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 190);
        epd->print(" ");
        epd->print(holdvalues ? bvf_aws_old.unit : bvf_aws.unit);

        // Show value TWD
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(295, 65);
        // TODO WTF? Der Formatter sollte das korrekt machen
        if (bv_twd->valid) {
            epd->print(abs(bv_twd->value * 180 / PI), 0);   // Value
        }
        else {
            epd->print(commonData->fmt->placeholder);
        }
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(335, 95);
        epd->print(name_twd);                       // Name
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(335, 115);
        epd->print(" ");
        epd->print(holdvalues ? bvf_twd_old.unit : bvf_twd.unit);

        // Horizintal separator right
        epd->fillRect(340, 149, 80, 3, commonData->fgcolor);

        // Show values TWS
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        epd->setCursor(295, 270);
        epd->print(name_tws);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(335, 220);
        epd->print(holdvalues ? bvf_tws_old.value : bvf_tws.value);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(335, 190);
        epd->print(" ");
        epd->print(holdvalues ? bvf_tws_old.unit : bvf_tws.unit);

        // *********************************************************************
        
        // Draw wind rose
        int rInstrument = 110;     // Radius of grafic instrument
        float pi = 3.141592;

        epd->fillCircle(200, 150, rInstrument + 10, commonData->fgcolor);    // Outer circle
        epd->fillCircle(200, 150, rInstrument + 7, commonData->bgcolor);     // Outer circle
        epd->fillCircle(200, 150, rInstrument - 10, commonData->fgcolor);    // Inner circle
        epd->fillCircle(200, 150, rInstrument - 13, commonData->bgcolor);    // Inner circle

        for(int i=0; i<360; i=i+10)
        {
            // Scaling values
            float x = 200 + (rInstrument-30)*sin(i/180.0*pi);  //  x-coordinate dots
            float y = 150 - (rInstrument-30)*cos(i/180.0*pi);  //  y-coordinate cots
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
            if (i % 30 == 0) {
                epd->setFont(&Ubuntu_Bold8pt8b);
                epd->print(ii);
            }

            // Draw sub scale with dots
            float x1c = 200 + rInstrument*sin(i/180.0*pi);
            float y1c = 150 - rInstrument*cos(i/180.0*pi);
            epd->fillCircle((int)x1c, (int)y1c, 2, commonData->fgcolor);
            float sinx=sin(i/180.0*pi);
            float cosx=cos(i/180.0*pi); 

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
        if (bv_aws->valid|| holdvalues || simulation) {
            float sinx = sin(bv_awa->value);     // Wind direction
            float cosx = cos(bv_awa->value);
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

        // *********************************************************************

        // Show values DBT
        epd->setFont(&DSEG7Classic_BoldItalic16pt7b);
        epd->setCursor(160, 200);
        epd->print(holdvalues ? bvf_dbt_old.value : bvf_dbt.value);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(190, 215);
        epd->print(" ");
        epd->print(holdvalues ? bvf_dbt_old.unit : bvf_dbt.unit);

        // Show values STW
        epd->setFont(&DSEG7Classic_BoldItalic16pt7b);
        epd->setCursor(160, 130);
        epd->print(holdvalues ? bvf_stw_old.value : bvf_stw.value);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(190, 90);
        epd->print(" ");
        epd->print(holdvalues ? bvf_stw_old.unit : bvf_stw.unit);

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageWindRose(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageWindRose(
    "WindRose", // Page name
    createPage, // Action
    0,          // Number of bus values depends on selection in Web configuration
    {"AWA", "AWS", "TWD", "TWS", "DBT", "STW"},    // Bus values we need in the page
    true        // Show display header on/off
);

#endif
