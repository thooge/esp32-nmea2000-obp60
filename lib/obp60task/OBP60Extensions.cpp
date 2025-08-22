// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include <Arduino.h>
#include <PCF8574.h>      // Driver for PCF8574 output modul from Horter
#include <Wire.h>         // I2C
#include <RTClib.h>       // Driver for DS1388 RTC
#include "SunRise.h"      // Lib for sunrise and sunset calculation
#include "Pagedata.h"
#include "OBP60Hardware.h"
#include "OBP60Extensions.h"
#include "imglib.h"

// Character sets
#include "fonts/DSEG7Classic-BoldItalic16pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic20pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic26pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic30pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic42pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic60pt7b.h"
#include "fonts/Ubuntu_Bold8pt8b.h"
#include "fonts/Ubuntu_Bold10pt8b.h"
#include "fonts/Ubuntu_Bold12pt8b.h"
#include "fonts/Ubuntu_Bold16pt8b.h"
#include "fonts/Ubuntu_Bold20pt8b.h"
#include "fonts/Ubuntu_Bold32pt8b.h"
#include "fonts/Atari16px8b.h" // Key label font

// E-Ink Display
#define GxEPD_WIDTH 400     // Display width
#define GxEPD_HEIGHT 300    // Display height

#ifdef DISPLAY_GDEW042T2
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
#endif
#ifdef DISPLAY_GDEY042T81
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
#endif
#ifdef DISPLAY_GYE042A87
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> display(GxEPD2_420_GYE042A87(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
#endif
#ifdef DISPLAY_SE0420NQ04
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> display(GxEPD2_420_SE0420NQ04(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
#endif
gxepd2display *epd = &display;

// Horter I2C moduls
PCF8574 pcf8574_Out(PCF8574_I2C_ADDR1); // First digital output modul PCF8574 from Horter

// FRAM
Adafruit_FRAM_I2C fram;
bool hasFRAM = false;

// SD Card
#ifdef BOARD_OBP40S3
sdmmc_card_t *sdcard;
#endif
bool hasSDCard = false;

// Global vars
bool heartbeat = false;         // Heartbeat indicator with two different states
bool blinkingLED = false;       // Enable / disable blinking flash LED
bool statusLED = false;         // Actual status of flash LED on/off
bool statusBacklightLED = false;// Actual status of flash LED on/off

int uvDuration = 0;             // Under voltage duration in n x 100ms

RTC_DATA_ATTR uint8_t RTC_lastpage; // Remember last page while deep sleeping


LedTaskData *ledTaskData=nullptr;

void hardwareInit(GwApi *api)
{
    GwLog *logger = api->getLogger();
    GwConfigHandler *config = api->getConfig();

    Wire.begin();
    // Init PCF8574 digital outputs
    Wire.setClock(I2C_SPEED);       // Set I2C clock on 10 kHz
    if(pcf8574_Out.begin()){        // Initialize PCF8574
        pcf8574_Out.write8(255);    // Clear all outputs
    }
    fram = Adafruit_FRAM_I2C();
    if (esp_reset_reason() ==  ESP_RST_POWERON) {
        // help initialize FRAM
        logger->logDebug(GwLog::LOG, "Delaying I2C init for 250ms due to cold boot");
        delay(250);
    }
    // FRAM (e.g. MB85RC256V)
    if (fram.begin(FRAM_I2C_ADDR)) {
        hasFRAM = true;
        uint16_t manufacturerID;
        uint16_t productID;
        fram.getDeviceID(&manufacturerID, &productID);
        // Boot counter
        uint8_t framcounter = fram.read(0x0000);
        fram.write(0x0000, framcounter+1);
        logger->logDebug(GwLog::LOG, "FRAM detected: 0x%04x/0x%04x (counter=%d)", manufacturerID, productID, framcounter);
    }
    else {
        hasFRAM = false;
        logger->logDebug(GwLog::LOG, "NO FRAM detected");
    }
    // SD Card
    hasSDCard = false;
#ifdef BOARD_OBP40S3
    if (config->getBool(config->useSDCard)) {
        esp_err_t ret;
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = SPI3_HOST;
        logger->logDebug(GwLog::DEBUG, "SDSPI_HOST: max_freq_khz=%d" , host.max_freq_khz);
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = SD_SPI_MOSI,
            .miso_io_num = SD_SPI_MISO,
            .sclk_io_num = SD_SPI_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };
        ret = spi_bus_initialize((spi_host_device_t) host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            logger->logDebug(GwLog::ERROR, "Failed to initialize SPI bus for SD card");
        } else {
            sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
            slot_config.gpio_cs = SD_SPI_CS;
            slot_config.host_id = (spi_host_device_t) host.slot;
            esp_vfs_fat_sdmmc_mount_config_t mount_config = {
                .format_if_mount_failed = false,
                .max_files = 5,
                .allocation_unit_size = 16 * 1024
            };
            ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &sdcard);
            if (ret != ESP_OK) {
                if (ret == ESP_FAIL) {
                    logger->logDebug(GwLog::ERROR, "Failed to mount SD card filesystem");
                } else {
                    // ret == 263 could be not powered up yet
                    logger->logDebug(GwLog::ERROR, "Failed to initialize SD card (error #%d)", ret);
                }
            } else {
                logger->logDebug(GwLog::LOG, "SD card filesystem mounted at '%s'", MOUNT_POINT);
                hasSDCard = true;
            }
        }
        if (hasSDCard) {
            // read some stats
            String features = "";
            if (sdcard->is_mem) features += "MEM "; // Memory card
            if (sdcard->is_sdio) features += "IO "; // IO Card
            if (sdcard->is_mmc) features += "MMC "; // MMC Card
            if (sdcard->is_ddr) features += "DDR ";
            // if (sdcard->is_uhs1) features += "UHS-1 ";
            // ext_csd. Extended information
            // uint8_t rev, uint8_t power_class
            logger->logDebug(GwLog::LOG, "SD card features: %s", features);
            logger->logDebug(GwLog::LOG, "SD card size: %lluMB", ((uint64_t) sdcard->csd.capacity) * sdcard->csd.sector_size / (1024 * 1024));
        }
    }
#endif
}

void powerInit(String powermode) {
    // Max Power | Only 5.0V | Min Power
    if (powermode == "Max Power" || powermode == "Only 5.0V") {
#ifdef HARDWARE_V21
        setPortPin(OBP_POWER_50, true); // Power on 5.0V rail
#endif
#ifdef BOARD_OBP40S3
        setPortPin(OBP_POWER_EPD, true);// Power on ePaper display
        setPortPin(OBP_POWER_SD, true); // Power on SD card
#endif
    } else { // Min Power
#ifdef HARDWARE_V21
        setPortPin(OBP_POWER_50, false); // Power off 5.0V rail
#endif
#ifdef BOARD_OBP40S3
        setPortPin(OBP_POWER_EPD, false);// Power off ePaper display
        setPortPin(OBP_POWER_SD, false); // Power off SD card
#endif
    }
}

void setPortPin(uint pin, bool value){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
}

void togglePortPin(uint pin){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, !digitalRead(pin));
}

void startLedTask(GwApi *api){
    ledTaskData=new LedTaskData(api);
    createSpiLedTask(ledTaskData);
}

uint8_t getLastPage() {
    return RTC_lastpage;
}

#ifdef BOARD_OBP60S3
void deepSleep(CommonData &common){
    RTC_lastpage = common.data.actpage - 1;
    // Switch off all power lines
    setPortPin(OBP_BACKLIGHT_LED, false);   // Backlight Off
    setFlashLED(false);                     // Flash LED Off
    buzzer(TONE4, 20);                      // Buzzer tone 4kHz 20ms
    // Shutdown EInk display
    epd->setFullWindow();               // Set full Refresh
    epd->fillScreen(common.bgcolor);    // Clear screen
    epd->setTextColor(common.fgcolor);
    epd->setFont(&Ubuntu_Bold20pt8b);
    epd->setCursor(85, 150);
    epd->print("Sleep Mode");
    epd->setFont(&Ubuntu_Bold8pt8b);
    epd->setCursor(65, 175);
    epd->print("To wake up press key and wait 5s");
    epd->nextPage();                // Update display contents
    epd->powerOff();                // Display power off
    setPortPin(OBP_POWER_50, false);        // Power off ePaper display
    // Stop system
    esp_deep_sleep_start();                 // Deep Sleep with weakup via touch pin
}
#endif
#ifdef BOARD_OBP40S3
// Deep sleep funktion
void deepSleep(CommonData &common){
    RTC_lastpage = common.data.actpage - 1;
    // Switch off all power lines
    setPortPin(OBP_BACKLIGHT_LED, false);   // Backlight Off
    setFlashLED(false);                     // Flash LED Off
    // Shutdown EInk display
    epd->setFullWindow();               // Set full Refresh
    //epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
    epd->fillScreen(common.bgcolor);    // Clear screen
    epd->setTextColor(common.fgcolor);
    epd->setFont(&Ubuntu_Bold20pt8b);
    epd->setCursor(85, 150);
    epd->print("Sleep Mode");
    epd->setFont(&Ubuntu_Bold8pt8b);
    epd->setCursor(65, 175);
    epd->print("To wake up press wheel and wait 5s");
    epd->nextPage();                // Partial update
    epd->powerOff();                // Display power off
    setPortPin(OBP_POWER_EPD, false);       // Power off ePaper display
    setPortPin(OBP_POWER_SD, false);        // Power off SD card
    // Stop system
    esp_deep_sleep_start();             // Deep Sleep with weakup via GPIO pin
}
#endif

// Valid colors see hue
Color colorMapping(const String &colorString){
    Color color = COLOR_RED;
    if(colorString == "Orange"){color = Color(255,153,0);}
    if(colorString == "Yellow"){color = Color(255,255,0);}
    if(colorString == "Green"){color = COLOR_GREEN;}
    if(colorString == "Blue"){color = COLOR_BLUE;}
    if(colorString == "Aqua"){color = Color(51,102,255);}
    if(colorString == "Violet"){color = Color(255,0,102);}
    if(colorString == "White"){color = COLOR_WHITE;}
    return color;
}

BacklightMode backlightMapping(const String &backlightString) {
    static std::map<String, BacklightMode> const table = {
        {"Off", BacklightMode::OFF},
        {"Control by Bus", BacklightMode::BUS},
        {"Control by Time", BacklightMode::TIME},
        {"Control by Key", BacklightMode::KEY},
        {"On", BacklightMode::ON},
    };
    auto it = table.find(backlightString);
    if (it != table.end()) {
        return it->second;
    }
    return BacklightMode::OFF;
}

// All defined colors see pixeltypes.h in FastLED lib
void setBacklightLED(uint brightness, const Color &color){
    if (ledTaskData == nullptr) return;
    Color nv=setBrightness(color,brightness);
    LedInterface current=ledTaskData->getLedData();
    current.setBacklight(nv);
    ledTaskData->setLedData(current);    
}

void toggleBacklightLED(uint brightness, const Color &color) {
    if (ledTaskData == nullptr) return;
    statusBacklightLED = !statusBacklightLED;
    Color nv = setBrightness(statusBacklightLED ? color : COLOR_BLACK, brightness);
    LedInterface current = ledTaskData->getLedData();
    current.setBacklight(nv);
    ledTaskData->setLedData(current); 
}

void setFlashLED(bool status) {
    if (ledTaskData == nullptr) return;
    Color c = status ? COLOR_RED : COLOR_BLACK;
    LedInterface current = ledTaskData->getLedData();
    current.setFlash(c);
    ledTaskData->setLedData(current);
}

void blinkingFlashLED() {
    if (blinkingLED == true) {
        statusLED = !statusLED;     // Toggle LED for each run
        setFlashLED(statusLED);
    }
}

void setBlinkingLED(bool status) {
    blinkingLED = status;
}

uint buzzerpower = 50;

void buzzer(uint frequency, uint duration){
    if(frequency > 8000){   // Max 8000Hz
        frequency = 8000;
    }
    if(buzzerpower > 100){  // Max 100%
        buzzerpower = 100;
    }
    if(duration > 1000){    // Max 1000ms
        duration = 1000;
    }
    
    // Using LED PWM function for sound generation
    pinMode(OBP_BUZZER, OUTPUT);
    ledcSetup(0, frequency, 8);         // Ch 0, ferquency in Hz, 8 Bit resolution of PWM
    ledcAttachPin(OBP_BUZZER, 0);
    ledcWrite(0, uint(buzzerpower * 1.28));    // 50% duty cycle are 100%
    delay(duration);
    ledcWrite(0, 0);                    // 0% duty cycle are 0%
}

void setBuzzerPower(uint power){
    buzzerpower = power;
}

// Delete xdr prefix from string
String xdrDelete(String input){
    if(input.substring(0,3) == "xdr"){
        input = input.substring(3, input.length());
    }
    return input;
}

void fillPoly4(const std::vector<Point>& p4, uint16_t color) {
    epd->fillTriangle(p4[0].x, p4[0].y, p4[1].x, p4[1].y, p4[2].x, p4[2].y, color);
    epd->fillTriangle(p4[0].x, p4[0].y, p4[2].x, p4[2].y, p4[3].x, p4[3].y, color);
}

void drawPoly(const std::vector<Point>& points, uint16_t color) {
    size_t polysize = points.size();
    for (size_t i = 0; i < polysize - 1; i++) {
        epd->drawLine(points[i].x, points[i].y, points[i+1].x, points[i+1].y, color);
    }
    // close path
    epd->drawLine(points[polysize-1].x, points[polysize-1].y, points[0].x, points[0].y, color);
}

// Split string into words, whitespace separated
std::vector<String> split(const String &s) {
    std::vector<String> words;
    String word = "";
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')  {
            if (word.length() > 0) {
                words.push_back(word);
                word = "";
            }
        } else {
             word += s[i];
        }
    }
    if (word.length() > 0) {
        words.push_back(word);
    }
    return words;
}

// Wordwrap single line, monospaced font
std::vector<String> wordwrap(String &line, uint16_t maxwidth) {
    std::vector<String> lines;
    std::vector<String> words = split(line);
    String currentLine = "";
    for (const auto& word : words) {
        if (currentLine.length() + word.length() + 1 > maxwidth) {
             if (currentLine.length() > 0) {
                  lines.push_back(currentLine);
                  currentLine = "";
             }
        }
        if (currentLine.length() > 0) {
            currentLine += " ";
        }
        currentLine += word;
    }
    if (currentLine.length() > 0) {
        lines.push_back(currentLine);
    }
    return lines;
}

// Draw centered text
void drawTextCenter(int16_t cx, int16_t cy, String text) {
    int16_t x1, y1;
    uint16_t w, h;
    epd->getTextBounds(text, 0, 150, &x1, &y1, &w, &h);
    epd->setCursor(cx - w / 2, cy + h / 2);
    epd->print(text);
}

// Draw right aligned text
void drawTextRalign(int16_t x, int16_t y, String text) {
    int16_t x1, y1;
    uint16_t w, h;
    epd->getTextBounds(text, 0, 150, &x1, &y1, &w, &h);
    epd->setCursor(x - w, y);
    epd->print(text);
}

// Draw text inside box, normal or inverted
void drawTextBoxed(Rect box, String text, uint16_t fg, uint16_t bg, bool inverted, bool border) {
    if (inverted) {
        epd->fillRect(box.x, box.y, box.w, box.h, fg);
        epd->setTextColor(bg);
    } else {
        if (border) {
            epd->fillRect(box.x + 1, box.y + 1, box.w - 2, box.h - 2, bg);
            epd->drawRect(box.x, box.y, box.w, box.h, fg);
        }
        epd->setTextColor(fg);
    }
    uint16_t border_offset = box.h / 4; // 25% of box height
    epd->setCursor(box.x + border_offset, box.y + box.h - border_offset);
    epd->print(text);
    epd->setTextColor(fg);
}

// Show a triangle for trend direction high (x, y is the left edge)
void displayTrendHigh(int16_t x, int16_t y, uint16_t size, uint16_t color){
    epd->fillTriangle(x, y, x+size*2, y, x+size, y-size*2, color);
}

// Show a triangle for trend direction low (x, y is the left edge)
void displayTrendLow(int16_t x, int16_t y, uint16_t size, uint16_t color){
    epd->fillTriangle(x, y, x+size*2, y, x+size, y+size*2, color);
}

// Show header informations
void displayHeader(CommonData &commonData, bool symbolmode, GwApi::BoatValue *date, GwApi::BoatValue *time, GwApi::BoatValue *hdop){

    static unsigned long usbRxOld = 0;
    static unsigned long usbTxOld = 0;
    static unsigned long serRxOld = 0;
    static unsigned long serTxOld = 0;
    static unsigned long tcpSerRxOld = 0;
    static unsigned long tcpSerTxOld = 0;
    static unsigned long tcpClRxOld = 0;
    static unsigned long tcpClTxOld = 0;
    static unsigned long n2kRxOld = 0;
    static unsigned long n2kTxOld = 0;

    uint16_t symbol_x = 2;
    static const uint16_t symbol_offset = 20;

    // TODO invert and get rid of the if
    if(commonData.config->getBool(commonData.config->statusLine) == true){

        // Show status info
        epd->setTextColor(commonData.fgcolor);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(0, 15);
        if (commonData.status.wifiApOn) {
            if (symbolmode) {
                epd->drawXBitmap(symbol_x, 1, iconmap["AP"], icon_width, icon_height, commonData.fgcolor);
                symbol_x += symbol_offset;
            } else {
                epd->print(" AP ");
            }
        }
        // If receive new telegram data then display bus name
        if(commonData.status.tcpClRx != tcpClRxOld || commonData.status.tcpClTx != tcpClTxOld || commonData.status.tcpSerRx != tcpSerRxOld || commonData.status.tcpSerTx != tcpSerTxOld){
            if (symbolmode) {
                epd->drawXBitmap(symbol_x, 1, iconmap["TCP"], icon_width, icon_height, commonData.fgcolor);
                symbol_x += symbol_offset;
            } else {
                epd->print("TCP ");
            }
        }
        if(commonData.status.n2kRx != n2kRxOld || commonData.status.n2kTx != n2kTxOld){
            if (symbolmode) {
                epd->drawXBitmap(symbol_x, 1, iconmap["N2K"], icon_width, icon_height, commonData.fgcolor);
                symbol_x += symbol_offset;
            } else {
                epd->print("N2K ");
            }
        }
        if(commonData.status.serRx != serRxOld || commonData.status.serTx != serTxOld){
            if (symbolmode) {
                epd->drawXBitmap(symbol_x, 1, iconmap["0183"], icon_width, icon_height, commonData.fgcolor);
                symbol_x += symbol_offset;
            } else {
                epd->print("183 ");
            }
        }
        if(commonData.status.usbRx != usbRxOld || commonData.status.usbTx != usbTxOld){
            if (symbolmode) {
                epd->drawXBitmap(symbol_x, 1, iconmap["USB"], icon_width, icon_height, commonData.fgcolor);
                symbol_x += symbol_offset;
            } else {
                epd->print("USB ");
            }
        }
        double gpshdop = commonData.fmt->formatValue(hdop, commonData).value;
        if(commonData.config->getString(commonData.config->useGPS) != "off" &&  gpshdop <= commonData.config->getInt(commonData.config->hdopAccuracy) && hdop->valid == true){
            if (symbolmode) {
                epd->drawXBitmap(symbol_x, 1, iconmap["GPS"], icon_width, icon_height, commonData.fgcolor);
                symbol_x += symbol_offset;
            } else {
                epd->print("GPS");
            }
        }
        // Save old telegram counter
        tcpClRxOld = commonData.status.tcpClRx;
        tcpClTxOld = commonData.status.tcpClTx;
        tcpSerRxOld = commonData.status.tcpSerRx;
        tcpSerTxOld = commonData.status.tcpSerTx;
        n2kRxOld = commonData.status.n2kRx;
        n2kTxOld = commonData.status.n2kTx;
        serRxOld = commonData.status.serRx;
        serTxOld = commonData.status.serTx;
        usbRxOld = commonData.status.usbRx;
        usbTxOld = commonData.status.usbTx;

#ifdef HARDWARE_V21
        // Display key lock status
        if (commonData.keylock) {
            epd->drawXBitmap(170, 1, lock_bits, icon_width, icon_height, commonData.fgcolor);
        } else {
            epd->drawXBitmap(166, 1, swipe_bits, swipe_width, swipe_height, commonData.fgcolor);
        }
#endif
#ifdef LIPO_ACCU_1200
        if (commonData.data.BatteryChargeStatus == 1) {
             epd->drawXBitmap(170, 1, battery_loading_bits, battery_width, battery_height, commonData.fgcolor);
        } else {
#ifdef VOLTAGE_SENSOR
            if (commonData.data.batteryLevelLiPo < 10) {
                epd->drawXBitmap(170, 1, battery_0_bits, battery_width, battery_height, commonData.fgcolor);
            } else if (commonData.data.batteryLevelLiPo < 25) {
                epd->drawXBitmap(170, 1, battery_25_bits, battery_width, battery_height, commonData.fgcolor);
            } else if (commonData.data.batteryLevelLiPo < 50) {
                epd->drawXBitmap(170, 1, battery_50_bits, battery_width, battery_height, commonData.fgcolor);
            } else if (commonData.data.batteryLevelLiPo < 75) {
                epd->drawXBitmap(170, 1, battery_75_bits, battery_width, battery_height, commonData.fgcolor);
            } else {
                epd->drawXBitmap(170, 1, battery_100_bits, battery_width, battery_height, commonData.fgcolor);
            }
#endif // VOLTAGE_SENSOR
        }
#endif // LIPO_ACCU_1200

        // Heartbeat as page number
        if (heartbeat) {
            epd->setTextColor(commonData.bgcolor);
            epd->fillRect(201, 0, 23, 19, commonData.fgcolor);
        } else {
            epd->setTextColor(commonData.fgcolor);
            epd->drawRect(201, 0, 23, 19, commonData.fgcolor);
        }
        epd->setFont(&Ubuntu_Bold8pt8b);
        drawTextCenter(211, 9, String(commonData.data.actpage));
        heartbeat = !heartbeat;

        // Date and time
        String fmttype = commonData.config->getString(commonData.config->dateFormat);
        String timesource = commonData.config->getString(commonData.config->timeSource);
        double tz = commonData.config->getString(commonData.config->timeZone).toDouble();
        epd->setTextColor(commonData.fgcolor);
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(230, 15);
        if (timesource == "RTC" or timesource == "iRTC") {
            // TODO take DST into account
            if (commonData.data.rtcValid) {
                time_t tv = mktime(&commonData.data.rtcTime) + (int)(tz * 3600);
                struct tm *local_tm = localtime(&tv);
                epd->print(formatTime('m', local_tm->tm_hour, local_tm->tm_min, 0));
                epd->print(" ");
                epd->print(formatDate(fmttype, local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday));
                epd->print(" ");
                epd->print(tz == 0 ? "UTC" : "LOT");
            } else {
                drawTextRalign(396, 15, "RTC invalid");
            }
        }
        else if (timesource == "GPS") {
            // Show date and time if date present
            if(date->valid == true){
                String acttime = commonData.fmt->formatValue(time, commonData).svalue;
                acttime = acttime.substring(0, 5);
                String actdate = commonData.fmt->formatValue(date, commonData).svalue;
                epd->print(acttime);
                epd->print(" ");
                epd->print(actdate);
                epd->print(" ");
                epd->print(tz == 0 ? "UTC" : "LOT");
            }
            else{
                if(commonData.config->getBool(commonData.config->useSimuData) == true){
                    epd->print("12:00 01.01.2024 LOT");
                }
                else{
                    drawTextRalign(396, 15, "No GPS data");
                }
            }
        } // timesource == "GPS"
        else {
            epd->print("No time source");
        }
    }
}

void displayFooter(CommonData &commonData) {

    epd->setFont(&Atari16px);
    epd->setTextColor(commonData.fgcolor);

#ifdef HARDWARE_V21
    // Frame around key icon area
    if (! commonData.keylock) {
        // horizontal elements
        const uint16_t top = 280;
        const uint16_t bottom = 299;
        // horizontal stub lines
        epd->drawLine(commonData.keydata[0].x, top, commonData.keydata[0].x+10, top, commonData.fgcolor);
        for (int i = 1; i <= 5; i++) {
            epd->drawLine(commonData.keydata[i].x-10, top, commonData.keydata[i].x+10, top, commonData.fgcolor);
        }
        epd->drawLine(commonData.keydata[5].x + commonData.keydata[5].w - 10, top, commonData.keydata[5].x + commonData.keydata[5].w + 1, top, commonData.fgcolor);
        // vertical key separators
        for (int i = 0; i <= 4; i++) {
            epd->drawLine(commonData.keydata[i].x + commonData.keydata[i].w, top, commonData.keydata[i].x + commonData.keydata[i].w, bottom, commonData.fgcolor);
        }
        for (int i = 0; i < 6; i++) {
            uint16_t x, y;
            if (commonData.keydata[i].label.length() > 0) {
                // check if icon is enabled
                String icon_name = commonData.keydata[i].label.substring(1);
                if (commonData.keydata[i].label[0] == '#') {
                    if (iconmap.find(icon_name) != iconmap.end()) {
                        x = commonData.keydata[i].x + (commonData.keydata[i].w - icon_width) / 2;
                        y = commonData.keydata[i].y + (commonData.keydata[i].h - icon_height) / 2;
                        epd->drawXBitmap(x, y, iconmap[icon_name], icon_width, icon_height, commonData.fgcolor);
                    } else {
                        // icon is missing, use name instead
                        x = commonData.keydata[i].x + commonData.keydata[i].w / 2;
                        y = commonData.keydata[i].y + commonData.keydata[i].h / 2;
                        drawTextCenter(x, y, icon_name);
                    }
                } else {
                    x = commonData.keydata[i].x + commonData.keydata[i].w / 2;
                    y = commonData.keydata[i].y + commonData.keydata[i].h / 2;
                    drawTextCenter(x, y, commonData.keydata[i].label);
                }
            }
        }
    } else {
        epd->setCursor(65, 295);
        epd->print("Press 1 and 6 fast to unlock keys");
    }
#endif
#ifdef BOARD_OBP40S3
    // grapical page indicator
    static const uint16_t r = 5;
    static const uint16_t space = 4;
    uint16_t w = commonData.data.maxpage * r * 2 + (commonData.data.maxpage - 1) * space;
    uint16_t x0 = (GxEPD_WIDTH - w) / 2 + r * 2;
    for (int i = 0; i < commonData.data.maxpage; i++) {
        if (i == (commonData.data.actpage - 1)) {
            epd->fillCircle(x0 + i * (r * 2 + space), 290, r, commonData.fgcolor);
        } else {
            epd->drawCircle(x0 + i * (r * 2 + space), 290, r, commonData.fgcolor);
        }
    }
    // key indicators
    // left side = top key "menu"
    epd->drawLine(0, 280, 10, 280, commonData.fgcolor);
    epd->drawLine(55, 280, 65, 280, commonData.fgcolor);
    epd->drawLine(65, 280, 65, 299, commonData.fgcolor);
    drawTextCenter(33, 291, commonData.keydata[0].label);
    // right side = bottom key "exit"
    epd->drawLine(390, 280, 399, 280, commonData.fgcolor);
    epd->drawLine(335, 280, 345, 280, commonData.fgcolor);
    epd->drawLine(335, 280, 335, 399, commonData.fgcolor);
    drawTextCenter(366, 291, commonData.keydata[1].label);
#endif
}

// Alarm overlay, to be drawn as very last draw operation
void displayAlarm(CommonData &commonData) {

    const uint16_t x = 50;   // overlay area
    const uint16_t y = 100;
    const uint16_t w = 300;
    const uint16_t h = 150;

    epd->setFont(&Atari16px);
    epd->setTextColor(commonData.fgcolor);

    // overlay
    epd->drawRect(x, y, w, h, commonData.fgcolor);
    epd->fillRect(x + 1, y + 1, w - 1, h - 1, commonData.bgcolor);
    epd->drawRect(x + 3, y + 3, w - 5, h - 5, commonData.fgcolor);

    // exclamation icon in left top corner
    epd->drawXBitmap(x + 16, y + 16, exclamation_bits, exclamation_width, exclamation_height, commonData.fgcolor);

    // title
    epd->setCursor(x + 64, y + 30);
    epd->print("A L A R M");
    epd->setCursor(x + 64, y + 48);
    epd->print("#" + commonData.alarm.id);
    epd->print(" from ");
    epd->print(commonData.alarm.source);

    // message, but maximum 4 lines
    std::vector<String> lines = wordwrap (commonData.alarm.message, w - 16 - 8 / 8);
    int n = 0;
    for (const auto& l : lines) {
        epd->setCursor(x + 16, y + 80 + n);
        epd->print(l);
        n += 16;
        if (n > 64) {
            break;
        }
    }
    drawTextCenter(x + w / 2,  y + h - 16, "Press button 1 to dismiss alarm");
}

// Sunset und sunrise calculation
SunData calcSunsetSunrise(double time, double date, double latitude, double longitude, float timezone){
    SunData returnset;
    SunRise sr;
    int secPerHour = 3600;
    int secPerYear = 86400;
    sr.hasRise = false;
    sr.hasSet = false;
    time_t t = 0;
    time_t sunR = 0;
    time_t sunS = 0;

    if (!isnan(time) && !isnan(date) && !isnan(latitude) && !isnan(longitude) && !isnan(timezone)) {
        
        // Calculate local epoch
        t = (date * secPerYear) + time;
        sr.calculate(latitude, longitude, t);       // LAT, LON, EPOCH
        // Sunrise
        if (sr.hasRise) {
            sunR = (sr.riseTime + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
            returnset.sunriseHour = int (sunR / secPerHour);
            returnset.sunriseMinute = int((sunR - returnset.sunriseHour * secPerHour)/60);
        }
        // Sunset
        if (sr.hasSet)  {
            sunS = (sr.setTime  + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
            returnset.sunsetHour = int (sunS / secPerHour);
            returnset.sunsetMinute = int((sunS - returnset.sunsetHour * secPerHour)/60);      
        }
        // Sun control (return value by sun on sky = false, sun down = true)
        if ((t >= sr.riseTime) && (t <= sr.setTime))      
             returnset.sunDown = false; 
        else returnset.sunDown = true;
    }
    // Return values
    return returnset;
}

SunData calcSunsetSunriseRTC(struct tm *rtctime, double latitude, double longitude, float timezone) {
    SunData returnset;
    SunRise sr;
    const int secPerHour = 3600;
    const int secPerYear = 86400;
    sr.hasRise = false;
    sr.hasSet = false;
    time_t t =  mktime(rtctime) + timezone * 3600;;
    time_t sunR = 0;
    time_t sunS = 0;

    sr.calculate(latitude, longitude, t);       // LAT, LON, EPOCH
    // Sunrise
    if (sr.hasRise) {
        sunR = (sr.riseTime + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
        returnset.sunriseHour = int (sunR / secPerHour);
        returnset.sunriseMinute = int((sunR - returnset.sunriseHour * secPerHour) / 60);
    }
    // Sunset
    if (sr.hasSet) {
        sunS = (sr.setTime + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
        returnset.sunsetHour = int (sunS / secPerHour);
        returnset.sunsetMinute = int((sunS - returnset.sunsetHour * secPerHour) / 60);
    }
    // Sun control (return value by sun on sky = false, sun down = true)
    if ((t >= sr.riseTime) && (t <= sr.setTime))
         returnset.sunDown = false;
    else returnset.sunDown = true;
    return returnset;
}

// Battery graphic with fill level
void batteryGraphic(uint x, uint y, float percent, int pcolor, int bcolor){
    // Show battery
        int xb = x;     // X position
        int yb = y;     // Y position
        int t = 4;      // Line thickness
        // Percent limits
        if(percent < 0){
            percent = 0;
        }
         if(percent > 99){
            percent = 99;
        }
        // Battery corpus 100x80 with fill level
        int level = int((100.0 - percent) * (80-(2*t)) / 100.0);
        epd->fillRect(xb, yb, 100, 80, pcolor);
        if(percent < 99){
            epd->fillRect(xb+t, yb+t, 100-(2*t), level, bcolor);
        }
        // Plus pol 20x15
        int xp = xb + 20;
        int yp = yb - 15 + t;
        epd->fillRect(xp, yp, 20, 15, pcolor);
        epd->fillRect(xp+t, yp+t, 20-(2*t), 15-(2*t), bcolor);
        // Minus pol 20x15
        int xm = xb + 60;
        int ym = yb -15 + t;
        epd->fillRect(xm, ym, 20, 15, pcolor);
        epd->fillRect(xm+t, ym+t, 20-(2*t), 15-(2*t), bcolor);
}

// Solar graphic with fill level
void solarGraphic(uint x, uint y, int pcolor, int bcolor){
    // Show solar modul
        int xb = x;     // X position
        int yb = y;     // Y position
        int t = 4;      // Line thickness
        int percent = 0;
        // Solar corpus 100x80
        int level = int((100.0 - percent) * (80-(2*t)) / 100.0);
        epd->fillRect(xb, yb, 100, 80, pcolor);
        if(percent < 99){
            epd->fillRect(xb+t, yb+t, 100-(2*t), level, bcolor);
        }
        // Draw horizontel lines
        epd->fillRect(xb, yb+28-t, 100, t, pcolor);
        epd->fillRect(xb, yb+54-t, 100, t, pcolor);
        // Draw vertical lines
        epd->fillRect(xb+19+t, yb, t, 80, pcolor);
        epd->fillRect(xb+39+2*t, yb, t, 80, pcolor);
        epd->fillRect(xb+59+3*t, yb, t, 80, pcolor);

}

// Generator graphic with fill level
void generatorGraphic(uint x, uint y, int pcolor, int bcolor){
    // Show battery
        int xb = x;     // X position
        int yb = y;     // Y position
        int t = 4;      // Line thickness

        // Generator corpus with radius 45
        epd->fillCircle(xb, yb, 45, pcolor);
        epd->fillCircle(xb, yb, 41, bcolor);
        // Insert G
        epd->setTextColor(pcolor);
        epd->setFont(&Ubuntu_Bold32pt8b);
        epd->setCursor(xb-22, yb+20);
        epd->print("G");
}

// Function to handle HTTP image request
// http://192.168.15.1/api/user/OBP60Task/screenshot
void doImageRequest(GwApi *api, int *pageno, const PageStruct pages[MAX_PAGE_NUMBER], AsyncWebServerRequest *request) {
    GwLog *logger = api->getLogger();

    String imgformat = api->getConfig()->getConfigItem(api->getConfig()->imageFormat,true)->asString();
    imgformat.toLowerCase();
    String filename = "Page" + String(*pageno) + "_" +  pages[*pageno].description->pageName + "." + imgformat;

    logger->logDebug(GwLog::LOG,"handle image request [%s]: %s", imgformat, filename);

    uint8_t *fb = epd->getBuffer(); // EPD framebuffer
    std::vector<uint8_t> imageBuffer;       // image in webserver transferbuffer
    String mimetype;

    if (imgformat == "gif") {
        // GIF is commpressed with LZW, so small
        mimetype = "image/gif";
        if (!createGIF(fb, &imageBuffer, GxEPD_WIDTH, GxEPD_HEIGHT)) {
             logger->logDebug(GwLog::LOG,"GIF creation failed: Hashtable init error!");
             return;
        }
    }
    else if (imgformat == "bmp") {
        // Microsoft BMP bitmap
        mimetype = "image/bmp";
        createBMP(fb, &imageBuffer, GxEPD_WIDTH, GxEPD_HEIGHT);
    }
    else {
        // PBM simple portable bitmap
        mimetype = "image/x-portable-bitmap";
        createPBM(fb, &imageBuffer, GxEPD_WIDTH, GxEPD_HEIGHT);
    }

    AsyncWebServerResponse *response = request->beginResponse_P(200, mimetype, (const uint8_t*)imageBuffer.data(), imageBuffer.size());
    response->addHeader("Content-Disposition", "inline; filename=" + filename);
    request->send(response);

    imageBuffer.clear();
}

#endif
