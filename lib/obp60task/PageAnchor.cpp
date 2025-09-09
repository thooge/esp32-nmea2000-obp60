// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "ConfigMenu.h"

/*
  Anchor overview with additional associated data
  This page is in experimental stage so be warned!
  North is up.

  Boatdata used
    DBS - Water depth
    HDT - Boat heading
    AWS - Wind strength; Boat not moving so we assume AWS=TWS and AWD=TWD
    AWD - Wind direction
    LAT/LON - Boat position, current
    HDOP - Position error

  This is the fist page to contain a configuration page with 
  data entry option.
  Also it will make use of the new alarm function.

  Data
    Anchor position lat/lon
    Depth at anchor position
    Chain length used
    Boat position current
    Depth at boat position
    Boat heading
    Wind direction
    Wind strength
    Alarm j/n
    Alarm radius
    GPS position error
    Timestamp while dropping anchor

   Drop / raise function in device OBP40 has to be done inside 
   config mode because of limited number of buttons.

   Save position in FRAM
   Alarm: gps fix lost
   switch unit feet/meter

*/

#define anchor_width 16
#define anchor_height 16
static unsigned char anchor_bits[] = {
   0x80, 0x01, 0x40, 0x02, 0x40, 0x02, 0x80, 0x01, 0xf0, 0x0f, 0x80, 0x01,
   0x80, 0x01, 0x88, 0x11, 0x8c, 0x31, 0x8e, 0x71, 0x84, 0x21, 0x86, 0x61,
   0x86, 0x61, 0xfc, 0x3f, 0xf8, 0x1f, 0x80, 0x01 };

class PageAnchor : public Page
{
private:
    String lengthformat;

    int scale = 50; // Radius of display circle in meter

    bool alarm = false;
    bool alarm_enabled = false;
    uint8_t alarm_range;

    uint8_t chain_length;
    uint8_t chain;

    bool anchor_set = false;
    double anchor_lat;
    double anchor_lon;
    double anchor_depth;
    int anchor_ts; // time stamp anchor dropped

    char mode = 'N'; // (N)ormal, (C)onfig
    int8_t editmode = -1; // marker for menu/edit/set function

    ConfigMenu *menu;

    void displayModeNormal(PageData &pageData) {

        // Boatvalues: DBS, HDT, AWS, AWD, LAT, LON, HDOP
        GwApi::BoatValue *bv_dbs = pageData.values[0]; // DBS
        String sval_dbs = commonData->fmt->formatValue(bv_dbs, *commonData).svalue;
        String sunit_dbs = commonData->fmt->formatValue(bv_dbs, *commonData).unit; 
        GwApi::BoatValue *bv_hdt = pageData.values[1]; // HDT
        String sval_hdt = commonData->fmt->formatValue(bv_hdt, *commonData).svalue;
        GwApi::BoatValue *bv_aws = pageData.values[2]; // AWS
        String sval_aws = commonData->fmt->formatValue(bv_aws, *commonData).svalue;
        String sunit_aws = commonData->fmt->formatValue(bv_aws, *commonData).unit; 
        GwApi::BoatValue *bv_awd = pageData.values[3]; // AWD
        String sval_awd = commonData->fmt->formatValue(bv_awd, *commonData).svalue;
        GwApi::BoatValue *bv_lat = pageData.values[4]; // LAT
        String sval_lat = commonData->fmt->formatValue(bv_lat, *commonData).svalue;
        GwApi::BoatValue *bv_lon = pageData.values[5]; // LON
        String sval_lon = commonData->fmt->formatValue(bv_lon, *commonData).svalue;
        GwApi::BoatValue *bv_hdop = pageData.values[6]; // HDOP
        String sval_hdop = commonData->fmt->formatValue(bv_hdop, *commonData).svalue;
        String sunit_hdop = commonData->fmt->formatValue(bv_hdop, *commonData).unit; 

        logger->logDebug(GwLog::DEBUG, "Drawing at PageAnchor; DBS=%f, HDT=%f, AWS=%f", bv_dbs->value, bv_hdt->value, bv_aws->value);

        Point c = {200, 150}; // center = anchor position
        uint16_t r = 125;

        Point b = {200, 180}; // boat position while dropping anchor

        const std::vector<Point> pts_boat = { // polygon lines
            {b.x - 5, b.y},
            {b.x - 5, b.y - 10},
            {b.x, b.y - 16},
            {b.x + 5, b.y - 10},
            {b.x + 5, b.y}
        };
        //rotatePoints und dann Linien zeichnen
        // TODO rotate boat according to current heading
        //drawPoly(rotatePoints(c, pts, RadToDeg(value2)), commonData->fgcolor);
        drawPoly(pts_boat, commonData->fgcolor);

        // Draw wind arrow
        const std::vector<Point> pts_wind = {
            {c.x, c.y - r + 25},
            {c.x - 12, c.y - r - 4},
            {c.x, c.y - r + 6},
            {c.x + 12, c.y - r - 4}
        };
        if (bv_awd->valid) {
            fillPoly4(rotatePoints(c, pts_wind, bv_awd->value), commonData->fgcolor);
        }

        // Title and corner value headings
        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("Anchor");

        epd->setFont(&Ubuntu_Bold10pt8b);
        epd->setCursor(8, 200);
        epd->print("Depth");
        drawTextRalign(392, 38, "Chain");
        drawTextRalign(392, 200, "Wind");

        // Units
        epd->setCursor(8, 272);
        epd->print(sunit_dbs);
        drawTextRalign(392, 272, sunit_aws);
        drawTextRalign(392, 100, lengthformat); // chain unit not implemented

        // Corner values
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(8, 70);
        epd->print("Alarm: ");
        epd->print(alarm_enabled ? "On" : "Off");

        epd->setCursor(8, 90);
        epd->print("HDOP");
        epd->setCursor(8, 106);
        if (bv_hdop->valid) {
            epd->print(round(bv_hdop->value), 0);
            epd->print(sunit_hdop);
        } else {
            epd->print("n/a");
        }

        // Values
        epd->setFont(&DSEG7Classic_BoldItalic20pt7b);
        // Current chain used
        epd->setCursor(328, 85);
        epd->print("27");

        // Depth
        epd->setCursor(8, 250);
        epd->print(sval_dbs);
        // Wind
        epd->setCursor(328, 250);
        epd->print(sval_aws);

        epd->drawCircle(c.x, c.y, r, commonData->fgcolor);
        epd->drawCircle(c.x, c.y, r + 1, commonData->fgcolor);
 
        // zoom scale
        epd->drawLine(c.x + 10, c.y, c.x + r - 4, c.y, commonData->fgcolor);
        // arrow left
        epd->drawLine(c.x + 10, c.y, c.x + 16, c.y - 4, commonData->fgcolor);
        epd->drawLine(c.x + 10, c.y, c.x + 16, c.y + 4, commonData->fgcolor);
        // arrow right
        epd->drawLine(c.x + r - 4, c.y, c.x + r - 10, c.y - 4, commonData->fgcolor);
        epd->drawLine(c.x + r - 4, c.y, c.x + r - 10, c.y + 4, commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold8pt8b);
        drawTextCenter(c.x + r / 2, c.y + 8, String(scale) + "m");
 
        // alarm range circle
        if (alarm_enabled) {
            // alarm range in meter has to be smaller than the scale in meter
            // r and r_range are pixel values
            uint16_t r_range = int(alarm_range * r / scale);
            logger->logDebug(GwLog::LOG, "Drawing at PageAnchor; Alarm range = %d", r_range);
            epd->drawCircle(c.x, c.y, r_range, commonData->fgcolor);
        }
 
        // draw anchor symbol (as bitmap)
        epd->drawXBitmap(c.x - anchor_width / 2, c.y - anchor_height / 2,
                                 anchor_bits, anchor_width, anchor_height, commonData->fgcolor);

    }

    void displayModeConfig() {

        epd->setTextColor(commonData->fgcolor);
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("Anchor configuration");
        
        // TODO
        // show lat/lon for anchor pos
        // show lat/lon for boat pos
        // show distance anchor <-> boat

        epd->setFont(&Ubuntu_Bold8pt8b);
        for (int i = 0 ; i < menu->getItemCount(); i++) {
            ConfigMenuItem *itm = menu->getItemByIndex(i);
            if (!itm) {
                logger->logDebug(GwLog::ERROR, "Menu item not found: %d", i);
            } else {
                Rect r = menu->getItemRect(i);
                bool inverted = (i == menu->getActiveIndex());
                drawTextBoxed(r, itm->getLabel(), commonData->fgcolor, commonData->bgcolor, inverted, false);
                if (inverted and editmode > 0) {
                    // triangle as edit marker
                    epd->fillTriangle(r.x + r.w + 20, r.y, r.x + r.w + 30, r.y + r.h / 2, r.x + r.w + 20, r.y + r.h,  commonData->fgcolor);
                }
                epd->setCursor(r.x + r.w + 40, r.y + r.h - 4);
                if (itm->getType() == "int") {
                    epd->print(itm->getValue());
                    epd->print(itm->getUnit());
                } else {
                     epd->print(itm->getValue() == 0 ? "No" : "Yes");
                }
            }
        } 

    }

public:
    PageAnchor(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageAnchor");

        // preload configuration data
        lengthformat = config->getString(config->lengthFormat);
        chain_length = config->getInt(config->chainLength);

        chain = 0;
        anchor_set = false;
        alarm_range = 30;

        // Initialize config menu
        menu = new ConfigMenu("Options", 40, 80);
        menu->setItemDimension(150, 20);

        ConfigMenuItem *newitem;
        newitem = menu->addItem("chain", "Chain out", "int", 0, "m");
        if (! newitem) {
            // Demo: in case of failure exit here, should never be happen
            logger->logDebug(GwLog::ERROR, "Menu item creation failed");
            return;
        }
        newitem->setRange(0, 200, {1, 5, 10});
        newitem = menu->addItem("chainmax", "Chain max", "int", chain_length, "m");
        newitem->setRange(0, 200, {1, 5, 10});
        newitem = menu->addItem("zoom", "Zoom", "int", 50, "m");
        newitem->setRange(0, 200, {1, });
        newitem = menu->addItem("range", "Alarm range", "int", 40, "m");
        newitem->setRange(0, 200, {1, 5, 10});
        newitem = menu->addItem("alat", "Adjust anchor lat.", "int", 0, "m");
        newitem->setRange(0, 200, {1, 5, 10});
        newitem = menu->addItem("alon", "Adjust anchor lon.", "int", 0, "m");
        newitem->setRange(0, 200, {1, 5, 10});
#ifdef BOARD_OBP40S3
        // Intodruced here because of missing keys for OBP40
        newitem = menu->addItem("anchor", "Anchor down", "bool", 0, "");
#endif
        menu->setItemActive("zoom");
     }

    void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
        commonData->keydata[1].label = "ALARM";
    }

#ifdef BOARD_OBP60S3
    int handleKey(int key){
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
            } else {
                mode = 'N';
            }
            return 0;
        }
        if (mode == 'N') {
            if (key == 2) { // Toggle alarm
                alarm_enabled = !alarm_enabled;
                return 0;
            }
        } else { // Config mode
            if (key == 3) {
                // menu down
                menu->goNext();
                return 0;
            }
            if (key == 4) {
                // menu up
                menu->goPrev();
                return 0;
            }
        }
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }
#endif
#ifdef BOARD_OBP40S3
    int handleKey(int key){
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
                commonData->keydata[1].label = "EDIT";
            } else {
                mode = 'N';
                commonData->keydata[1].label = "ALARM";
            }
            return 0;
        }
        if (mode == 'N') {
            if (key == 2) { // Toggle alarm
                alarm_enabled = !alarm_enabled;
                return 0;
            }
        } else { // Config mode
            // TODO different code for OBP40 / OBP60 
            if (key == 9) {
                // menu down
                if (editmode > 0) {
                    // decrease item value
                    menu->getActiveItem()->decValue();
                } else {
                    menu->goNext();
                }
                return 0;
            }
            if (key == 10) {
                // menu up or value up
                if (editmode > 0) {
                    // increase item value
                    menu->getActiveItem()->incValue();
                } else {
                    menu->goPrev();
                }
                return 0;
            }
            if (key == 2) {
                // enter / leave edit mode for current menu item
                if (editmode > 0) {
                    commonData->keydata[1].label = "EDIT";
                    editmode = 0;
                } else {
                    commonData->keydata[1].label = "SET";
                    editmode = 1;
                }
                return 0;
            }
        }
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }
#endif

    void displayNew(PageData &pageData){
 #ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
   };

    int displayPage(PageData &pageData){

        // Logging boat values
        logger->logDebug(GwLog::LOG, "Drawing at PageAnchor; Mode=%c", mode);

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

       if (mode == 'N') {
           displayModeNormal(pageData);
       } else if (mode == 'C') {
           displayModeConfig();
       }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageAnchor(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageAnchor(
    "Anchor",   // Page name
    createPage, // Action
    0,          // Number of bus values depends on selection in Web configuration
    {"DBS", "HDT", "AWS", "AWD", "LAT", "LON", "HDOP"}, // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true        // Show display header on/off
);

#endif
