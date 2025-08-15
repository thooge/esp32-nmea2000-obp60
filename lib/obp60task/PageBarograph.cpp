#ifdef BOARD_OBP60S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageBarograph : public Page{

    bool keylock = false;
    bool has_fram = false;
    String flashLED;
    String useenvsensor;

    char source = 'I'; // (I)nternal, e(X)ternal
    const int series[5] = {75, 150, 300, 600, 900};
    const int zoom[5] = {1, 2, 3, 6, 12};
    int zoomindex = 4;
    uint16_t data[336] = {0}; // current data to display

    // y-axis
    uint16_t vmin;
    uint16_t vmax;
    uint16_t scalemin = 1000;
    uint16_t scalemax = 1020;
    uint16_t scalestep = 5;
    int hist1 = 0;  // one hour trend
    int hist3 = 0;  // three hours trend

    long refresh = 0; // millis

    void loadData() {
        // Transfer data from history to page buffer,
        // set y-axis according to data
        int i = zoom[zoomindex];

        // get min and max values of measured data
        vmin = data[0];
        vmax = data[0];
        for (int x = 0; x < 336; x++) {
            if (data[x] != 0) {
                if (data[x] < vmin) {
                    vmin = data[x];
                } else if (data[x] > vmax) {
                    vmax = data[x];
                }
            }
        }

        // calculate y-axis scale
        uint16_t diff = vmax - vmin;
        if (diff < 20) {
            scalestep = 5;
        } else if (diff < 40) {
            scalestep = 10;
        } else {
            scalestep = 15;
        }
        scalemin = vmin - (vmin % scalestep);
        scalemax = vmax + scalestep - (vmax % scalestep);

        // TODO implement history buffer
    };

    public:
    PageBarograph(CommonData &common){
        common.logger->logDebug(GwLog::LOG,"Instantiate PageBarograph");
        // Get config data
        flashLED = common.config->getString(common.config->flashLED);
        useenvsensor = common.config->getString(common.config->useEnvSensor);
        // possible values for internal sensor
        static std::vector<String> sensorList = {
            "BME280", "BMP280", "BMP180", "BMP085", "HTU21", "SHT21"
            };
        if (std::find(sensorList.begin(), sensorList.end(), useenvsensor) != sensorList.end()) {
            source = 'I';
        } else {
            // "off" means user external data if available
            source = 'X';
        }
        //common.logger->logDebug(GwLog::LOG,"Source=%s (%s)", source, useenvsensor);
        loadData(); // initial load
    }

    virtual int handleKey(int key) {
        if (key == 1) {
            // zoom in
            if (zoomindex > 0) {
                zoomindex -= 1;
            }
            return 0;
        }
        if (key == 2) {
            // zoom out
            if (zoomindex < sizeof(zoom)) {
                zoomindex += 1;
            }
            return 0;
        }
        if (key == 11) {
            keylock = !keylock;
            return 0;
        }
        return key;
    }

    virtual void displayPage(CommonData &commonData, PageData &pageData){
        GwLog *logger=commonData.logger;

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageBarograph");

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        // Frames 
        getdisplay().fillRect(0, 75, 400, 2, commonData.fgcolor);  // fillRect: x, y, w, h
        getdisplay().fillRect(130, 20, 2, 55, commonData.fgcolor);
        getdisplay().fillRect(270, 20, 2, 55, commonData.fgcolor);
        getdisplay().fillRect(325, 20, 2, 55, commonData.fgcolor);

        getdisplay().setTextColor(commonData.fgcolor);

        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        if (source == 'I') {
            drawTextCenter(360, 40, useenvsensor);
        } else {
            drawTextCenter(360, 40, "ext.");
        }

        // Trend
        drawTextCenter(295, 62, "0.0");

        // Alarm placeholder
         drawTextCenter(70, 62, "Alarm Off");

        // Zoom
        int datastep = series[zoomindex];
        String fmt;
        if (datastep > 120) {
            if (datastep % 60 == 0) {
                fmt = String(datastep / 60.0, 0) + " min";
            } else {
                fmt = String(datastep / 60.0, 1) + " min";
            }
        } else {
            fmt = String(datastep) + " s";
        }
        drawTextCenter(360, 62, fmt);

        // Current measurement
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        drawTextCenter(200, 40, String(commonData.data.airPressure / 100, 1));
        getdisplay().setFont(&Ubuntu_Bold8pt7b);
        drawTextCenter(200, 62, "hPa"); // Unit

        // Diagram
        const int xstep = 48; // x-axis-grid
        const int x0 = 350;   // origin
        const int y0 = 270;
        const int w = 7 * 48;
        const int h = 180;

        // getdisplay().drawRect(x0 - w, y0 - h, w, h, commonData.fgcolor);

        // x-axis are hours
        for (int i = 1; i <= 6; i++) {
            String label = String(-1 * zoom[zoomindex] * i);
            getdisplay().drawLine(x0 - i * xstep, y0, x0 - i * xstep, y0 - h, commonData.fgcolor);
            drawTextCenter(x0 - i * xstep, y0 - 10, label);
        }

        // y-axis
        getdisplay().drawLine(x0 + 5, y0, x0 + 5, y0 - h, commonData.fgcolor);  // drawLine: x1, y1, x2, y2
        getdisplay().drawLine(x0 - w, y0, x0 - w, y0 - h, commonData.fgcolor);
        getdisplay().drawLine(x0 - w - 5, y0, x0 - w - 5, y0 - h, commonData.fgcolor);
        getdisplay().drawLine(x0, y0, x0, y0 - h, commonData.fgcolor);

        int16_t dy = 9; // px for one hPa
        int16_t y = y0;
        int16_t ys = scalemin;
        while (y >= y0 - h) {
            if (y % scalestep == 0) {
                // big step, show label and long line
                getdisplay().setCursor(x0 + 10, y + 5);
                getdisplay().print(String(ys));
                getdisplay().drawLine(x0 + 5, y, x0 - w - 5, y, commonData.fgcolor);
            } else {
                // small step, only short lines left and right
                getdisplay().drawLine(x0 + 5, y, x0, y, commonData.fgcolor);
                getdisplay().drawLine(x0 - w - 5, y, x0 - w, y, commonData.fgcolor);
            }
            y -= dy;
            ys += 1;
        }

        // Update display
        getdisplay().nextPage();    // Partial update (fast)

    };
};

static Page* createPage(CommonData &common){
    return new PageBarograph(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageBarograph(
    "Barograph",    // Page name
    createPage,     // Action
    0,              // No bus values needed
    true           // Show display header on/off
);

#endif
