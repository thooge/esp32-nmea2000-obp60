// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

/*
  This page is in experimental stage so be warned!
  North is up.
  Anchor page with background map from mapservice

  Boatdata used
    DBS - Water depth
    HDT - Boat heading
    AWS - Wind strength; Boat not moving so we assume AWS=TWS and AWD=TWD
    AWD - Wind direction
    LAT/LON - Boat position, current
    HDOP - Position error

   Drop / raise function in device OBP40 has to be done inside 
   config mode because of limited number of buttons.

  TODO 
    gzip for data transfer,
       manually inflating with tinflate from ROM
    Save position in FRAM
    Alarm: gps fix lost
    switch unit feet/meter
    force map update if new position is different from old position by 
    a certain level (e.g. 10m)

    Map service options / URL parameters
      - mandatory
          lat: latitude
          lon: longitude
          width: image width in px
          height: image height in px
      - optional
          zoom: zoom level, default 15
          mrot: map rotation angle in degrees
          mtype: map type, default="Open Street Map"
          dtype: dithering type, default="Atkinson"
          cutout: image cutout type 0=none
          tab: tab size, 0=none
          border: border line zize in px, default 2
          symbol: synmol number, default=2 triangle
          srot: symbol rotation in degrees
          ssize: symbol size in px, default=15
          grid: show map grid

*/

#include <WiFi.h>
#include <HTTPClient.h>
#include "Pagedata.h"
#include "OBP60Extensions.h"

#define anchor_width 16
#define anchor_height 16
static unsigned char anchor_bits[] PROGMEM = {
   0x80, 0x01, 0x40, 0x02, 0x40, 0x02, 0x80, 0x01, 0xf0, 0x0f, 0x80, 0x01,
   0x80, 0x01, 0x88, 0x11, 0x8c, 0x31, 0x8e, 0x71, 0x84, 0x21, 0x86, 0x61,
   0x86, 0x61, 0xfc, 0x3f, 0xf8, 0x1f, 0x80, 0x01 };

class PageAnchor : public Page
{
private:
    char mode = 'N'; // (N)ormal, (C)onfig
    int8_t editmode = -1; // marker for menu/edit/set function

    //uint8_t *mapbuf = new uint8_t[10000]; // 8450 Byte without header
    //int mapbuf_size = 10000;
    //uint8_t *mapbuf = (uint8_t*) heap_caps_malloc(mapbuf_size, MALLOC_CAP_SPIRAM);
    GFXcanvas1 *canvas;
    const uint16_t map_width = 264;
    const uint16_t map_height = 260;
    bool map_valid = false;
    char map_service = 'R'; // (O)BP Service, (L)ocal Service, (R)emote Service
    double map_lat = 0; // current center of valid map
    double map_lon = 0;
    String server_name; // server with map service
    uint16_t server_port = 80;
    String tile_path;

    String lengthformat;

    double scale = 50;    // Radius of display circle in meter, depends on lat
    uint8_t zoom = 15; // map zoom level

    bool alarm = false;
    bool alarm_enabled = false;
    uint8_t alarm_range;

    uint8_t chain_length;
    uint8_t chain = 0;

    bool anchor_set = false;
    double anchor_lat;
    double anchor_lon;
    double anchor_depth;
    int anchor_ts; // time stamp anchor dropped

    void displayModeNormal(PageData &pageData) {

        // Boatvalues: DBS, HDT, AWS, AWD, LAT, LON, HDOP
        GwApi::BoatValue *bv_dbs = pageData.values[0]; // DBS
        String sval_dbs = formatValue(bv_dbs, *commonData).svalue;
        String sunit_dbs = formatValue(bv_dbs, *commonData).unit; 
        GwApi::BoatValue *bv_hdt = pageData.values[1]; // HDT
        String sval_hdt = formatValue(bv_hdt, *commonData).svalue;
        GwApi::BoatValue *bv_aws = pageData.values[2]; // AWS
        String sval_aws = formatValue(bv_aws, *commonData).svalue;
        String sunit_aws = formatValue(bv_aws, *commonData).unit; 
        GwApi::BoatValue *bv_awd = pageData.values[3]; // AWD
        String sval_awd = formatValue(bv_awd, *commonData).svalue;
        GwApi::BoatValue *bv_lat = pageData.values[4]; // LAT
        String sval_lat = formatValue(bv_lat, *commonData).svalue;
        GwApi::BoatValue *bv_lon = pageData.values[5]; // LON
        String sval_lon = formatValue(bv_lon, *commonData).svalue;
        GwApi::BoatValue *bv_hdop = pageData.values[6]; // HDOP
        String sval_hdop = formatValue(bv_hdop, *commonData).svalue;
        String sunit_hdop = formatValue(bv_hdop, *commonData).unit; 

        commonData->logger->logDebug(GwLog::DEBUG, "Drawing at PageAnchor; DBS=%f, HDT=%f, AWS=%f", bv_dbs->value, bv_hdt->value, bv_aws->value);

        // Draw canvas with background map
        // rhumb(map_lat, map_lon, bv_lat->value, bv_lon->value)
        int posdiff = 0;
        if (map_valid) {
            if (bv_lat->valid and bv_lon->valid) {
                // calculate movement since last map refresh
                posdiff = rhumb(map_lat, map_lon, bv_lat->value, bv_lon->value);
                if (posdiff > 25) {
                    map_lat = bv_lat->value;
                    map_lon = bv_lon->value;
                    map_valid = getBackgroundMap(map_lat, map_lon, zoom);
                    if (map_valid) {
                        // prepare visible space for anchor-symbol or boat
                        canvas->fillCircle(132, 130, 12, commonData->fgcolor);
                    }
                }
            }
            getdisplay().drawBitmap(68, 20, canvas->getBuffer(), map_width, map_height, commonData->fgcolor);
        }

        Point c = {200, 150}; // center = anchor position
        uint16_t r = 125;

        // Circle as map border
        getdisplay().drawCircle(c.x, c.y, r, commonData->fgcolor);
        getdisplay().drawCircle(c.x, c.y, r + 1, commonData->fgcolor);

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
        if (bv_hdt->valid) {
            if (map_valid) {
                Point b1 = rotatePoint(c, {b.x, b.y - 8}, bv_hdt->value * RAD_TO_DEG);
                getdisplay().fillCircle(b1.x, b1.y, 10, commonData->bgcolor);
            }
            drawPoly(rotatePoints(c, pts_boat, bv_hdt->value * RAD_TO_DEG), commonData->fgcolor);
        } else {
            // no heading available draw north oriented
            if (map_valid) {
                getdisplay().fillCircle(b.x, b.y - 8, 10, commonData->bgcolor);
            }
            drawPoly(pts_boat, commonData->fgcolor);
        }

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
        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(&Ubuntu_Bold10pt8b);
        // Left
        getdisplay().setCursor(8, 36);
        getdisplay().print("Anchor");
        getdisplay().setCursor(8, 210);
        getdisplay().print("Depth");
        // Right
        drawTextRalign(392, 80, "Chain");
        drawTextRalign(392, 210, "Wind");

        // Units
        getdisplay().setCursor(8, 272);
        getdisplay().print(sunit_dbs);
        drawTextRalign(392, 272, sunit_aws);
        // drawTextRalign(392, 100, lengthformat); // chain unit not implemented

        // Corner values
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(8, 54);
        getdisplay().print(anchor_set ? "Dropped" : "Ready"); // Anchor state
        getdisplay().setCursor(8, 72);
        getdisplay().print("Alarm: "); // Alarm state
        getdisplay().print(alarm_enabled ? "on" : "off");

        getdisplay().setCursor(8, 120);
        getdisplay().print("Zoom");
        getdisplay().setCursor(8, 136);
        getdisplay().print(zoom);

        getdisplay().setCursor(8, 160);
        getdisplay().print("diff");
        getdisplay().setCursor(8, 176);
        if (map_valid and bv_lat->valid and bv_lon->valid) {
            getdisplay().print(String(posdiff));
        } else {
            getdisplay().print("n/a");
        }

        // Chain out TODO lengthformat ft/m
        drawTextRalign(392, 96, String(chain) + " m");
        drawTextRalign(392, 96+16, "of " + String(chain_length) + " m");

        getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);

        // Depth
        getdisplay().setCursor(8, 250);
        getdisplay().print(sval_dbs);

        // Wind
        getdisplay().setCursor(320, 250);
        getdisplay().print(sval_aws);

        // Position of boat in center of map
        getdisplay().setFont(&IBM8x8px);
        drawTextRalign(392, 34, sval_lat);
        drawTextRalign(392, 44, sval_lon);
        // quality
        String hdop = "HDOP: ";
        if (bv_hdop->valid) {
            hdop += String(round(bv_hdop->value));
        } else {
            hdop += " n/a";
        }
        drawTextRalign(392, 54, hdop);

        // zoom scale
        getdisplay().drawLine(c.x + 10, c.y, c.x + r - 4, c.y, commonData->fgcolor);
        // arrow left
        getdisplay().drawLine(c.x + 10, c.y, c.x + 16, c.y - 4, commonData->fgcolor);
        getdisplay().drawLine(c.x + 10, c.y, c.x + 16, c.y + 4, commonData->fgcolor);
        // arrow right
        getdisplay().drawLine(c.x + r - 4, c.y, c.x + r - 10, c.y - 4, commonData->fgcolor);
        getdisplay().drawLine(c.x + r - 4, c.y, c.x + r - 10, c.y + 4, commonData->fgcolor);
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        drawTextCenter(c.x + r / 2, c.y + 8, String(scale, 0) + "m");

        // draw anchor symbol (as bitmap)
        getdisplay().drawXBitmap(c.x - anchor_width / 2, c.y - anchor_height / 2,
            anchor_bits, anchor_width, anchor_height, commonData->fgcolor);

    }

    void displayModeConfig(PageData &pageData) {

        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 48);
        getdisplay().print("Anchor configuration");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        getdisplay().setCursor(8, 250);
        getdisplay().print("Press MODE to leave config");

        getdisplay().setCursor(8, 68);
        getdisplay().printf("Server: %s", server_name.c_str());
        getdisplay().setCursor(8, 88);
        getdisplay().printf("Port: %d", server_port);
        getdisplay().setCursor(8, 108);
        getdisplay().printf("Tilepath: %s", tile_path.c_str());

        GwApi::BoatValue *bv_lat = pageData.values[4]; // LAT
        GwApi::BoatValue *bv_lon = pageData.values[5]; // LON
        if (!bv_lat->valid or !bv_lon->valid) {
            getdisplay().setCursor(8, 128);
            getdisplay().printf("No valid position: background map disabled");
        }

    }

public:
    PageAnchor(CommonData &common) 
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageAnchor");

        String mapsource = common.config->getString(common.config->mapsource);
        if (mapsource == "Local Service") {
            map_service = 'L';
            server_name = common.config->getString(common.config->ipAddress);
            server_port = common.config->getInt(common.config->localPort);
            tile_path = "";
        } else if (mapsource == "Remote Service") {
            map_service = 'R';
            server_name = common.config->getString(common.config->mapServer);
            tile_path = common.config->getString(common.config->mapTilePath);
        } else { // OBP Service or undefined
            map_service = 'O';
            server_name = "norbert-walter.dnshome.de";
            tile_path = "";
        }
        zoom = common.config->getInt(common.config->zoomlevel);

        lengthformat = common.config->getString(common.config->lengthFormat);
        chain_length = common.config->getInt(common.config->chainLength);

        canvas = new GFXcanvas1(264, 260); // Byte aligned, no padding!
    }

    void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "MODE";
#ifdef BOARD_OBP40S3
        commonData->keydata[1].label = "DROP";
#endif
#ifdef BOARD_OBP60S3
        commonData->keydata[4].label = "DROP";
#endif
    }

    // TODO OBP40 / OBP60 different handling
    int handleKey(int key) {
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
                commonData->keydata[1].label = "EDIT";
            } else {
                mode = 'N';
#ifdef BOARD_OBP40S3
                commonData->keydata[1].label = anchor_set ? "RAISE": "DROP";
#endif
#ifdef BOARD_OBP60S3
                commonData->keydata[4].label = anchor_set ? "RAISE": "DROP";
#endif
            }
            return 0;
        }
        if (key == 2) {
            anchor_set = !anchor_set;
            commonData->keydata[1].label = anchor_set ? "RAISE": "DROP";
            return 0;
        }
        // Code for keylock
        if (key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }

    int rhumb(double lat1, double lon1, double lat2, double lon2) {
        // calc distance in m between two geo points
        static const double degToRad = M_PI / 180.0;
        lat1 = degToRad * lat1;
        lon1 = degToRad * lon1;
        lat2 = degToRad * lat2;
        lon2 = degToRad * lon2;
        double dlon = lon2 - lon1;
        double dlat = lat2 - lat1;
        double mlat = (lat1 + lat2) / 2;
        return (int) (6371000 * sqrt(pow(dlat, 2) + pow(cos(mlat) * dlon, 2)));
    }

    bool getBackgroundMap(double lat, double lon, uint8_t zoom) {
        // HTTP-Request for map
        // TODO über pagedata -> status abfragen?
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }
        bool valid = false;
        HTTPClient http;
        String url = "http://" + server_name + "/" + tile_path;
        String parameter = "?lat=" + String(lat, 6) + "&lon=" + String(lon, 6)+ "&zoom=" + String(zoom)
                         + "&width=" + String(map_width) + "&height=" + String(map_height);
        commonData->logger->logDebug(GwLog::LOG, "HTTP query: %s", String(url + parameter).c_str());
        http.begin(url + parameter);
        // http.SetAcceptEncoding("gzip");
        // TODO miniz.c from ROM
        int httpCode = http.GET();
        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                WiFiClient* stream = http.getStreamPtr();
                int size = http.getSize();
                commonData->logger->logDebug(GwLog::LOG, "HTTP get size: %d", size);
                // header: P4<LF><width> <height><LF> (e.g. 11 byte)
                uint8_t header[14]; // max: P4<LF>wwww wwww<LF>
                bool header_read = false;
                int header_size = 0;
                uint8_t* buf = canvas->getBuffer();
                int n = 0;
                int ix = 0;
                while (stream->available()) {
                    uint8_t b = stream->read();
                    n += 1;
                    if ((! header_read) and (n < 13) ) {
                        header[n-1] = b;
                        if ((n > 3) and (b == 0x0a)) {
                            header_read = true;
                            header_size = n;
                            header[n] = 0;
                        }
                    } else {
                        // write image data to canvas buffer
                        buf[ix++] = b;
                    }
                }
                if (n == size) {
                    valid = true;
                }
                commonData->logger->logDebug(GwLog::LOG, "HTTP: final bytesRead=%d, header-size=%d", n, header_size);
            } else {
                commonData->logger->logDebug(GwLog::LOG, "HTTP result #%d", httpCode);
            }
        } else {
            commonData->logger->logDebug(GwLog::ERROR, "HTTP error #%d", httpCode);
        }
        http.end();
        return valid;
    }

    void displayNew(PageData &pageData){

        GwApi::BoatValue *bv_lat = pageData.values[4]; // LAT
        GwApi::BoatValue *bv_lon = pageData.values[5]; // LON

        // check if valid data available
        if (!bv_lat->valid or !bv_lon->valid) {
            map_valid = false;
            return;
        }

        map_lat = bv_lat->value; // save for later comparison
        map_lon = bv_lon->value;
        map_valid = getBackgroundMap(map_lat, map_lon, zoom);

        if (map_valid) {
            // prepare visible space for anchor-symbol or boat
            canvas->fillCircle(132, 130, 10, commonData->fgcolor);
        }
    };

    int displayPage(PageData &pageData) {
        GwLog *logger = commonData->logger;

        // Logging boat values
        logger->logDebug(GwLog::LOG, "Drawing at PageAnchor; Mode=%c", mode);

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

       if (mode == 'N') {
           displayModeNormal(pageData);
       } else if (mode == 'C') {
           displayModeConfig(pageData);
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
