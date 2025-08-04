#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "ConfigMenu.h"
#include "images/logo64.xbm"
#include <esp32/clk.h>
#include "qrcode.h"

#ifdef BOARD_OBP40S3
#include <SD.h>
#include <FS.h>
#endif

#define STRINGIZE_IMPL(x) #x
#define STRINGIZE(x) STRINGIZE_IMPL(x)
#define VERSINFO STRINGIZE(GWDEVVERSION)
#define BOARDINFO STRINGIZE(BOARD)
#define PCBINFO STRINGIZE(PCBVERS)
#define DISPLAYINFO STRINGIZE(EPDTYPE)
#define GXEPD2INFO STRINGIZE(GXEPD2VERS)

/*
 * Special system page, called directly with fast key sequence 5,4
 * Out of normal page order.
 * Consists of some sub-pages with following content:
 *   1. Hard and software information
 *   2. System settings
 *   3. System configuration: running and NVRAM
 *   4. NMEA2000 device list
 *   5. SD Card information if available
 *
 * TODO
 *    - setCpuFrequencyMhz(80|160|240);
 *    - Accesspoint / ! Änderung im Gatewaycode erforderlich?
 *         if (! isApActive()) {
 *           wifiSSID = config->getString(config->wifiSSID);
 *           wifiPass = config->getString(config->wifiPass);
 *           wifiSoftAP(wifiSSID, wifiPass);
 *         }
 *     - Power mode
 *         powerInit(powermode);
 */

class PageSystem : public Page
{
private:
    GwConfigHandler *config;
    GwLog *logger;

    // NVRAM config options
    String flashLED;

    // Generic data access

    uint64_t chipid;
    bool simulation;
    bool sdcard;
    String buzzer_mode;
    uint8_t buzzer_power;
    String cpuspeed;
    String rtc_module;
    String gps_module;
    String env_module;

    String batt_sensor;
    String solar_sensor;
    String gen_sensor;
    String rot_sensor;
    double homelat;
    double homelon;

    char mode = 'N'; // (N)ormal, (S)ettings, (C)onfiguration, (D)evice list, c(A)rd
    int8_t editmode = -1; // marker for menu/edit/set function

    ConfigMenu *menu;

    void incMode() {
        if (mode == 'N') {        // Normal
            mode = 'S';
        } else if (mode == 'S') { // Settings
            mode = 'C';
        } else if (mode == 'C') { // Config
            mode = 'D';
        } else if (mode == 'D') { // Device list
            if (sdcard) {
                mode = 'A';       // SD-Card
            } else {
                mode = 'N';
            }
        } else {
            mode = 'N';
        }
    }

    void decMode() {
        if (mode == 'N') {
            if (sdcard) {
                mode = 'A';
            } else {
                mode = 'D';
            }
        } else if (mode == 'S') { // Settings
            mode = 'N';
        } else if (mode == 'C') { // Config
            mode = 'S';
        } else if (mode == 'D') { // Device list
            mode = 'C';
        } else {
            mode = 'D';
        }
    }

    void displayModeNormal() {
        // Default system page view

        uint16_t y0 = 155;

        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 48);
        getdisplay().print("System information");

        getdisplay().drawXBitmap(320, 25, logo64_bits, logo64_width, logo64_height, commonData->fgcolor);

        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        char ssid[13];
        snprintf(ssid, 13, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
        displayBarcode(String(ssid), 320, 200, 2);
        getdisplay().setCursor(8, 70);
        getdisplay().print(String("MCUDEVICE-") + String(ssid));

        getdisplay().setCursor(8, 95);
        getdisplay().print("Firmware version: ");
        getdisplay().setCursor(150, 95);
        getdisplay().print(VERSINFO);

        getdisplay().setCursor(8, 113);
        getdisplay().print("Board version: ");
        getdisplay().setCursor(150, 113);
        getdisplay().print(BOARDINFO);
        getdisplay().print(String(" HW ") + String(PCBINFO));

        getdisplay().setCursor(8, 131);
        getdisplay().print("Display version: ");
        getdisplay().setCursor(150, 131);
        getdisplay().print(DISPLAYINFO);
        getdisplay().print("; GxEPD2 v");
        getdisplay().print(GXEPD2INFO);

        getdisplay().setCursor(8, 265);
#ifdef BOARD_OBP60S3
        getdisplay().print("Press STBY to enter deep sleep mode");
#endif
#ifdef BOARD_OBP40S3
        getdisplay().print("Press wheel to enter deep sleep mode");
#endif

        // Flash memory size
        uint32_t flash_size = ESP.getFlashChipSize();
        getdisplay().setCursor(8, y0);
        getdisplay().print("FLASH:");
        getdisplay().setCursor(90, y0);
        getdisplay().print(String(flash_size / 1024) + String(" kB"));

        // PSRAM memory size
        uint32_t psram_size = ESP.getPsramSize();
        getdisplay().setCursor(8, y0 + 16);
        getdisplay().print("PSRAM:");
        getdisplay().setCursor(90, y0 + 16);
        getdisplay().print(String(psram_size / 1024) + String(" kB"));

        // FRAM available / status
        getdisplay().setCursor(8, y0 + 32);
        getdisplay().print("FRAM:");
        getdisplay().setCursor(90, y0 + 32);
        getdisplay().print(hasFRAM ? "available" : "not found");

#ifdef BOARD_OBP40S3
        // SD-Card
        getdisplay().setCursor(8, y0 + 48);
        getdisplay().print("SD-Card:");
        getdisplay().setCursor(90, y0 + 48);
        if (sdcard) {
            uint64_t cardsize = SD.cardSize() / (1024 * 1024);
            getdisplay().print(String(cardsize) + String(" MB"));
        } else {
            getdisplay().print("off");
        }
#endif

        // CPU speed config / active
        getdisplay().setCursor(202, y0);
        getdisplay().print("CPU speed:");
        getdisplay().setCursor(300, y0);
        getdisplay().print(cpuspeed);
        getdisplay().print(" / ");
        int cpu_freq = esp_clk_cpu_freq() / 1000000;
        getdisplay().print(String(cpu_freq));

        // total RAM free
        int Heap_free = esp_get_free_heap_size();
        getdisplay().setCursor(202, y0 + 16);
        getdisplay().print("Total free:");
        getdisplay().setCursor(300, y0 + 16);
        getdisplay().print(String(Heap_free));

        // RAM free for task
        int RAM_free = uxTaskGetStackHighWaterMark(NULL);
        getdisplay().setCursor(202, y0 + 32);
        getdisplay().print("Task free:");
        getdisplay().setCursor(300, y0 + 32);
        getdisplay().print(String(RAM_free));
    }

    void displayModeConfig() {
        // Configuration interface

        uint16_t x0 = 16;
        uint16_t y0 = 80;
        uint16_t dy = 20;

        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 48);
        getdisplay().print("System configuration");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        /*getdisplay().setCursor(x0, y0);
        getdisplay().print("CPU speed: 80 | 160 | 240");
        getdisplay().setCursor(x0, y0 + 1 * dy);
        getdisplay().print("Power mode: Max | 5V | Min");
        getdisplay().setCursor(x0, y0 + 2 * dy);
        getdisplay().print("Accesspoint: On | Off");

        // TODO Change NVRAM-preferences settings here
        getdisplay().setCursor(x0, y0 + 4 * dy);
        getdisplay().print("Simulation: On | Off"); */

        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        for (int i = 0 ; i < menu->getItemCount(); i++) {
            ConfigMenuItem *itm = menu->getItemByIndex(i);
            if (!itm) {
                LOG_DEBUG(GwLog::ERROR, "Menu item not found: %d", i);
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

    void displayModeSettings() {
        // View some of the current settings

        const uint16_t x0 = 8;
        const uint16_t y0 = 72;

        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(x0, 48);
        getdisplay().print("System settings");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        // left column
        getdisplay().setCursor(x0, y0);
        getdisplay().print("Simulation:");
        getdisplay().setCursor(120, y0);
        getdisplay().print(simulation ? "on" : "off");

        getdisplay().setCursor(x0, y0 + 16);
        getdisplay().print("Environment:");
        getdisplay().setCursor(120, y0 + 16);
        getdisplay().print(env_module);

        getdisplay().setCursor(x0, y0 + 32);
        getdisplay().print("Buzzer:");
        getdisplay().setCursor(120, y0 + 32);
        getdisplay().print(buzzer_mode);

        getdisplay().setCursor(x0, y0 + 64);
        getdisplay().print("GPS:");
        getdisplay().setCursor(120, y0 + 64);
        getdisplay().print(gps_module);

        getdisplay().setCursor(x0, y0 + 80);
        getdisplay().print("RTC:");
        getdisplay().setCursor(120, y0 + 80);
        getdisplay().print(rtc_module);

        getdisplay().setCursor(x0, y0 + 96);
        getdisplay().print("Wifi:");
        getdisplay().setCursor(120, y0 + 96);
        getdisplay().print(commonData->status.wifiApOn ? "on" : "off");

        // Home location
        getdisplay().setCursor(x0, y0 + 128);
        getdisplay().print("Home Lat.:");
        drawTextRalign(230, y0 + 128, formatLatitude(homelat));
        getdisplay().setCursor(x0, y0 + 144);
        getdisplay().print("Home Lon.:");
        drawTextRalign(230, y0 + 144, formatLongitude(homelon));

        // right column
        getdisplay().setCursor(202, y0);
        getdisplay().print("Batt. sensor:");
        getdisplay().setCursor(320, y0);
        getdisplay().print(batt_sensor);

        // Solar sensor
        getdisplay().setCursor(202, y0 + 16);
        getdisplay().print("Solar sensor:");
        getdisplay().setCursor(320, y0 + 16);
        getdisplay().print(solar_sensor);

        // Generator sensor
        getdisplay().setCursor(202, y0 + 32);
        getdisplay().print("Gen. sensor:");
        getdisplay().setCursor(320, y0 + 32);
        getdisplay().print(gen_sensor);

#ifdef BOARD_OBP60S3
        // Backlight infos
        getdisplay().setCursor(202, y0 + 64);
        getdisplay().print("Backlight:");
        getdisplay().setCursor(320, y0 + 64);
        getdisplay().printf("%d%%", commonData->backlight.brightness);
        // TODO test function with OBP60 device
        getdisplay().setCursor(202, y0 + 80);
        getdisplay().print("Bl color:");
        getdisplay().setCursor(320, y0 + 80);
        getdisplay().print(commonData->backlight.color.toName());
        getdisplay().setCursor(202, y0 + 96);
        getdisplay().print("Bl mode:");
        getdisplay().setCursor(320, y0 + 96);
        getdisplay().print(commonData->backlight.mode);
        // TODO Buzzer mode and power
#endif

        // Gyro sensor
        // WIP / FEATURE
    }

    void displayModeSDCard() {
        // SD card info
        uint16_t x0 = 20;
        uint16_t y0 = 72;

        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 48);
        getdisplay().print("SD Card info");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(x0, y0);
        getdisplay().print("Work in progress...");
    }

    void displayModeDevicelist() {
        // NMEA2000 device list
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(8, 48);
        getdisplay().print("NMEA2000 device list");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(20, 80);
        getdisplay().print("RxD: ");
        getdisplay().print(String(commonData->status.n2kRx));
        getdisplay().setCursor(20, 100);
        getdisplay().print("TxD: ");
        getdisplay().print(String(commonData->status.n2kTx));
    }

   void storeConfig() {
       menu->storeValues();
   }

public:
    PageSystem(CommonData &common){
        commonData = &common;
        config = commonData->config;
        logger = commonData->logger;

        logger->logDebug(GwLog::LOG,"Instantiate PageSystem");
        if (hasFRAM) {
            mode = fram.read(FRAM_SYSTEM_MODE);
        }
        flashLED = common.config->getString(common.config->flashLED);

        chipid = ESP.getEfuseMac();
        simulation = common.config->getBool(common.config->useSimuData);
#ifdef BOARD_OBP40S3
        sdcard = common.config->getBool(common.config->useSDCard);
#endif
        buzzer_mode = common.config->getString(common.config->buzzerMode);
        buzzer_mode.toLowerCase();
        buzzer_power = common.config->getInt(common.config->buzzerPower);
        cpuspeed = common.config->getString(common.config->cpuSpeed);
        env_module = common.config->getString(common.config->useEnvSensor);
        rtc_module = common.config->getString(common.config->useRTC);
        gps_module = common.config->getString(common.config->useGPS);
        batt_sensor = common.config->getString(common.config->usePowSensor1);
        solar_sensor = common.config->getString(common.config->usePowSensor2);
        gen_sensor = common.config->getString(common.config->usePowSensor3);
        rot_sensor = common.config->getString(common.config->useRotSensor);
        homelat = common.config->getString(common.config->homeLAT).toDouble();
        homelon = common.config->getString(common.config->homeLON).toDouble();

        // CPU speed: 80 | 160 | 240
        // Power mode: Max | 5V | Min
        // Accesspoint: On | Off

        // TODO Change NVRAM-preferences settings here
        // getdisplay().setCursor(x0, y0 + 4 * dy);
        // getdisplay().print("Simulation: On | Off");

        // Initialize config menu
        menu = new ConfigMenu("Options", 40, 80);
        menu->setItemDimension(150, 20);

        ConfigMenuItem *newitem;
        newitem = menu->addItem("accesspoint", "Accesspoint", "bool", 0, "");
        newitem = menu->addItem("simulation", "Simulation", "on/off", 0, "");
        menu->setItemActive("accesspoint");

    }

    virtual void setupKeys(){
        commonData->keydata[0].label = "EXIT";
        commonData->keydata[1].label = "MODE";
        commonData->keydata[2].label = "";
        commonData->keydata[3].label = "RST";
        commonData->keydata[4].label = "STBY";
        commonData->keydata[5].label = "ILUM";
    }

    virtual int handleKey(int key){
        // do *NOT* handle key #1 this handled by obp60task as exit
        // Switch display mode
        commonData->logger->logDebug(GwLog::LOG, "System keyboard handler");
        if (key == 2) {
            incMode();
            if (hasFRAM) fram.write(FRAM_SYSTEM_MODE, mode);
            return 0;
        }
#ifdef BOARD_OBP60S3
        // grab cursor key to disable page navigation
        if (key == 3) {
            return 0;
        }
        // soft reset
        if (key == 4) {
            ESP.restart();
        }
        // standby / deep sleep
        if (key == 5) {
           deepSleep(*commonData);
        }
        // Code for keylock
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0;
        }
#endif
#ifdef BOARD_OBP40S3
        // use cursor keys for local mode navigation
        if (key == 9) {
            incMode();
            return 0;
        }
        if (key == 10) {
            decMode();
            return 0;
        }
        // standby / deep sleep
        if (key == 12) {
            deepSleep(*commonData);
        }
#endif
        return key;
    }

    void displayBarcode(String serialno, uint16_t x, uint16_t y, uint16_t s) {
        // Barcode with serial number
        // x, y is top left corner
        // s is pixel size of a single box
        QRCode qrcode;
        uint8_t qrcodeData[qrcode_getBufferSize(4)];
#ifdef BOARD_OBP40S3
        String prefix = "OBP40:SN:";
#endif
#ifdef BOARD_OBP60S3
        String prefix = "OBP60:SN:";
#endif
        qrcode_initText(&qrcode, qrcodeData, 4, 0, (prefix + serialno).c_str());
        int16_t x0 = x;
        for (uint8_t j = 0; j < qrcode.size; j++) {
            for (uint8_t i = 0; i < qrcode.size; i++) {
                if (qrcode_getModule(&qrcode, i, j)) {
                    getdisplay().fillRect(x, y, s, s, commonData->fgcolor);
                }
                x += s;
            }
            y += s;
            x = x0;
        }
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        // Optical warning by limit violation (unused)
        if(flashLED == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging page information
        LOG_DEBUG(GwLog::LOG,"Drawing at PageSystem, Mode=%c", mode);

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height());

        // call current system page
        switch (mode) {
            case 'N':
                displayModeNormal();
                break;
            case 'S':
                displayModeSettings();
                break;
            case 'C':
                displayModeConfig();
                break;
            case 'A':
                displayModeSDCard();
                break;
            case 'D':
                displayModeDevicelist();
                break;
        }

        // Update display
        getdisplay().nextPage();    // Partial update (fast)
        return PAGE_OK;
    };
};

static Page* createPage(CommonData &common){
    return new PageSystem(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageSystem(
    "System",   // Page name
    createPage, // Action
    0,          // No bus values
    true        // Headers are anabled so far
);

#endif
