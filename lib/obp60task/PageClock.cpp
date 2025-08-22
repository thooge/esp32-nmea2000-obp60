// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

/*
 * TODO mode: race timer: keys
 *   - prepare: set countdown to 5min
 *     reset: abort current countdown and start over with 5min preparation
 *   - 5min: key press
 *   - 4min: key press to sync
 *   - 1min: buzzer signal
 *   - start: buzzer signal for start
 *
 */

class PageClock : public Page
{
private:
    fmtDate dateformat;
    int simtime;
    bool keylock = false;
    char source = 'R';  // time source (R)TC | (G)PS | (N)TP
    char mode = 'A';    // display mode (A)nalog | (D)igital | race (T)imer
    char tz = 'L';      // time zone (L)ocal | (U)TC
    double timezone = 0; // there are timezones with non int offsets, e.g. 5.5 or 5.75
    double homelat;
    double homelon;
    bool homevalid = false; // homelat and homelon are valid

public:
    PageClock(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageClock");

        // Get config data
        dateformat = common.fmt->getDateFormat(config->getString(config->dateFormat));
        timezone = config->getString(config->timeZone).toDouble();
        homelat = config->getString(config->homeLAT).toDouble();
        homelon = config->getString(config->homeLON).toDouble();
        homevalid = homelat >= -180.0 and homelat <= 180 and homelon >= -90.0 and homelon <= 90.0;
        simtime = 38160; // time value 11:36

#ifdef BOARD_OBP60S3
        // WIP time source
        String use_rtc = config->getString(config->useRTC);
        if (use_rtc == "off") {
            source = 'G';
        }
#endif
    }

    void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "SRC";
        commonData->keydata[1].label = "MODE";
        commonData->keydata[4].label = "TZ";
    }

    // Key functions
    int handleKey(int key){
        // Time source
        if (key == 1) {
            if (source == 'G') {
                source = 'R';
            } else {
                source = 'G';
            }
            return 0;
        }
        if (key == 2) {
            if (mode == 'A') {
                mode = 'D';
            } else if (mode == 'D') {
                mode = 'T';
            } else {
                mode = 'A';
            }
            return 0;
        }
        // Time zone: Local / UTC
        if (key == 5) {
            if (tz == 'L') {
                tz = 'U';
            } else {
                tz = 'L';
            }
            return 0;
        }

        // Keylock function
        if(key == 11){              // Code for keylock
            keylock = !keylock;     // Toggle keylock
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

        static String svalue5old = "";
        static String svalue6old = "";

        double value1 = 0; // GPS time
        double value2 = 0; // GPS date FIXME date defined as uint32_t!
        double value3 = 0; // HDOP
        bool gpsvalid = false;

        // Get boat values for GPS time
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = bvalue1->getName().c_str();      // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        if(simulation == false){
            value1 = bvalue1->value;                    // Value as double in SI unit
        }
        else{
            value1 = simtime++;                         // Simulation data for time value 11:36 in seconds
        }                                               // Other simulation data see OBP60Formatter.cpp
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = commonData->fmt->formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = commonData->fmt->formatValue(bvalue1, *commonData).unit;        // Unit of value
        if(valid1 == true){
            svalue1old = svalue1;                       // Save old value
            unit1old = unit1;                           // Save old unit
        }

        // Get boat values for GPS date
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list (only one value by PageOneValue)
        String name2 = bvalue2->getName().c_str();      // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        value2 = bvalue2->value;                        // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = commonData->fmt->formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = commonData->fmt->formatValue(bvalue2, *commonData).unit;        // Unit of value
        if(valid2 == true){
            svalue2old = svalue2;                       // Save old value
            unit2old = unit2;                           // Save old unit
        }

        // Get boat values for HDOP date
        GwApi::BoatValue *bvalue3 = pageData.values[2]; // Third element in list (only one value by PageOneValue)
        String name3 = bvalue3->getName().c_str();      // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        value3 = bvalue3->value;                        // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information 
        String svalue3 = commonData->fmt->formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = commonData->fmt->formatValue(bvalue3, *commonData).unit;        // Unit of value
        if(valid3 == true){
            svalue3old = svalue3;                       // Save old value
            unit3old = unit3;                           // Save old unit
        }

        // GPS date and time are valid and can be used
        gpsvalid = (valid1 && valid2 && valid3);

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        logger->logDebug(GwLog::LOG, "Drawing at PageClock, %s:%f,  %s:%f, %s:%f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        epd->setTextColor(commonData->fgcolor);

        time_t tv = mktime(&commonData->data.rtcTime) + timezone * 3600;
        struct tm *local_tm = localtime(&tv);

        // Show values GPS date
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 65);
        if (holdvalues == false) {
            if (source == 'G') {
                 // GPS value
                 epd->print(svalue2);
            } else if (commonData->data.rtcValid) {
                // RTC value
                 if (tz == 'L') {
                     epd->print(formatDate(dateformat, local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday));
                 }
                 else {
                     epd->print(formatDate(dateformat, commonData->data.rtcTime.tm_year + 1900, commonData->data.rtcTime.tm_mon + 1, commonData->data.rtcTime.tm_mday));
                 }
            } else {
                epd->print(commonData->fmt->placeholder);
            }
        } else {
            epd->print(svalue2old);
        }
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(10, 95);
        epd->print("Date");                          // Name

        // Horizintal separator left
        epd->fillRect(0, 149, 60, 3, commonData->fgcolor);

        // Show values GPS time
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(10, 250);
        if (holdvalues == false) {
            if (source == 'G') {
                epd->print(svalue1); // Value
            }
            else if (commonData->data.rtcValid) {
                 if (tz == 'L') {
                      epd->print(formatTime(fmtTime::MMHHSS, local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec));
                 }
                 else {
                      epd->print(formatTime(fmtTime::MMHHSS, commonData->data.rtcTime.tm_hour, commonData->data.rtcTime.tm_min, commonData->data.rtcTime.tm_sec));
                 }
            } else {
                epd->print(commonData->fmt->placeholder);
            }
        }
        else {
             epd->print(svalue1old);
        }
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(10, 220);
        epd->print("Time");                          // Name

        // Show values sunrise
        String sunrise = commonData->fmt->placeholder;
        if (((source == 'G') and gpsvalid) or (homevalid and commonData->data.rtcValid)) {
            sunrise = String(commonData->sundata.sunriseHour) + ":" + String(commonData->sundata.sunriseMinute + 100).substring(1);
            svalue5old = sunrise;
        } else if (simulation) {
            sunrise = String("06:42");
        }

        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(335, 65);
        if(holdvalues == false) epd->print(sunrise); // Value
        else epd->print(svalue5old);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(335, 95);
        epd->print("SunR");                          // Name

        // Horizintal separator right
        epd->fillRect(340, 149, 80, 3, commonData->fgcolor);

        // Show values sunset
        String sunset = commonData->fmt->placeholder;
        if (((source == 'G') and gpsvalid) or (homevalid and commonData->data.rtcValid)) {
            sunset = String(commonData->sundata.sunsetHour) + ":" +  String(commonData->sundata.sunsetMinute + 100).substring(1);
            svalue6old = sunset;
        } else if (simulation) {
            sunset = String("21:03");
        }

        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(335, 250);
        if(holdvalues == false) epd->print(sunset);  // Value
        else epd->print(svalue6old);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(335, 220);
        epd->print("SunS");                          // Name

//*******************************************************************************************

        // Draw clock
        int rInstrument = 110;     // Radius of clock
        float pi = 3.141592;

        epd->fillCircle(200, 150, rInstrument + 10, commonData->fgcolor);    // Outer circle
        epd->fillCircle(200, 150, rInstrument + 7, commonData->bgcolor);     // Outer circle

        for(int i=0; i<360; i=i+1)
        {
            // Scaling values
            float x = 200 + (rInstrument-30)*sin(i/180.0*pi);  //  x-coordinate dots
            float y = 150 - (rInstrument-30)*cos(i/180.0*pi);  //  y-coordinate cots 
            const char *ii = "";
            switch (i)
            {
            case 0: ii="12"; break;
            case 30 : ii=""; break;
            case 60 : ii=""; break;
            case 90 : ii="3"; break;
            case 120 : ii=""; break;
            case 150 : ii=""; break;
            case 180 : ii="6"; break;
            case 210 : ii=""; break;
            case 240 : ii=""; break;
            case 270 : ii="9"; break;
            case 300 : ii=""; break;
            case 330 : ii=""; break;
            default: break;
            }

            // Print text centered on position x, y
            int16_t x1, y1;     // Return values of getTextBounds
            uint16_t w, h;      // Return values of getTextBounds
            epd->getTextBounds(ii, int(x), int(y), &x1, &y1, &w, &h); // Calc width of new string
            epd->setCursor(x-w/2, y+h/2);
            if(i % 30 == 0){
                epd->setFont(&Ubuntu_Bold12pt8b);
                epd->print(ii);
            }

            // Draw sub scale with dots
            float sinx = 0;
            float cosx = 0;
             if(i % 6 == 0){
                float x1c = 200 + rInstrument*sin(i/180.0*pi);
                float y1c = 150 - rInstrument*cos(i/180.0*pi);
                epd->fillCircle((int)x1c, (int)y1c, 2, commonData->fgcolor);
                sinx=sin(i/180.0*pi);
                cosx=cos(i/180.0*pi);
             }

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

        // Print Unit in clock
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(175, 110);
        if(holdvalues == false){
            epd->print(tz == 'L' ? "LOT" : "UTC");
        }
        else{
            epd->print(unit2old); // date unit
        }

        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(185, 190);
        if (source == 'G') {
            epd->print("GPS");
        } else {
            epd->print("RTC");
        }

        // Clock values
        double hour = 0;
        double minute = 0;
        if (source == 'R') {
            if (tz == 'L') {
                time_t tv = mktime(&commonData->data.rtcTime) + timezone * 3600;
                struct tm *local_tm = localtime(&tv);
                minute = local_tm->tm_min;
                hour = local_tm->tm_hour;
            } else {
                minute = commonData->data.rtcTime.tm_min;
                hour = commonData->data.rtcTime.tm_hour;
            }
            hour += minute / 60;
        } else {
            if (tz == 'L') {
                value1 += timezone * 3600;
            }
            if (value1 > 86400) {value1 -= 86400;}
            if (value1 < 0) {value1 += 86400;}
            hour = (value1 / 3600.0);
    //        minute = (hour - int(hour)) * 3600.0 / 60.0;        // Analog minute pointer smooth moving
            minute = int((hour - int(hour)) * 3600.0 / 60.0);   // Jumping minute pointer from minute to minute
        }
        if (hour > 12) {
            hour -= 12.0;
        }
        logger->logDebug(GwLog::DEBUG, "... PageClock, value1: %f hour: %f minute:%f", value1, hour, minute);

        // Draw hour pointer
        float startwidth = 8;       // Start width of pointer
        if(valid1 == true || (source == 'R' && commonData->data.rtcValid) || holdvalues == true || simulation == true){
            float sinx=sin(hour * 30.0 * pi / 180);     // Hour
            float cosx=cos(hour * 30.0 * pi / 180);
            // Normal pointer
            // Pointer as triangle with center base 2*width
            float xx1 = -startwidth;
            float xx2 = startwidth;
            float yy1 = -startwidth;
            float yy2 = -(rInstrument * 0.5);
            epd->fillTriangle(200+(int)(cosx*xx1-sinx*yy1),150+(int)(sinx*xx1+cosx*yy1),
                200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                200+(int)(cosx*0-sinx*yy2),150+(int)(sinx*0+cosx*yy2),commonData->fgcolor);
            // Inverted pointer
            // Pointer as triangle with center base 2*width
            float endwidth = 2;         // End width of pointer
            float ix1 = endwidth;
            float ix2 = -endwidth;
            float iy1 = -(rInstrument * 0.5);
            float iy2 = -endwidth;
            epd->fillTriangle(200+(int)(cosx*ix1-sinx*iy1),150+(int)(sinx*ix1+cosx*iy1),
                200+(int)(cosx*ix2-sinx*iy1),150+(int)(sinx*ix2+cosx*iy1),
                200+(int)(cosx*0-sinx*iy2),150+(int)(sinx*0+cosx*iy2),commonData->fgcolor);
        }

        // Draw minute pointer
        startwidth = 8;       // Start width of pointer
        if(valid1 == true || (source == 'R' && commonData->data.rtcValid) || holdvalues == true || simulation == true){
            float sinx=sin(minute * 6.0 * pi / 180);     // Minute
            float cosx=cos(minute * 6.0 * pi / 180);
            // Normal pointer
            // Pointer as triangle with center base 2*width
            float xx1 = -startwidth;
            float xx2 = startwidth;
            float yy1 = -startwidth;
            float yy2 = -(rInstrument - 15);
            epd->fillTriangle(200+(int)(cosx*xx1-sinx*yy1),150+(int)(sinx*xx1+cosx*yy1),
                200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                200+(int)(cosx*0-sinx*yy2),150+(int)(sinx*0+cosx*yy2),commonData->fgcolor);
            // Inverted pointer
            // Pointer as triangle with center base 2*width
            float endwidth = 2;         // End width of pointer
            float ix1 = endwidth;
            float ix2 = -endwidth;
            float iy1 = -(rInstrument - 15);
            float iy2 = -endwidth;
            epd->fillTriangle(200+(int)(cosx*ix1-sinx*iy1),150+(int)(sinx*ix1+cosx*iy1),
                200+(int)(cosx*ix2-sinx*iy1),150+(int)(sinx*ix2+cosx*iy1),
                200+(int)(cosx*0-sinx*iy2),150+(int)(sinx*0+cosx*iy2),commonData->fgcolor);
        }

        // Center circle
        epd->fillCircle(200, 150, startwidth + 6, commonData->bgcolor);
        epd->fillCircle(200, 150, startwidth + 4, commonData->fgcolor);

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageClock(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageClock(
    "Clock",            // Page name
    createPage,         // Action
    0,                  // Number of bus values depends on selection in Web configuration
    {"GPST", "GPSD", "HDOP"},   // Bus values we need in the page
    true                // Show display header on/off
);

#endif
