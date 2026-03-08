// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

/*
  This page is in experimental stage so be warned!
  North is up.
  Anchor page with background map from mapservice

  Boatdata used
    DBS - Water depth
    HDT - Boat heading, true
    AWS - Wind strength; Boat not moving so we assume AWS=TWS and AWD=TWD
    AWD - Wind direction
    LAT/LON - Boat position, current
    HDOP - Position error, horizontal

   Raise function in device OBP40 has to be done inside config mode
   because of limited number of buttons.

  TODO 
    gzip for data transfer,
       miniz.c from ROM?
       manually inflating with tinflate from ROM
    Save position in FRAM
    Alarm: gps fix lost
    switch unit feet/meter
    force map update if new position is different from old position by 
    a certain level (e.g. 10m)
    windlass integration
    chain counter

    Map service options / URL parameters
      - mandatory
          lat: latitude
          lon: longitude
          width: image width in px (400)
          height: image height in px (300)
      - optional
          zoom: zoom level, default 15
          mrot: map rotation angle in degrees
          mtype: map type, default="Open Street Map"
          dtype: dithering type, default="Atkinson"
          cutout: image cutout type 0=none
          alpha: alpha blending for cutout
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
#include "ConfigMenu.h"
// #include "miniz.h" // devices without PSRAM use <rom/miniz.h>

// extern "C" {
  #include "rom/miniz.h"
// }

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
    ConfigMenu *menu;

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

    double scale = 50; // Radius of display circle in meter, depends on lat
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

    GwApi::BoatValue *bv_dbs; // depth below surface
    GwApi::BoatValue *bv_hdt; // true heading
    GwApi::BoatValue *bv_aws; // apparent wind speed
    GwApi::BoatValue *bv_awd; // apparent wind direction
    GwApi::BoatValue *bv_lat; // latitude, current
    GwApi::BoatValue *bv_lon; // longitude, current
    GwApi::BoatValue *bv_hdop; // horizontal position error

    bool simulation = false;
    int last_mapsize = 0;
    String errmsg = "";
    int loops;
    int readbytes = 0;

    void displayModeNormal(PageData &pageData) {

        // get currrent boatvalues
        bv_dbs = pageData.values[0]; // DBS
        String sval_dbs = formatValue(bv_dbs, *commonData).svalue;
        String sunit_dbs = formatValue(bv_dbs, *commonData).unit; 
        bv_hdt = pageData.values[1]; // HDT
        String sval_hdt = formatValue(bv_hdt, *commonData).svalue;
        bv_aws = pageData.values[2]; // AWS
        String sval_aws = formatValue(bv_aws, *commonData).svalue;
        String sunit_aws = formatValue(bv_aws, *commonData).unit; 
        bv_awd = pageData.values[3]; // AWD
        String sval_awd = formatValue(bv_awd, *commonData).svalue;
        bv_lat = pageData.values[4]; // LAT
        String sval_lat = formatValue(bv_lat, *commonData).svalue;
        bv_lon = pageData.values[5]; // LON
        String sval_lon = formatValue(bv_lon, *commonData).svalue;
        bv_hdop = pageData.values[6]; // HDOP
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
        //rotatePoints then draw lines
        // TODO rotate boat according to current heading
        if (bv_hdt->valid) {
            if (map_valid) {
                Point b1 = rotatePoint(c, {b.x, b.y - 8}, bv_hdt->value * RAD_TO_DEG);
                getdisplay().fillCircle(b1.x, b1.y, 10, commonData->bgcolor);
            }
            drawPoly(rotatePoints(c, pts_boat, bv_hdt->value * RAD_TO_DEG), commonData->fgcolor);
        } else {
            // no heading available: draw north oriented
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

        getdisplay().setCursor(8, 260);
        getdisplay().print("Press BACK to leave config");

/*        getdisplay().setCursor(8, 68);
        getdisplay().printf("Server: %s", server_name.c_str());
        getdisplay().setCursor(8, 88);
        getdisplay().printf("Port: %d", server_port);
        getdisplay().setCursor(8, 108);
        getdisplay().printf("Tilepath: %s", tile_path.c_str());
        getdisplay().setCursor(8, 128);
        getdisplay().printf("Last mapsize: %d", last_mapsize);
        getdisplay().setCursor(8, 148);
        getdisplay().printf("Last error: %s", errmsg);
        getdisplay().setCursor(8, 168);
        getdisplay().printf("Loops: %d, Readbytes: %d", loops, readbytes);
        */

        GwApi::BoatValue *bv_lat = pageData.values[4]; // LAT
        GwApi::BoatValue *bv_lon = pageData.values[5]; // LON
        if (!bv_lat->valid or !bv_lon->valid) {
            getdisplay().setCursor(8, 228);
            getdisplay().printf("No valid position: background map disabled");
        }

        // Display menu
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        for (int i = 0 ; i < menu->getItemCount(); i++) {
            ConfigMenuItem *itm = menu->getItemByIndex(i);
            if (!itm) {
               commonData->logger->logDebug(GwLog::ERROR, "Menu item not found: %d", i);
            } else {
                Rect r = menu->getItemRect(i);
                bool inverted = (i == menu->getActiveIndex());
                drawTextBoxed(r, itm->getLabel(), commonData->fgcolor, commonData->bgcolor, inverted, false);
                if (inverted and editmode > 0) {
                    // triangle as edit marker
                    getdisplay().fillTriangle(r.x + r.w + 20, r.y, r.x + r.w + 30, r.y + r.h / 2, r.x + r.w + 20, r.y + r.h,  commonData->fgcolor);
                }
                getdisplay().setCursor(r.x + r.w + 40, r.y + r.h - 4);
                if (itm->getType() == "int") {
                    getdisplay().print(itm->getValue());
                    getdisplay().print(itm->getUnit());
                } else {
                     getdisplay().print(itm->getValue() == 0 ? "No" : "Yes");
                }
            }
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

        if (simulation) {
            map_lat = 53.56938345759218;
            map_lon = 9.679658234303275;
        }

        canvas = new GFXcanvas1(264, 260); // Byte aligned, no padding!

        // Initialize config menu
        menu = new ConfigMenu("Options", 40, 80);
        menu->setItemDimension(150, 20);
        ConfigMenuItem *newitem;
        newitem = menu->addItem("chain", "Chain out", "int", 0, "m");
        newitem->setRange(0, 200, {1, 2, 5, 10});
        newitem = menu->addItem("alarm", "Alarm", "bool", 0, "");
        newitem = menu->addItem("alarm", "Alarm range", "int", 50, "m");
        newitem->setRange(0, 200, {1, 2, 5, 10});
        newitem = menu->addItem("raise", "Raise Anchor", "bool", 0, "");
        newitem = menu->addItem("zoom", "Zoom", "int", 15, "");
        newitem->setRange(14, 17, {1});
        menu->setItemActive("chain");

    }

    void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "CFG";
#ifdef BOARD_OBP40S3
        commonData->keydata[1].label = "DROP";
#endif
#ifdef BOARD_OBP60S3
        commonData->keydata[4].label = "DROP";
#endif
    }

    // TODO OBP40 / OBP60 different handling
    int handleKey(int key) {
        commonData->logger->logDebug(GwLog::LOG, "Page Anchor handle key %d", key);
        if (key == 1) { // Switch between normal and config mode
            if (mode == 'N') {
                mode = 'C';
#ifdef BOARD_OBP40S3
                commonData->keydata[0].label = "BACK";
                commonData->keydata[1].label = "EDIT";
#endif
            } else {
                mode = 'N';
#ifdef BOARD_OBP40S3
                commonData->keydata[0].label = "CFG";
                commonData->keydata[1].label = anchor_set ? "RAISE": "DROP";
#endif
#ifdef BOARD_OBP60S3
                commonData->keydata[4].label = anchor_set ? "RAISE": "DROP";
#endif
            }
            return 0;
        }
        if (key == 2) {
            if (mode == 'N') {
                anchor_set = !anchor_set;
                commonData->keydata[1].label = anchor_set ? "ALARM": "DROP";
                if (anchor_set) {
                    anchor_lat = bv_lat->value;
                    anchor_lon =  bv_lon->value;
                    anchor_depth = bv_dbs->value;
                    // TODO set timestamp
                    // anchor_ts =
                }
                return 0;
            } else if (mode == 'C') {
                // Change edit mode
                if (editmode > 0) {
                    editmode = 0;
                    commonData->keydata[1].label = "EDIT";
                } else {
                    editmode = 1;
                    commonData->keydata[1].label = "OK";
                }
            }
        }
        if (key == 9) {
            // OBP40 Down
            if (mode == 'C') {
                if (editmode > 0) {
                    // decrease current menu item
                     menu->getActiveItem()->decValue();
                } else {
                    // move to next menu item
                     menu->goNext();
                }
                return 0;
            }
        }
        if (key == 10) {
            // OBP40 Up
            if (mode == 'C') {
                if (editmode > 0) {
                    // increase current menu item
                     ConfigMenuItem *itm = menu->getActiveItem();                    
                     commonData->logger->logDebug(GwLog::LOG, "step = %d", itm->getStep());
                     itm->incValue();
                } else {
                    // move to previous menu item
                     menu->goPrev();
                }
                return 0;
            }
        }
        // Code for keylock
        if (key == 11) {
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
        const char* headerKeys[] = { "Content-Encoding", "Content-Length" };
        http.collectHeaders(headerKeys, 2);
        String url = "http://" + server_name + "/" + tile_path;
        String parameter = "?lat=" + String(lat, 6) + "&lon=" + String(lon, 6)+ "&zoom=" + String(zoom)
                         + "&width=" + String(map_width) + "&height=" + String(map_height);
        commonData->logger->logDebug(GwLog::LOG, "HTTP query: %s", String(url + parameter).c_str());
        http.begin(url + parameter);
        http.addHeader("Accept-Encoding", "deflate");
        int httpCode = http.GET();
        if (httpCode > 0) {
            commonData->logger->logDebug(GwLog::LOG, "HTTP GET result code: %d", httpCode);
            if (httpCode == HTTP_CODE_OK) {
                WiFiClient* stream = http.getStreamPtr();
                int size = http.getSize();
                String encoding = http.header("Content-Encoding");
                commonData->logger->logDebug(GwLog::LOG, "HTTP size: %d, encoding: '%s'", size, encoding);
                bool is_gzip = encoding.equalsIgnoreCase("deflate");

                uint8_t header[14]; // max: P4<LF>wwww wwww<LF>
                int header_size = 0;
                bool header_read = false;
                int n = 0;
                int ix = 0;

                uint8_t* buf = canvas->getBuffer();

                if (is_gzip) {
                    /* gzip compressed data
                     * has to be decompressed into a buffer big enough 
                     * to hold the whole data.
                     * so the PBM header is included
                     * search a method to use that as canvas without
                     * additional copy 
                     */
                    commonData->logger->logDebug(GwLog::LOG, "Map received in gzip encoding");

                    #define HEADER_MAX 24
                    #define HTTP_CHUNK 512
                    uint8_t in_buf[HTTP_CHUNK];
                    uint8_t header_buf[HEADER_MAX];
                    tinfl_decompressor decomp;
                    tinfl_init(&decomp);
                    size_t bitmap_written = 0;
                    size_t header_written = 0;
                    bool header_done = false;
                    int row_bytes = 0;
                    size_t expected_bitmap = 0;

                    while (stream->connected() || stream->available()) {
                        int bytes_read = stream->read(in_buf, HTTP_CHUNK);
                        if (bytes_read <= 0) break;
commonData->logger->logDebug(GwLog::LOG, "stream: bytes_read=%d", bytes_read);
                        size_t in_ofs = 0; // offset
                        while (in_ofs < (size_t)bytes_read) {
                            size_t in_size = bytes_read - in_ofs;
                            size_t out_size;
                            uint8_t *out_ptr;
                            uint8_t *out_ptr_next;
                            if (!header_done) {
                                if (header_written >= HEADER_MAX) {
                                    commonData->logger->logDebug(GwLog::LOG, "PBM header too large");
                                    return false;
                                }
                                out_ptr = header_buf + header_written;
                                out_size = HEADER_MAX - header_written;
                            } else {
                                out_ptr = buf + bitmap_written;
                                out_size = expected_bitmap - bitmap_written;
                            }
commonData->logger->logDebug(GwLog::LOG, "in_size=%d, out_size=%d", in_size, out_size);
 // TODO correct loop !!!
 // tinfl_status tinfl_decompress(
 // tinfl_decompressor *r,
 // const mz_uint8 *pIn_buf_next,
 // size_t *pIn_buf_size,
 // mz_uint8 *pOut_buf_start
 // mz_uint8 *pOut_buf_next,
 // size_t *pOut_buf_size,
 // const mz_uint32 decomp_flags)
                            tinfl_status status = tinfl_decompress(
                                &decomp,
                                in_buf + in_ofs, // start address in input buffer
                                &in_size,        // number of bytes to process
                                out_ptr,         // start of output buffer
                                out_ptr,         // next write position in output buffer
                                &out_size,       // free size in output buffer
                                // TINFL_FLAG_PARSE_ZLIB_HEADER |
                                TINFL_FLAG_HAS_MORE_INPUT |
                                TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF
                            );
                            if (status < 0) {
                                commonData->logger->logDebug(GwLog::LOG, "Decompression error (%d)", status);
                                return false;
                            }
                            in_ofs += in_size;
                            commonData->logger->logDebug(GwLog::LOG, "in_size=%d, in_ofs=%d", in_size, in_ofs);

                            if (!header_done) {
                                commonData->logger->logDebug(GwLog::LOG, "Decoding header");
                                header_written += out_size;

                                // Detect header end: two '\n'
                                char *first_nl = strchr((char*)header_buf, '\n');
                                if (!first_nl) continue;

                                char *second_nl = strchr(first_nl + 1, '\n');
                                if (!second_nl) continue;

                                // Null-terminate header for sscanf
                                header_buf[header_written < HEADER_MAX ? header_written : HEADER_MAX - 1] = 0;

                                // Check magic
                                if (strncmp((char*)header_buf, "P4", 2) != 0) {
                                    commonData->logger->logDebug(GwLog::LOG, "Invalid PBM magic");
                                    return false;
                                }

                                // Parse width and height strictly
                                int header_width = 0, header_height = 0;
                                if (sscanf((char*)header_buf, "P4\n%d %d", &header_width, &header_height) != 2) {
                                    commonData->logger->logDebug(GwLog::LOG, "Failed to parse PBM dimensions");
                                    return false;
                                }

                                if (header_width != map_width || header_height != map_height) {
                                    commonData->logger->logDebug(GwLog::LOG, "PBM size mismatch: header %dx%d, requested %dx%d\n",
                                        header_width, header_height, map_width, map_height);
                                    return false;
                                }
                                commonData->logger->logDebug(GwLog::LOG, "Header: %dx%d", header_width, header_height);

                                // Compute row bytes and expected bitmap size
                                row_bytes = (header_width + 7) / 8;
                                commonData->logger->logDebug(GwLog::LOG, "row_bytes=%d", row_bytes);
                                expected_bitmap = (size_t)row_bytes * header_height;
                                commonData->logger->logDebug(GwLog::LOG, "expected_bitmap=%d", expected_bitmap);

                                // Copy any extra decompressed bitmap after header
                                size_t header_size = (second_nl + 1) - (char*)header_buf;
                                commonData->logger->logDebug(GwLog::LOG, "header_size=%d", header_size);
                                size_t extra_bitmap = header_written - header_size;
                                commonData->logger->logDebug(GwLog::LOG, "extra bitmap=%d", extra_bitmap);

                                header_done = true;

                                if (extra_bitmap > 0) {
                                    memcpy(buf, header_buf + header_size, extra_bitmap);
                                    bitmap_written = extra_bitmap;
                                }
                            } else {
                                bitmap_written += out_size;
                                if (bitmap_written >= expected_bitmap) {
                                    commonData->logger->logDebug(GwLog::LOG, "Image fully received");
                                }
                            }
                            commonData->logger->logDebug(GwLog::LOG, "bitmap_written=%d", bitmap_written);
                        }
                    }
                } else {
                    // uncompressed data
                    commonData->logger->logDebug(GwLog::LOG, "Map received uncompressed");
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

        errmsg = "";

        map_lat = bv_lat->value; // save for later comparison
        map_lon = bv_lon->value;
        map_valid = getBackgroundMap(map_lat, map_lon, zoom);

        if (map_valid) {
            // prepare visible space for anchor-symbol or boat
            canvas->fillCircle(132, 130, 10, commonData->fgcolor);
        }
    };

    void display_side_keys() {
        // An rechter Seite neben dem Rad inc, dec, set etc ?
    }

    int displayPage(PageData &pageData) {

        // Logging boat values
        commonData->logger->logDebug(GwLog::LOG, "Drawing at PageAnchor; Mode=%c", mode);

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
