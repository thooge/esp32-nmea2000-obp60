// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "ConfigMenu.h"
#include "images/logo64.xbm"
#include <esp32/clk.h>
#include "qrcode.h"
#include "Nmea2kTwai.h"


#ifdef BOARD_OBP40S3
// #include <SD.h>
// #include <FS.h>
#include "dirent.h"
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
    // NVRAM config options
    String flashLED;

    // Generic data access

    uint64_t chipid;
    bool simulation;
    bool use_sdcard;
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

    Nmea2kTwai *NMEA2000;

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
            if (use_sdcard) {
                mode = 'A';       // SD-Card
            } else {
                mode = 'N';
            }
        } else {
            mode = 'N';
        }
        if (hasFRAM) fram.write(FRAM_SYSTEM_MODE, mode);
    }

    void decMode() {
        if (mode == 'N') {
            if (use_sdcard) {
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
        if (hasFRAM) fram.write(FRAM_SYSTEM_MODE, mode);
    }

    void displayModeNormal() {
        // Default system page view

        uint16_t y0 = 155;

        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("System information");

        epd->drawXBitmap(320, 25, logo64_bits, logo64_width, logo64_height, commonData->fgcolor);

        epd->setFont(&Ubuntu_Bold8pt8b);

        char ssid[13];
        snprintf(ssid, 13, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
        displayBarcode(String(ssid), 320, 200, 2);
        epd->setCursor(8, 70);
        epd->print(String("MCUDEVICE-") + String(ssid));

        epd->setCursor(8, 95);
        epd->print("Firmware version: ");
        epd->setCursor(150, 95);
        epd->print(VERSINFO);

        epd->setCursor(8, 113);
        epd->print("Board version: ");
        epd->setCursor(150, 113);
        epd->print(BOARDINFO);
        epd->print(String(" HW ") + String(PCBINFO));

        epd->setCursor(8, 131);
        epd->print("Display version: ");
        epd->setCursor(150, 131);
        epd->print(DISPLAYINFO);
        epd->print("; GxEPD2 v");
        epd->print(GXEPD2INFO);

        epd->setCursor(8, 265);
#ifdef BOARD_OBP60S3
        epd->print("Press STBY to enter deep sleep mode");
#endif
#ifdef BOARD_OBP40S3
        epd->print("Press wheel to enter deep sleep mode");
#endif

        // Flash memory size
        uint32_t flash_size = ESP.getFlashChipSize();
        epd->setCursor(8, y0);
        epd->print("FLASH:");
        epd->setCursor(90, y0);
        epd->print(String(flash_size / 1024) + String(" kB"));

        // PSRAM memory size
        uint32_t psram_size = ESP.getPsramSize();
        epd->setCursor(8, y0 + 16);
        epd->print("PSRAM:");
        epd->setCursor(90, y0 + 16);
        epd->print(String(psram_size / 1024) + String(" kB"));

        // FRAM available / status
        epd->setCursor(8, y0 + 32);
        epd->print("FRAM:");
        epd->setCursor(90, y0 + 32);
        epd->print(hasFRAM ? "available" : "not found");

#ifdef BOARD_OBP40S3
        // SD-Card
        epd->setCursor(8, y0 + 48);
        epd->print("SD-Card:");
        epd->setCursor(90, y0 + 48);
        if (hasSDCard) {
            uint64_t cardsize = ((uint64_t) sdcard->csd.capacity) * sdcard->csd.sector_size / (1024 * 1024);
            // epd->print(String(cardsize) + String(" MB"));
            epd->printf("%llu MB", cardsize);
        } else {
            epd->print("off");
        }
#endif

        // Uptime
        int64_t uptime = esp_timer_get_time() / 1000000;
        String uptime_unit;
        if (uptime < 120) {
            uptime_unit = " seconds";
        } else {
            if (uptime < 2 * 3600) {
                uptime /= 60;
                uptime_unit = " minutes";
            } else if (uptime < 2 * 3600 * 24) {
                uptime /= 3600;
                uptime_unit = " hours";
            } else {
                uptime /= 86400;
                uptime_unit = " days";
            }
        }
        epd->setCursor(8, y0 + 80);
        epd->print("Uptime:");
        epd->setCursor(90, y0 + 80);
        epd->print(uptime);
        epd->print(uptime_unit);

        // CPU speed config / active
        epd->setCursor(202, y0);
        epd->print("CPU speed:");
        epd->setCursor(300, y0);
        epd->print(cpuspeed);
        epd->print(" / ");
        int cpu_freq = esp_clk_cpu_freq() / 1000000;
        epd->print(String(cpu_freq));

        // total RAM free
        int Heap_free = esp_get_free_heap_size();
        epd->setCursor(202, y0 + 16);
        epd->print("Total free:");
        epd->setCursor(300, y0 + 16);
        epd->print(String(Heap_free));

        // RAM free for task
        int RAM_free = uxTaskGetStackHighWaterMark(NULL);
        epd->setCursor(202, y0 + 32);
        epd->print("Task free:");
        epd->setCursor(300, y0 + 32);
        epd->print(String(RAM_free));
    }

    void displayModeConfig() {
        // Configuration interface

        uint16_t x0 = 16;
        uint16_t y0 = 80;
        uint16_t dy = 20;

        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("System configuration");

        epd->setFont(&Ubuntu_Bold8pt8b);

        /*epd->setCursor(x0, y0);
        epd->print("CPU speed: 80 | 160 | 240");
        epd->setCursor(x0, y0 + 1 * dy);
        epd->print("Power mode: Max | 5V | Min");
        epd->setCursor(x0, y0 + 2 * dy);
        epd->print("Accesspoint: On | Off");

        // TODO Change NVRAM-preferences settings here
        epd->setCursor(x0, y0 + 4 * dy);
        epd->print("Simulation: On | Off"); */

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

    void displayModeSettings() {
        // View some of the current settings

        const uint16_t x0 = 8;
        const uint16_t y0 = 72;

        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(x0, 48);
        epd->print("System settings");

        epd->setFont(&Ubuntu_Bold8pt8b);

        // left column
        epd->setCursor(x0, y0);
        epd->print("Simulation:");
        epd->setCursor(120, y0);
        epd->print(simulation ? "on" : "off");

        epd->setCursor(x0, y0 + 16);
        epd->print("Environment:");
        epd->setCursor(120, y0 + 16);
        epd->print(env_module);

        epd->setCursor(x0, y0 + 32);
        epd->print("Buzzer:");
        epd->setCursor(120, y0 + 32);
        epd->print(buzzer_mode);

        epd->setCursor(x0, y0 + 64);
        epd->print("GPS:");
        epd->setCursor(120, y0 + 64);
        epd->print(gps_module);

        epd->setCursor(x0, y0 + 80);
        epd->print("RTC:");
        epd->setCursor(120, y0 + 80);
        epd->print(rtc_module);

        epd->setCursor(x0, y0 + 96);
        epd->print("Wifi:");
        epd->setCursor(120, y0 + 96);
        epd->print(commonData->status.wifiApOn ? "on" : "off");

        // Home location
        epd->setCursor(x0, y0 + 128);
        epd->print("Home Lat.:");
        drawTextRalign(230, y0 + 128, formatLatitude(homelat));
        epd->setCursor(x0, y0 + 144);
        epd->print("Home Lon.:");
        drawTextRalign(230, y0 + 144, formatLongitude(homelon));

        // right column
        epd->setCursor(202, y0);
        epd->print("Batt. sensor:");
        epd->setCursor(320, y0);
        epd->print(batt_sensor);

        // Solar sensor
        epd->setCursor(202, y0 + 16);
        epd->print("Solar sensor:");
        epd->setCursor(320, y0 + 16);
        epd->print(solar_sensor);

        // Generator sensor
        epd->setCursor(202, y0 + 32);
        epd->print("Gen. sensor:");
        epd->setCursor(320, y0 + 32);
        epd->print(gen_sensor);

#ifdef BOARD_OBP60S3
        // Backlight infos
        epd->setCursor(202, y0 + 64);
        epd->print("Backlight:");
        epd->setCursor(320, y0 + 64);
        epd->printf("%d%%", commonData->backlight.brightness);
        // TODO test function with OBP60 device
        epd->setCursor(202, y0 + 80);
        epd->print("Bl color:");
        epd->setCursor(320, y0 + 80);
        epd->print(commonData->backlight.color.toName());
        epd->setCursor(202, y0 + 96);
        epd->print("Bl mode:");
        epd->setCursor(320, y0 + 96);
        epd->print(commonData->backlight.mode);
        // TODO Buzzer mode and power
#endif

        // Gyro sensor
        // WIP / FEATURE
    }

    void displayModeSDCard() {
        // SD card info
        uint16_t x0 = 20;
        uint16_t y0 = 72;

        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("SD card info");

        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(x0, y0);
        epd->print("Work in progress...");

        // TODO directories IMG, MAP, HIST should exist.
        // Show state: Files and used size

        // Simple test for one file in root
        epd->setCursor(x0, y0 + 32);
        String file_test = MOUNT_POINT "/IMG/icon-settings2.pbm";
        logger->logDebug(GwLog::LOG, "Testfile: %s", file_test.c_str());
        struct stat st;
        if (stat(file_test.c_str(), &st) == 0) {
            epd->printf("File %s exists", file_test.c_str());
        } else {
            epd->printf("File %s not found", file_test.c_str());
        }

        // Root directory check
        DIR* dir = opendir(MOUNT_POINT);
        if (dir != NULL) {
            logger->logDebug(GwLog::LOG, "Root directory: %s", MOUNT_POINT);
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                logger->logDebug(GwLog::LOG, "    %s type %d", entry->d_name, entry->d_type);
                // type 1 is file
                // type 2 is dir
            }
            closedir(dir);
        } else {
            logger->logDebug(GwLog::LOG, "Failed to open root directory");
        }

        // try to load example pbm file 
        // TODO create PBM load function in imglib
        String filepath = MOUNT_POINT "/IMG/icon-settings2.pbm";
        const char* file_img = filepath.c_str();
        FILE* fh = fopen(file_img, "r");
        if (fh != NULL) {
            logger->logDebug(GwLog::LOG, "Open file for reading: %s", file_img);

            char magic[3];
            char buffer[80];

            // 2 Byte Magic: P1=ASCII, P4=Binary
            fgets(magic, sizeof(magic), fh);
            if (strcmp(magic, "P1") != 0) {
                logger->logDebug(GwLog::LOG, "Not a valid PBM file of type P1 (%s)", magic);
            } else {
                uint16_t width = 0;
                uint16_t height = 0;
                while (fgets(buffer, sizeof(buffer), fh)) {
                     // logger->logDebug(GwLog::LOG, "%s", buffer);
                     if (buffer[0] == '#') {
                         continue; // skip comment
                     }
                     if (sscanf(buffer, "%d %d", &width, &height) == 2) {
                         break;
                     }
                }
                logger->logDebug(GwLog::LOG, "Image: %dx%d", width, height);
            }

            /*size_t bytesRead = fread(buffer, sizeof(char), sizeof(buffer) - 1, fh);
            buffer[bytesRead] = '\0';  // Null-terminate the string
            if (bytesRead > 0) {
                logger->logDebug(GwLog::LOG, "Read %d bytes", bytesRead);
                logger->logDebug(GwLog::LOG, ">%s<", buffer);
            } */

            /*  WIP
            // Read pixel data and pack into the buffer
            for (int i = 0; i < width * height; i++) {
                int pixel;
                fscanf(file, "%d", &pixel);

                // Calculate the byte index and bit position
                int byteIndex = i / 8;
                int bitPosition = 7 - (i % 8); // Store MSB first

                // Set the corresponding bit in the byte
                if (pixel == 1) {
                    buffer[byteIndex] |= (1 << bitPosition);
                }
            }
            */

            fclose(fh);
            // epd->drawXBitmap(20, 200, buffer, width, height, commonData.fgcolor);
        }
    }

    void displayModeDevicelist() {
        // NMEA2000 device list
        epd->setFont(&Ubuntu_Bold12pt8b);
        epd->setCursor(8, 48);
        epd->print("NMEA2000 device list");

        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(20, 80);
        epd->print("RxD: ");
        epd->print(String(commonData->status.n2kRx));
        epd->setCursor(20, 100);
        epd->print("TxD: ");
        epd->print(String(commonData->status.n2kTx));

        epd->setCursor(20, 140);
        epd->printf("N2k source address: %d", NMEA2000->GetN2kSource());

    }

   void storeConfig() {
       menu->storeValues();
   }

public:
    PageSystem(CommonData &common) : Page(common)
    {
        logger->logDebug(GwLog::LOG, "Instantiate PageSystem");
        if (hasFRAM) {
            mode = fram.read(FRAM_SYSTEM_MODE);
            logger->logDebug(GwLog::LOG, "Loaded mode '%c' from FRAM", mode);
        }
        flashLED = config->getString(config->flashLED);

        chipid = ESP.getEfuseMac();
        simulation = config->getBool(config->useSimuData);
#ifdef BOARD_OBP40S3
        use_sdcard = config->getBool(config->useSDCard);
#endif
        buzzer_mode = config->getString(config->buzzerMode);
        buzzer_mode.toLowerCase();
        buzzer_power = config->getInt(config->buzzerPower);
        cpuspeed = config->getString(config->cpuSpeed);
        env_module = config->getString(config->useEnvSensor);
        rtc_module = config->getString(config->useRTC);
        gps_module = config->getString(config->useGPS);
        batt_sensor = config->getString(config->usePowSensor1);
        solar_sensor = config->getString(config->usePowSensor2);
        gen_sensor = config->getString(config->usePowSensor3);
        rot_sensor = config->getString(config->useRotSensor);
        homelat = config->getString(config->homeLAT).toDouble();
        homelon = config->getString(config->homeLON).toDouble();

        // CPU speed: 80 | 160 | 240
        // Power mode: Max | 5V | Min
        // Accesspoint: On | Off

        // TODO Change NVRAM-preferences settings here
        // epd->setCursor(x0, y0 + 4 * dy);
        // epd->print("Simulation: On | Off");

        // Initialize config menu
        menu = new ConfigMenu("Options", 40, 80);
        menu->setItemDimension(150, 20);

        ConfigMenuItem *newitem;
        newitem = menu->addItem("accesspoint", "Accesspoint", "bool", 0, "");
        newitem = menu->addItem("simulation", "Simulation", "on/off", 0, "");
        menu->setItemActive("accesspoint");

        // Create info-file if not exists
        // TODO

    }

    void setupKeys() {
        commonData->keydata[0].label = "EXIT";
        commonData->keydata[1].label = "MODE";
        commonData->keydata[2].label = "";
        commonData->keydata[3].label = "RST";
        commonData->keydata[4].label = "STBY";
        commonData->keydata[5].label = "ILUM";
    }

    int handleKey(int key) {
        // do *NOT* handle key #1 this handled by obp60task as exit
        // Switch display mode
        logger->logDebug(GwLog::LOG, "System keyboard handler");
        if (key == 2) {
            incMode();
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
            logger->logDebug(GwLog::LOG, "System going into deep sleep mode...");
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
            logger->logDebug(GwLog::LOG, "System going into deep sleep mode...");
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
                    epd->fillRect(x, y, s, s, commonData->fgcolor);
                }
                x += s;
            }
            y += s;
            x = x0;
        }
    }

    void displayNew(PageData &pageData) {
        // Get references from API
        logger->logDebug(GwLog::LOG, "New page display: PageSystem");
        NMEA2000 = pageData.api->getNMEA2000();
    };

    int displayPage(PageData &pageData) {

        // Optical warning by limit violation (unused)
        if(flashLED == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging page information
        logger->logDebug(GwLog::LOG, "Drawing at PageSystem, Mode=%c", mode);

        // Set display in partial refresh mode
        epd->setPartialWindow(0, 0, epd->width(), epd->height());

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
        epd->nextPage();    // Partial update (fast)
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
