#ifdef BOARD_OBP60S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

/*
 * SkyView / Satellites
 */

class PageSkyView : public Page
{
public:
    PageSkyView(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Show PageSkyView");
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
        String flashLED = config->getString(config->flashLED);
        String displaycolor = config->getString(config->displaycolor);
        String backlightMode = config->getString(config->backlight);
        
        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageSkyView");

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        // current position
        getdisplay().setFont(&Ubuntu_Bold8pt7b);

        GwApi::BoatValue *bv_lat = pageData.values[0];
        String sv_lat = formatValue(bv_lat, *commonData).svalue;
        //getdisplay().setCursor(300, 40);
        //getdisplay().print(sv_lat);

        GwApi::BoatValue *bv_lon = pageData.values[1];
        String sv_lon = formatValue(bv_lon, *commonData).svalue;
        //getdisplay().setCursor(300, 60);
        //getdisplay().print(sv_lon);

        GwApi::BoatValue *bv_hdop = pageData.values[2];
        String sv_hdop = formatValue(bv_hdop, *commonData).svalue;
        //getdisplay().setCursor(300, 80);
        //getdisplay().print(sv_hdop);

        // sky view
        Point c = {130, 148};
        uint16_t r = 125;
        uint16_t r1 = r / 2;

        getdisplay().fillCircle(c.x, c.y, r, commonData->bgcolor);
        getdisplay().drawCircle(c.x, c.y, r + 1, commonData->fgcolor);
        getdisplay().drawCircle(c.x, c.y, r + 2, commonData->fgcolor);
        getdisplay().drawCircle(c.x, c.y, r1, commonData->fgcolor);

        // separation lines
        getdisplay().drawLine(c.x - r, c.y, c.x + r, c.y, commonData->fgcolor);
        getdisplay().drawLine(c.x, c.y - r, c.x, c.y + r, commonData->fgcolor);
        Point p = {c.x, c.y - r};
        Point p1, p2;
        p1 = rotatePoint(c, p, 45);
        p2 = rotatePoint(c, p, 45 + 180);
        getdisplay().drawLine(p1.x, p1.y, p2.x, p2.y,  commonData->fgcolor);
        p1 = rotatePoint(c, p, -45);
        p2 = rotatePoint(c, p, -45 + 180);
        getdisplay().drawLine(p1.x, p1.y, p2.x, p2.y, commonData->fgcolor);

        // directions 

        int16_t  x1, y1;
        uint16_t w, h;
        getdisplay().setFont(&Ubuntu_Bold12pt7b);

		getdisplay().getTextBounds("N", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x - w / 2, c.y - r + h + 2);
        getdisplay().print("N");

		getdisplay().getTextBounds("S", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x - w / 2, c.y + r - 2);
        getdisplay().print("S");

		getdisplay().getTextBounds("E", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x + r - w - 2, c.y + h / 2);
        getdisplay().print("E");
        
		getdisplay().getTextBounds("W", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x - r + 2 , c.y + h / 2);
        getdisplay().print("W");

        // satellites


        // Signal / Noise bars
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        getdisplay().setCursor(325, 34);
        getdisplay().print("SNR");
        getdisplay().drawRect(270, 20, 125, 257, commonData->fgcolor);
        for (int i = 0; i < 12; i++) {
            uint16_t y = 29 + (i + 1) * 20;
            getdisplay().setCursor(276, y);
            char buffer[3];
            snprintf(buffer, 3, "%02d", i+1);
            getdisplay().print(String(buffer));
            getdisplay().drawRect(305, y-12, 85, 14, commonData->fgcolor);
        }

        // Update display
        getdisplay().nextPage();    // Partial update (fast)

    };
};

static Page* createPage(CommonData &common){
    return new PageSkyView(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageSkyView(
    "SkyView",              // Page name
    createPage,             // Action
    0,                      // Number of bus values depends on selection in Web configuration
    {"LAT", "LON", "HDOP"}, // Bus values we need in the page
    true                    // Show display header on/off
);

#endif
