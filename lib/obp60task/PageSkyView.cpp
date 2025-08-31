// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

#include <vector>
#include <algorithm> // for vector sorting

/*
 * SkyView / Satellites
 */

class PageSkyView : public Page
{
private:
    GwBoatData *bd;

public:
    PageSkyView(CommonData &common) : Page(common)
    {
        // task name access is for example purpose only
        TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
        const char* taskName = pcTaskGetName(currentTaskHandle);
        logger->logDebug(GwLog::LOG, "Instantiate PageSkyView in task '%s'", taskName);
    }

    int handleKey(int key) {
        // return 0 to mark the key handled completely
        // return the key to allow further action
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0;
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
        bd = pageData.api->getBoatData();
    };

    // Comparator function to sort by SNR
    static bool compareBySNR(const GwSatInfo& a, const GwSatInfo& b) {
       return a.SNR > b.SNR; // Sort in descending order
    }

    int displayPage(PageData &pageData) {

        std::vector<GwSatInfo> sats;
        int nSat = bd->SatInfo->getNumSats();

        logger->logDebug(GwLog::LOG, "Drawing at PageSkyView, %d satellites", nSat);

        for (int i = 0; i < nSat; i++) {
            sats.push_back(*bd->SatInfo->getAt(i));
        }
        std::sort(sats.begin(), sats.end(), compareBySNR);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update

        // current position
        epd->setFont(&Ubuntu_Bold8pt8b);


        GwApi::BoatValue *bv_lat = pageData.values[0];
        String sv_lat = commonData->fmt->formatValue(bv_lat, *commonData).svalue;
        //epd->setCursor(300, 40);
        //epd->print(sv_lat);

        GwApi::BoatValue *bv_lon = pageData.values[1];
        String sv_lon = commonData->fmt->formatValue(bv_lon, *commonData).svalue;
        //epd->setCursor(300, 60);
        //epd->print(sv_lon);

        GwApi::BoatValue *bv_hdop = pageData.values[2];
        String sv_hdop = commonData->fmt->formatValue(bv_hdop, *commonData).svalue;
        //epd->setCursor(300, 80);
        //epd->print(sv_hdop);

        // sky view
        Point c = {130, 148};
        uint16_t r = 125;
        uint16_t r1 = r / 2;

        epd->fillCircle(c.x, c.y, r, commonData->bgcolor);
        epd->drawCircle(c.x, c.y, r + 1, commonData->fgcolor);
        epd->drawCircle(c.x, c.y, r + 2, commonData->fgcolor);
        epd->drawCircle(c.x, c.y, r1, commonData->fgcolor);

        // separation lines
        epd->drawLine(c.x - r, c.y, c.x + r, c.y, commonData->fgcolor);
        epd->drawLine(c.x, c.y - r, c.x, c.y + r, commonData->fgcolor);
        Point p = {c.x, c.y - r};
        Point p1, p2;
        p1 = rotatePoint(c, p, 45);
        p2 = rotatePoint(c, p, 45 + 180);
        epd->drawLine(p1.x, p1.y, p2.x, p2.y,  commonData->fgcolor);
        p1 = rotatePoint(c, p, -45);
        p2 = rotatePoint(c, p, -45 + 180);
        epd->drawLine(p1.x, p1.y, p2.x, p2.y, commonData->fgcolor);

        // directions 

        int16_t x1, y1;
        uint16_t w, h;
        epd->setFont(&Ubuntu_Bold12pt8b);

		epd->getTextBounds("N", 0, 150, &x1, &y1, &w, &h);
        epd->setCursor(c.x - w / 2, c.y - r + h + 2);
        epd->print("N");

		epd->getTextBounds("S", 0, 150, &x1, &y1, &w, &h);
        epd->setCursor(c.x - w / 2, c.y + r - 2);
        epd->print("S");

		epd->getTextBounds("E", 0, 150, &x1, &y1, &w, &h);
        epd->setCursor(c.x + r - w - 2, c.y + h / 2);
        epd->print("E");
        
		epd->getTextBounds("W", 0, 150, &x1, &y1, &w, &h);
        epd->setCursor(c.x - r + 2 , c.y + h / 2);
        epd->print("W");

        // show satellites in "map"
        epd->setFont(&Atari6px);
        for (int i = 0; i < nSat; i++) {
            float arad = sats[i].Azimut * M_PI / 180.0;
            float erad = sats[i].Elevation * M_PI / 180.0;
            uint16_t x = c.x + sin(arad) * erad * r;
            uint16_t y = c.y + cos(arad) * erad * r;
            epd->drawRect(x-4, y-4, 8, 8, commonData->fgcolor);
            // Add Sat number
            epd->setCursor(x+5, y);
            char buffer[3];
            snprintf(buffer, 3, "%02d", static_cast<int>(sats[i].PRN));
            epd->print(String(buffer));
        }

        // Signal / Noise bars
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(325, 34);
        epd->print("SNR");
        epd->drawRect(270, 20, 125, 257, commonData->fgcolor);
        int maxsat = std::min(nSat, 12);
        for (int i = 0; i < maxsat; i++) {
            uint16_t y = 29 + (i + 1) * 20;
            epd->setCursor(276, y);
            char buffer[3];
            snprintf(buffer, 3, "%02d", static_cast<int>(sats[i].PRN));
            epd->print(String(buffer));
            epd->drawRect(305, y-12, 85, 14, commonData->fgcolor);
            epd->setCursor(315, y);
            // TODO SNR as number or as bar via mode key?
            if (sats[i].SNR <= 100) {
                // epd->print(sats[i].SNR);
                epd->fillRect(307, y-10, int(81 * sats[i].SNR / 100.0), 10, commonData->fgcolor);
            } else {
                epd->print("n/a");
            }
        }

        return PAGE_UPDATE;
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
