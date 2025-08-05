// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "GwXDRMappings.h"

/*
  Fluid level view

    0: "Fuel",
    1: "Water",
    2: "Gray Water",
    3: "Live Well",
    4: "Oil",
    5: "Black Water",
    6: "Fuel Gasoline",
    14: "Error",
    15: "Unavailable"

TODO 
  - Check fluid type against connected XDR-value

*/

#define fuel_width 16
#define fuel_height 16
static unsigned char fuel_bits[] = {
   0xfc, 0x03, 0xfe, 0x67, 0x06, 0x86, 0x06, 0x86, 0x06, 0xe6, 0x06, 0xe6,
   0x06, 0xe6, 0x06, 0xe6, 0xfe, 0x47, 0xfe, 0x47, 0xfe, 0x4f, 0xfe, 0x57,
   0xfe, 0x57, 0xfe, 0x77, 0xfe, 0x07, 0xff, 0x0f };

#define water_width 16
#define water_height 16
static unsigned char water_bits[] = {
   0x00, 0x00, 0x80, 0x01, 0xfc, 0x3f, 0xfc, 0x3f, 0x80, 0x01, 0xc0, 0x03,
   0xc0, 0xc3, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xc7,
   0x87, 0x01, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00 };

#define waste_width 16
#define waste_height 16
static unsigned char waste_bits[] = {
   0xf0, 0x00, 0x9f, 0x00, 0x80, 0x07, 0x80, 0x08, 0x80, 0x10, 0x9f, 0x23,
   0xf0, 0x24, 0x00, 0x64, 0x10, 0x25, 0x40, 0xa4, 0x08, 0x66, 0xff, 0xff,
   0x00, 0x00, 0xbc, 0x6e, 0x00, 0x00, 0xf0, 0x19 };

#define oil_width 16
#define oil_height 16
static unsigned char oil_bits[] = {
   0x00, 0x01, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x07, 0xc0, 0x07, 0xe0, 0x0f,
   0xe0, 0x0f, 0xf0, 0x1f, 0xf0, 0x1f, 0xf8, 0x3f, 0xd8, 0x3f, 0xd8, 0x3f,
   0xb8, 0x3f, 0x70, 0x1e, 0xe0, 0x0f, 0xc0, 0x07 };

#define gasoline_width 16
#define gasoline_height 16
static unsigned char gasoline_bits[] = {
   0x0f, 0x00, 0xdf, 0x7f, 0xf8, 0xff, 0x70, 0xe0, 0xf8, 0xff, 0xf8, 0xff,
   0x98, 0xcf, 0x38, 0xe7, 0x78, 0xf0, 0xf8, 0xfa, 0xf8, 0xfa, 0x78, 0xf0,
   0x38, 0xe7, 0x98, 0xcf, 0xf8, 0xff, 0xf0, 0x7f };

#define fish_width 16
#define fish_height 16
static unsigned char fish_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x01, 0xf0, 0x03, 0xf8, 0x37,
   0xfc, 0x7f, 0xfc, 0x7f, 0xec, 0x3f, 0xfc, 0x7f, 0xfc, 0x7f, 0xf8, 0x37,
   0xf0, 0x03, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00 };

class PageFluid : public Page
{
    bool simulation = false;
    double simgoto;
    double simval;
    double simstep;
    bool holdvalues = false;
    int fluidtype;

    public:
    PageFluid(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageFluid");
        simulation = common.config->getBool(common.config->useSimuData);
        holdvalues = common.config->getBool(common.config->holdvalues);
        simval = double(random(0, 100));
        simgoto = double(random(0, 100));
        simstep = (simgoto - simval) / 20.0;
    }

    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    virtual void displayNew(PageData &pageData){
        fluidtype = commonData->config->getInt("page" + String(pageData.pageNumber) + "fluid", 0);
        commonData->logger->logDebug(GwLog::LOG,"New PageFluid: fluidtype=%d", fluidtype);
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        // Old values for hold function
        static double value1old;

        // Get config data
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false);
        }

        GwApi::BoatValue *bvalue1 = pageData.values[0];
        String name1 = bvalue1->getName();
        double fluidlevel = bvalue1->value;
        if (!simulation) {
            if (holdvalues and bvalue1->valid) {
                value1old = bvalue1->value;
            }
        } else {
            fluidlevel = simval;
            simval += simstep;
            if ((simgoto - simval) < 1.5 * simstep) {
                simgoto = double(random(0, 100));
                simstep = (simgoto - simval) / 20.0;
            }
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageFluid: value=%f", bvalue1->value);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height());

        epd->setTextColor(commonData->fgcolor);

        // descriptions
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(20, 60);
        epd->print("Fluid");

        epd->setCursor(300, 60);
        epd->print(xdrDelete(name1).substring(0, 6));

        // analog instrument
        // scale from -120 to 120

        // center
        Point c = {200, 150};
        uint8_t r = 110;

        // circular frame
        epd->drawCircle(c.x, c.y, r+5, commonData->fgcolor);
        epd->fillCircle(c.x, c.y, r+2, commonData->fgcolor);
        epd->fillCircle(c.x, c.y, r-1, commonData->bgcolor);
        // center of pointer as dot 
        epd->fillCircle(c.x, c.y, 8, commonData->fgcolor);

        // value down centered
        char buffer[6];
        if (bvalue1->valid or simulation) {
            snprintf(buffer, 6, "%3.0f%%", fluidlevel);
        } else {
            strcpy(buffer, "---");
        }
        drawTextCenter(c.x, c.y + r - 20, String(buffer));

        // draw symbol (as bitmap)
        switch (fluidtype) {
            case 0:
                epd->drawXBitmap(c.x-8, c.y-50, fuel_bits, fuel_width, fuel_height, commonData->fgcolor);
                break;
            case 1:
                epd->drawXBitmap(c.x-8, c.y-50, water_bits, water_width, water_height, commonData->fgcolor);
                break;
            case 2: // gray water no symbol yet
                // epd->drawXBitmap(c.x-8, c.y-50, gray_bits, gray_width, gray_height, commonData->fgcolor);
                break;
            case 3:
                epd->drawXBitmap(c.x-8, c.y-50, fish_bits, fish_width, fish_height, commonData->fgcolor);
                break;
            case 4:
                epd->drawXBitmap(c.x-8, c.y-50, oil_bits, oil_width, oil_height, commonData->fgcolor);
                break;
            case 5:
                epd->drawXBitmap(c.x-8, c.y-50, waste_bits, waste_width, waste_height, commonData->fgcolor);
                break;
            case 6:
                epd->drawXBitmap(c.x-8, c.y-50, gasoline_bits, gasoline_width, gasoline_height, commonData->fgcolor);
                break;
        }

        Point p, pr;

        // scale texts
        epd->setFont(&Ubuntu_Bold8pt8b);
        p = {c.x, c.y - r + 30};
        drawTextCenter(p.x, p.y, "1/2");
        pr = rotatePoint(c, p, -60);
        drawTextCenter(pr.x, pr.y, "1/4");
        pr = rotatePoint(c, p, 60);
        drawTextCenter(pr.x, pr.y, "3/4");

        // empty and full
        epd->setFont(&Ubuntu_Bold12pt8b);
        p = rotatePoint(c, {c.x, c.y - r + 30}, -130);
        drawTextCenter(p.x, p.y, "E");
        p = rotatePoint(c, {c.x, c.y - r + 30}, 130);
        drawTextCenter(p.x, p.y, "F");

        // lines
        std::vector<Point> pts = {
            {c.x - 2, c.y - r},
            {c.x + 2, c.y - r},
            {c.x + 2, c.y - (r - 16)},
            {c.x - 2, c.y - (r - 16)} 
        };
        fillPoly4(rotatePoints(c, pts, -120), commonData->fgcolor);
        fillPoly4(rotatePoints(c, pts, -60), commonData->fgcolor);
        fillPoly4(rotatePoints(c, pts, 0), commonData->fgcolor);
        fillPoly4(rotatePoints(c, pts, 60), commonData->fgcolor);
        fillPoly4(rotatePoints(c, pts, 120), commonData->fgcolor);

        // dots
        // rotate 0 to 360 in 12 degree steps
        for (int angle = -120; angle < 120; angle += 12) {
            if (angle == -120 or angle == -60 or angle == 0 or angle == 60) {
                continue;
            }
            p = rotatePoint(c, {c.x, c.y - r + 10}, angle);
            epd->fillCircle(p.x, p.y, 3, commonData->fgcolor);
        }

        // pointer
        if (bvalue1->valid or simulation) {
            pts = {
                {c.x - 1, c.y - (r - 20)},
                {c.x + 1, c.y - (r - 20)},
                {c.x + 6, c.y + 15},
                {c.x - 6, c.y + 15}
            };
            fillPoly4(rotatePoints(c, pts, -120 + fluidlevel * 2.4), commonData->fgcolor);
            // Pointer axis is white
            epd->fillCircle(c.x, c.y, 6, commonData->bgcolor);
        }
 
        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData &common){
    return new PageFluid(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageFluid(
    "Fluid",                // Page name
    createPage,             // Action
    1,                      // Number of bus values depends on selection in Web configuration
    true                    // Show display header on/off
);

#endif
