// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3
#include "obp60task.h"
#include "Pagedata.h"                   // Data exchange for pages
#include "OBP60Hardware.h"              // PIN definitions
#include <Wire.h>                       // I2C connections
#include <MCP23017.h>                   // MCP23017 extension Port
#include <N2kTypes.h>                   // NMEA2000
#include <N2kMessages.h>
#include <NMEA0183.h>                   // NMEA0183
#include <NMEA0183Msg.h>
#include <NMEA0183Messages.h>
#include <GxEPD2_BW.h>                  // GxEPD2 lib for b/w E-Ink displays
#include "OBP60Extensions.h"            // Functions lib for extension board
#include "OBP60Keypad.h"                // Functions for keypad
#include "BoatDataCalibration.h"        // Functions lib for data instance calibration
#include "OBPRingBuffer.h"              // Functions lib with ring buffer for history storage of some boat data
#include "OBPDataOperations.h"          // Functions lib for data operations such as true wind calculation

#ifdef BOARD_OBP40S3
#include "driver/rtc_io.h"              // Needs for weakup from deep sleep
#include <SPI.h>
#endif

// Pictures
#include "images/OBP_400x300.xbm"       // OBP Logo
#include "images/unknown.xbm"           // unknown page indicator
#include "OBP60QRWiFi.h"                // Functions lib for WiFi QR code
#include "OBPSensorTask.h"              // Functions lib for sensor data

// Global vars
bool initComplete = false;      // Initialization complete
int taskRunCounter = 0;         // Task couter for loop section

// Hardware initialization before start all services
//####################################################################################
void OBP60Init(GwApi *api){

    GwLog *logger = api->getLogger();
    GwConfigHandler *config = api->getConfig();

    // Set a new device name and hidden the original name in the main config
    String devicename = config->getConfigItem(config->deviceName, true)->asString();
    config->setValue(GwConfigDefinitions::systemName, devicename, GwConfigInterface::ConfigType::HIDDEN);

    logger->prefix = devicename + ":";
    logger->logDebug(GwLog::LOG,"obp60init running");

    // Check I2C devices

    // Init power
    String powermode = config->getConfigItem(config->powerMode,true)->asString();
    logger->logDebug(GwLog::DEBUG, "Power Mode is: %s", powermode.c_str());
    powerInit(powermode);

    // Init hardware
    hardwareInit(api);

#ifdef BOARD_OBP40S3
    // Deep sleep wakeup configuration
    esp_sleep_enable_ext0_wakeup(OBP_WAKEWUP_PIN, 0);   // 1 = High, 0 = Low
    rtc_gpio_pullup_en(OBP_WAKEWUP_PIN);                // Activate pullup resistor
    rtc_gpio_pulldown_dis(OBP_WAKEWUP_PIN);             // Disable pulldown resistor
#endif

    // Settings for e-paper display
    String fastrefresh = api->getConfig()->getConfigItem(api->getConfig()->fastRefresh,true)->asString();
    logger->logDebug(GwLog::DEBUG,"Fast Refresh Mode is: %s", fastrefresh.c_str());
    #ifdef DISPLAY_GDEY042T81
    if(fastrefresh == "true"){
        static const bool useFastFullUpdate = true;   // Enable fast full display update only for GDEY042T81
    }
    #endif

    #ifdef BOARD_OBP60S3
    touchSleepWakeUpEnable(TP1, 45); // TODO sensitivity should be configurable via web interface
    touchSleepWakeUpEnable(TP2, 45);
    touchSleepWakeUpEnable(TP3, 45);
    touchSleepWakeUpEnable(TP4, 45);
    touchSleepWakeUpEnable(TP5, 45);
    touchSleepWakeUpEnable(TP6, 45);
    esp_sleep_enable_touchpad_wakeup();
    #endif

    // Get CPU speed
    int freq = getCpuFrequencyMhz();
    logger->logDebug(GwLog::LOG,"CPU speed at boot: %i MHz", freq);
    
    // Settings for backlight
    String backlightMode = api->getConfig()->getConfigItem(api->getConfig()->backlight,true)->asString();
    logger->logDebug(GwLog::DEBUG,"Backlight Mode is: %s", backlightMode.c_str());
    uint brightness = uint(api->getConfig()->getConfigItem(api->getConfig()->blBrightness,true)->asInt());
    String backlightColor = api->getConfig()->getConfigItem(api->getConfig()->blColor,true)->asString();
    if(String(backlightMode) == "On"){
           setBacklightLED(brightness, colorMapping(backlightColor));
    }
    else if(String(backlightMode) == "Off"){
           setBacklightLED(0, COLOR_BLACK); // Backlight LEDs off (blue without britghness)
    }
    else if(String(backlightMode) == "Control by Key"){
           setBacklightLED(0, COLOR_BLUE); // Backlight LEDs off (blue without britghness)
    }

    // Settings flash LED mode
    String ledMode = api->getConfig()->getConfigItem(api->getConfig()->flashLED,true)->asString();
    logger->logDebug(GwLog::DEBUG,"LED Mode is: %s", ledMode.c_str());
    if(String(ledMode) == "Off"){
        setBlinkingLED(false);
    }

    // Marker for init complete
    // Used in OBP60Task()
    initComplete = true;

    // Buzzer tone for initialization finish
    setBuzzerPower(uint(api->getConfig()->getConfigItem(api->getConfig()->buzzerPower,true)->asInt()));
    buzzer(TONE4, 500);

}

typedef struct {
        int page0=0;
        QueueHandle_t queue;
        GwLog* logger = NULL;
//        GwApi* api = NULL;
        uint sensitivity = 100;
        bool use_syspage = true;
    } MyData;

// Keyboard Task
void keyboardTask(void *param){
    MyData *data=(MyData *)param;

    int keycode = 0;
    data->logger->logDebug(GwLog::LOG,"Start keyboard task");

    // Loop for keyboard task
    while (true){
        keycode = readKeypad(data->logger, data->sensitivity, data->use_syspage);
        //send a key event
        if(keycode != 0){
            xQueueSend(data->queue, &keycode, 0);
            data->logger->logDebug(GwLog::LOG,"Send keycode: %d", keycode);
        }
        delay(20);      // 50Hz update rate (20ms)
    }
    vTaskDelete(NULL);
}

class BoatValueList{
    public:
    static const int MAXVALUES=100;
    //we create a list containing all our BoatValues
    //this is the list we later use to let the api fill all the values
    //additionally we put the necessary values into the paga data - see below
    GwApi::BoatValue *allBoatValues[MAXVALUES];
    int numValues=0;
    
    bool addValueToList(GwApi::BoatValue *v){
        for (int i=0;i<numValues;i++){
            if (allBoatValues[i] == v){
                //already in list...
                return true;
            }
        }
        if (numValues >= MAXVALUES) return false;
        allBoatValues[numValues]=v;
        numValues++;
        return true;
    }
    //helper to ensure that each BoatValue is only queried once
    GwApi::BoatValue *findValueOrCreate(String name){
        for (int i=0;i<numValues;i++){
            if (allBoatValues[i]->getName() == name) {
                return allBoatValues[i];
            }
        }
        GwApi::BoatValue *rt=new GwApi::BoatValue(name);
        addValueToList(rt);
        return rt;
    }
};

//we want to have a list that has all our page definitions
//this way each page can easily be added here
//needs some minor tricks for the safe static initialization
typedef std::vector<PageDescription*> Pages;
//the page list class
class PageList{
    public:
        Pages pages;
        void add(PageDescription *p){
            pages.push_back(p);
        }
        PageDescription *find(String name){
            for (auto it=pages.begin();it != pages.end();it++){
                if ((*it)->pageName == name){
                    return *it;
                }
            }
            return NULL;
        }
};

/**
 * this function will add all the pages we know to the pagelist
 * each page should have defined a registerXXXPage variable of type
 * PageData that describes what it needs
 */
void registerAllPages(GwLog *logger, PageList &list){
    //the next line says that this variable is defined somewhere else
    //in our case in a separate C++ source file
    //this way this separate source file can be compiled by it's own
    //and has no access to any of our data except the one that we
    //give as a parameter to the page function
    logger->logDebug(GwLog::LOG, "Memory before registering pages: stack=%d, heap=%d", uxTaskGetStackHighWaterMark(NULL), ESP.getFreeHeap());
    extern PageDescription registerPageSystem;
    //we add the variable to our list
    list.add(&registerPageSystem);
    extern PageDescription registerPageOneValue;
    list.add(&registerPageOneValue);
    extern PageDescription registerPageTwoValues;
    list.add(&registerPageTwoValues);
    extern PageDescription registerPageThreeValues;
    list.add(&registerPageThreeValues);
    extern PageDescription registerPageSixValues;
    list.add(&registerPageSixValues);
    extern PageDescription registerPageFourValues;
    list.add(&registerPageFourValues);
    extern PageDescription registerPageFourValues2;
    list.add(&registerPageFourValues2);
    extern PageDescription registerPageWind;
    list.add(&registerPageWind);
    extern PageDescription registerPageWindPlot;
    list.add(&registerPageWindPlot); 
    extern PageDescription registerPageWindRose;
    list.add(&registerPageWindRose);
    extern PageDescription registerPageWindRoseFlex;
    list.add(&registerPageWindRoseFlex);
    extern PageDescription registerPageVoltage;
    list.add(&registerPageVoltage);
    extern PageDescription registerPageDST810;
    list.add(&registerPageDST810);
    extern PageDescription registerPageClock;
    list.add(&registerPageClock);
    extern PageDescription registerPageCompass;
    list.add(&registerPageCompass);
    extern PageDescription registerPageWhite;
    list.add(&registerPageWhite);
    extern PageDescription registerPageBME280;
    list.add(&registerPageBME280);
    extern PageDescription registerPageRudderPosition;
    list.add(&registerPageRudderPosition);
    extern PageDescription registerPageKeelPosition;
    list.add(&registerPageKeelPosition);
    extern PageDescription registerPageBattery;
    list.add(&registerPageBattery);
    extern PageDescription registerPageBattery2;
    list.add(&registerPageBattery2);
    extern PageDescription registerPageRollPitch;
    list.add(&registerPageRollPitch);
    extern PageDescription registerPageSolar;
    list.add(&registerPageSolar);
    extern PageDescription registerPageGenerator;
    list.add(&registerPageGenerator);
    extern PageDescription registerPageXTETrack;
    list.add(&registerPageXTETrack);
    extern PageDescription registerPageFluid;
    list.add(&registerPageFluid);
    extern PageDescription registerPageSkyView;
    list.add(&registerPageSkyView);
    extern PageDescription registerPageAnchor;
    list.add(&registerPageAnchor);
    extern PageDescription registerPageAIS;
    list.add(&registerPageAIS);
    logger->logDebug(GwLog::LOG,"Memory after registering pages: stack=%d, heap=%d", uxTaskGetStackHighWaterMark(NULL), ESP.getFreeHeap());
}

// Undervoltage detection for shutdown display
void underVoltageDetection(GwApi *api, CommonData &common){
    // Read settings
    double voffset = (api->getConfig()->getConfigItem(api->getConfig()->vOffset,true)->asString()).toFloat();
    double vslope = (api->getConfig()->getConfigItem(api->getConfig()->vSlope,true)->asString()).toFloat();
    // Read supply voltage
    #if defined VOLTAGE_SENSOR && defined LIPO_ACCU_1200
    float actVoltage = (float(analogRead(OBP_ANALOG0)) * 3.3 / 4096 + 0.53) * 2;   // Vin = 1/2 for OBP40
    float minVoltage = 3.65;  // Absolut minimum volatge for 3,7V LiPo accu
    #else
    float actVoltage = (float(analogRead(OBP_ANALOG0)) * 3.3 / 4096 + 0.17) * 20;   // Vin = 1/20 for OBP60
    float minVoltage = MIN_VOLTAGE;
    #endif
    double calVoltage = actVoltage * vslope + voffset;  // Calibration
    if(calVoltage < minVoltage){
        #if defined VOLTAGE_SENSOR && defined LIPO_ACCU_1200
        // Switch off all power lines
        setPortPin(OBP_BACKLIGHT_LED, false);   // Backlight Off
        setFlashLED(false);                     // Flash LED Off            
        buzzer(TONE4, 20);                      // Buzzer tone 4kHz 20ms
        // Shutdown EInk display
        epd->setFullWindow();           // Set full Refresh
        //epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
        epd->fillScreen(common.bgcolor);// Clear screen
        epd->setTextColor(common.fgcolor);
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(65, 150);
        epd->print("Undervoltage");
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(65, 175);
        epd->print("Charge battery and restart system");
        epd->nextPage();                // Partial update
        epd->powerOff();                // Display power off
        setPortPin(OBP_POWER_EPD, false);       // Power off ePaper display
        setPortPin(OBP_POWER_SD, false);        // Power off SD card
        #else
        // Switch off all power lines
        setPortPin(OBP_BACKLIGHT_LED, false);   // Backlight Off
        setFlashLED(false);                     // Flash LED Off            
        buzzer(TONE4, 20);                      // Buzzer tone 4kHz 20ms
        setPortPin(OBP_POWER_50, false);        // Power rail 5.0V Off
        // Shutdown EInk display
        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
        epd->fillScreen(common.bgcolor);// Clear screen
        epd->setTextColor(common.fgcolor);
        epd->setFont(&Ubuntu_Bold20pt8b);
        epd->setCursor(65, 150);
        epd->print("Undervoltage");
        epd->setFont(&Ubuntu_Bold8pt8b);
        epd->setCursor(65, 175);
        epd->print("To wake up repower system");
        epd->nextPage();                // Partial update
        epd->powerOff();                // Display power off
        #endif
        // Stop system
        while(true){
            esp_deep_sleep_start();             // Deep Sleep without weakup. Weakup only after power cycle (restart).
        }
    }
}

//bool addTrueWind(GwApi* api, BoatValueList* boatValues, double *twd, double *tws, double *twa) {
bool addTrueWind(GwApi* api, BoatValueList* boatValues) {
    // Calculate true wind data and add to obp60task boat data list

    double awaVal, awsVal, cogVal, stwVal, sogVal, hdtVal, hdmVal, varVal;
    double twd, tws, twa;
    bool isCalculated = false;
    const double DBL_MIN = std::numeric_limits<double>::lowest();

    GwApi::BoatValue *twdBVal = boatValues->findValueOrCreate("TWD");
    GwApi::BoatValue *twsBVal = boatValues->findValueOrCreate("TWS");
    GwApi::BoatValue *twaBVal = boatValues->findValueOrCreate("TWA");
    GwApi::BoatValue *awaBVal = boatValues->findValueOrCreate("AWA");
    GwApi::BoatValue *awsBVal = boatValues->findValueOrCreate("AWS");
    GwApi::BoatValue *cogBVal = boatValues->findValueOrCreate("COG");
    GwApi::BoatValue *stwBVal = boatValues->findValueOrCreate("STW");
    GwApi::BoatValue *sogBVal = boatValues->findValueOrCreate("SOG");
    GwApi::BoatValue *hdtBVal = boatValues->findValueOrCreate("HDT");
    GwApi::BoatValue *hdmBVal = boatValues->findValueOrCreate("HDM");
    GwApi::BoatValue *varBVal = boatValues->findValueOrCreate("VAR");
    awaVal = awaBVal->valid ? awaBVal->value : DBL_MIN;
    awsVal = awsBVal->valid ? awsBVal->value : DBL_MIN;
    cogVal = cogBVal->valid ? cogBVal->value : DBL_MIN;
    stwVal = stwBVal->valid ? stwBVal->value : DBL_MIN;
    sogVal = sogBVal->valid ? sogBVal->value : DBL_MIN;
    hdtVal = hdtBVal->valid ? hdtBVal->value : DBL_MIN;
    hdmVal = hdmBVal->valid ? hdmBVal->value : DBL_MIN;
    varVal = varBVal->valid ? varBVal->value : DBL_MIN;
    api->getLogger()->logDebug(GwLog::DEBUG,"obp60task addTrueWind: AWA %.1f, AWS %.1f, COG %.1f, STW %.1f, SOG %.1f, HDT %.1f, HDM %.1f, VAR %.1f", awaBVal->value * RAD_TO_DEG, awsBVal->value * 3.6 / 1.852,
            cogBVal->value * RAD_TO_DEG, stwBVal->value * 3.6 / 1.852, sogBVal->value * 3.6 / 1.852, hdtBVal->value * RAD_TO_DEG, hdmBVal->value * RAD_TO_DEG, varBVal->value * RAD_TO_DEG);

    isCalculated = WindUtils::calcTrueWind(&awaVal, &awsVal, &cogVal, &stwVal, &sogVal, &hdtVal, &hdmVal, &varVal, &twd, &tws, &twa);

    if (isCalculated) { // Replace values only, if successfully calculated and not already available
        if (!twdBVal->valid) {
            twdBVal->value = twd;
            twdBVal->valid = true;
        }
        if (!twsBVal->valid) {
            twsBVal->value = tws;
            twsBVal->valid = true;
        }
        if (!twaBVal->valid) {
            twaBVal->value = twa;
            twaBVal->valid = true;
        }
    }
    api->getLogger()->logDebug(GwLog::DEBUG,"obp60task addTrueWind: TWD_Valid %d, isCalculated %d, TWD %.1f, TWA %.1f, TWS %.1f", twdBVal->valid, isCalculated, twdBVal->value * RAD_TO_DEG,
        twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852);

    return isCalculated;
}

void initHstryBuf(GwApi* api, BoatValueList* boatValues, tBoatHstryData hstryBufList) {
    // Init history buffers for TWD, TWS

    GwApi::BoatValue *calBVal; // temp variable just for data calibration -> we don't want to calibrate the original data here

    int hstryUpdFreq = 1000; // Update frequency for history buffers in ms
    int hstryMinVal = 0; // Minimum value for these history buffers
    int twdHstryMax = 6283; // Max value for wind direction (TWD) in rad (0...2*PI), shifted by 1000 for 3 decimals
    int twsHstryMax = 1000; // Max value for wind speed (TWS) in m/s, shifted by 10 for 1 decimal
    // Initialize history buffers with meta data
    hstryBufList.twdHstry->setMetaData("TWD", "formatCourse", hstryUpdFreq, hstryMinVal, twdHstryMax);
    hstryBufList.twsHstry->setMetaData("TWS", "formatKnots", hstryUpdFreq, hstryMinVal, twsHstryMax);

    GwApi::BoatValue *twdBVal = boatValues->findValueOrCreate(hstryBufList.twdHstry->getName());
    GwApi::BoatValue *twsBVal = boatValues->findValueOrCreate(hstryBufList.twsHstry->getName());
    GwApi::BoatValue *twaBVal = boatValues->findValueOrCreate("TWA");
}

void handleHstryBuf(GwApi* api, BoatValueList* boatValues, tBoatHstryData hstryBufList) {
    // Handle history buffers for TWD, TWS

    GwLog *logger = api->getLogger();

    int16_t twdHstryMin = hstryBufList.twdHstry->getMinVal();
    int16_t twdHstryMax = hstryBufList.twdHstry->getMaxVal();
    int16_t twsHstryMin = hstryBufList.twsHstry->getMinVal();
    int16_t twsHstryMax = hstryBufList.twsHstry->getMaxVal();
    int16_t twdBuf, twsBuf;
    GwApi::BoatValue *calBVal; // temp variable just for data calibration -> we don't want to calibrate the original data here

    GwApi::BoatValue *twdBVal = boatValues->findValueOrCreate(hstryBufList.twdHstry->getName());
    GwApi::BoatValue *twsBVal = boatValues->findValueOrCreate(hstryBufList.twsHstry->getName());
    GwApi::BoatValue *twaBVal = boatValues->findValueOrCreate("TWA");

    api->getLogger()->logDebug(GwLog::DEBUG,"obp60task handleHstryBuf: twdBVal: %.1f, twaBVal: %.1f, twsBVal: %.1f, TWD_isValid? %d", twdBVal->value * RAD_TO_DEG,
        twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852, twdBVal->valid);
    calBVal = new GwApi::BoatValue("TWD"); // temporary solution for calibration of history buffer values
    calBVal->setFormat(twdBVal->getFormat());
    if (twdBVal->valid) {
        calBVal->value = twdBVal->value;
        calBVal->valid = twdBVal->valid;
        calibrationData.calibrateInstance(calBVal, logger); // Check if boat data value is to be calibrated
        twdBuf = static_cast<int16_t>(std::round(calBVal->value * 1000));
        if (twdBuf >= twdHstryMin && twdBuf <= twdHstryMax) {
            hstryBufList.twdHstry->add(twdBuf);
        }
    }
    delete calBVal;
    calBVal = nullptr;

    calBVal = new GwApi::BoatValue("TWS"); // temporary solution for calibration of history buffer values
    calBVal->setFormat(twsBVal->getFormat());
    if (twsBVal->valid) {
        calBVal->value = twsBVal->value;
        calBVal->valid = twsBVal->valid;
        calibrationData.calibrateInstance(calBVal, logger); // Check if boat data value is to be calibrated
        twsBuf = static_cast<int16_t>(std::round(calBVal->value * 10));
        if (twsBuf >= twsHstryMin && twsBuf <= twsHstryMax) {
            hstryBufList.twsHstry->add(twsBuf);
        }
    }
    delete calBVal;
    calBVal = nullptr;
}

// OBP60 Task
//####################################################################################
void OBP60Task(GwApi *api){
//    vTaskDelete(NULL);
//    return;
    GwLog *logger=api->getLogger();
    GwConfigHandler *config=api->getConfig();
#ifdef HARDWARE_V21
    startLedTask(api);
#endif
    PageList allPages;
    registerAllPages(logger, allPages);
    CommonData commonData;
    commonData.logger=logger;
    commonData.config=config;

#ifdef HARDWARE_V21
    // Keyboard coordinates for page footer
    initKeys(commonData);
#endif

    tN2kMsg N2kMsg;

    LOG_DEBUG(GwLog::LOG,"obp60task started");
    for (auto it=allPages.pages.begin();it != allPages.pages.end();it++){
        LOG_DEBUG(GwLog::LOG,"found registered page %s",(*it)->pageName.c_str());
    }

    // Init E-Ink display
    String displaymode = api->getConfig()->getConfigItem(api->getConfig()->display,true)->asString();
    String displaycolor = api->getConfig()->getConfigItem(api->getConfig()->displaycolor,true)->asString();
    if (displaycolor == "Normal") {
        commonData.fgcolor = GxEPD_BLACK;
        commonData.bgcolor = GxEPD_WHITE;
    }
    else{
        commonData.fgcolor = GxEPD_WHITE;
        commonData.bgcolor = GxEPD_BLACK;
    }
    String systemname = api->getConfig()->getConfigItem(api->getConfig()->systemName,true)->asString();
    String wifipass = api->getConfig()->getConfigItem(api->getConfig()->apPassword,true)->asString();
    bool refreshmode = api->getConfig()->getConfigItem(api->getConfig()->refresh,true)->asBoolean();
    bool symbolmode = (config->getString(config->headerFormat) == "ICON");
    String fastrefresh = api->getConfig()->getConfigItem(api->getConfig()->fastRefresh,true)->asString();
    uint fullrefreshtime = uint(api->getConfig()->getConfigItem(api->getConfig()->fullRefreshTime,true)->asInt());
    #ifdef BOARD_OBP40S3
    bool syspage_enabled = config->getBool(config->systemPage);
    #endif

    #ifdef DISPLAY_GDEY042T81
        epd->init(115200, true, 2, false);  // Init for Waveshare boards with "clever" reset circuit, 2ms reset pulse
    #else
        epd->init(115200);                  // Init for normal displays
    #endif

    epd->setRotation(0);                 // Set display orientation (horizontal)
    epd->setFullWindow();                // Set full Refresh
    epd->firstPage();                    // set first page
    epd->fillScreen(commonData.bgcolor);
    epd->setTextColor(commonData.fgcolor);
    epd->nextPage();                     // Full Refresh
    epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
    epd->fillScreen(commonData.bgcolor);
    epd->nextPage();                     // Fast Refresh
    epd->nextPage();                     // Fast Refresh
    if(String(displaymode) == "Logo + QR Code" || String(displaymode) == "Logo"){
        epd->fillScreen(commonData.bgcolor);
        epd->drawXBitmap(0, 0, OBP_400x300_bits, OBP_400x300_width, OBP_400x300_height, commonData.fgcolor);
        epd->nextPage();                 // Fast Refresh
        epd->nextPage();                 // Fast Refresh
        delay(SHOW_TIME);                        // Logo show time
        if(String(displaymode) == "Logo + QR Code"){
            epd->fillScreen(commonData.bgcolor);
            qrWiFi(systemname, wifipass, commonData.fgcolor, commonData.bgcolor);  // Show QR code for WiFi connection
            epd->nextPage();             // Fast Refresh
            epd->nextPage();             // Fast Refresh
            delay(SHOW_TIME);                    // QR code show time
        }
        epd->fillScreen(commonData.bgcolor);
        epd->nextPage();                 // Fast Refresh
        epd->nextPage();                 // Fast Refresh
    }
    
    // Init pages
    int numPages=1;
    PageStruct pages[MAX_PAGE_NUMBER];
    // Set start page
    int pageNumber = int(api->getConfig()->getConfigItem(api->getConfig()->startPage,true)->asInt()) - 1;

    LOG_DEBUG(GwLog::LOG,"Checking wakeup...");
#ifdef BOARD_OBP60S3
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TOUCHPAD) {
        LOG_DEBUG(GwLog::LOG,"Wake up by touch pad %d",esp_sleep_get_touchpad_wakeup_status());
        pageNumber = getLastPage();
    } else {
        LOG_DEBUG(GwLog::LOG,"Other wakeup reason");
    }
#endif
#ifdef BOARD_OBP40S3
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        LOG_DEBUG(GwLog::LOG,"Wake up by key");
        pageNumber = getLastPage();
    } else {
        LOG_DEBUG(GwLog::LOG,"Other wakeup reason");
    }
#endif
    LOG_DEBUG(GwLog::LOG,"...done");

    int lastPage=pageNumber;

    BoatValueList boatValues; //all the boat values for the api query
    //commonData.distanceformat=config->getString(xxx);
    //add all necessary data to common data

    // Create ring buffers for history storage of some boat data
    RingBuffer<int16_t> twdHstry(960); // Circular buffer to store wind direction values; store 960 TWD values for 16 minutes history
    RingBuffer<int16_t> twsHstry(960); // Circular buffer to store wind speed values (TWS)
    tBoatHstryData hstryBufList = {&twdHstry, &twsHstry};

    //fill the page data from config
    numPages=config->getInt(config->visiblePages,1);
    if (numPages < 1) numPages=1;
    if (numPages >= MAX_PAGE_NUMBER) numPages=MAX_PAGE_NUMBER;
    LOG_DEBUG(GwLog::LOG,"Number of pages %d",numPages);
    String configPrefix="page";
    for (int i=0;i< numPages;i++){
       String prefix=configPrefix+String(i+1); //e.g. page1
       String configName=prefix+String("type");
       LOG_DEBUG(GwLog::DEBUG,"asking for page config %s",configName.c_str());
       String pageType=config->getString(configName,"");
       PageDescription *description=allPages.find(pageType);
       if (description == NULL){
           LOG_DEBUG(GwLog::ERROR,"page description for %s not found",pageType.c_str());
           continue;
       }
       pages[i].description=description;
       pages[i].page=description->creator(commonData);
       pages[i].parameters.pageName=pageType;
       pages[i].parameters.pageNumber = i + 1;
       LOG_DEBUG(GwLog::DEBUG,"found page %s for number %d",pageType.c_str(),i);
       //fill in all the user defined parameters
       for (int uid=0;uid<description->userParam;uid++){
           String cfgName=prefix+String("value")+String(uid+1);
           GwApi::BoatValue *value=boatValues.findValueOrCreate(config->getString(cfgName,""));
           LOG_DEBUG(GwLog::DEBUG,"add user input cfg=%s,value=%s for page %d",
                cfgName.c_str(),
                value->getName().c_str(),
                i
           );
           pages[i].parameters.values.push_back(value);
       }
       //now add the predefined values
       for (auto it=description->fixedParam.begin();it != description->fixedParam.end();it++){
            GwApi::BoatValue *value=boatValues.findValueOrCreate(*it);
            LOG_DEBUG(GwLog::DEBUG,"added fixed value %s to page %d",value->getName().c_str(),i);
            pages[i].parameters.values.push_back(value); 
       }
        if (pages[i].description->pageName == "WindPlot") {
            // Add boat history data to page parameters
            pages[i].parameters.boatHstry = hstryBufList;
        }
    }
    // add out of band system page (always available)
    Page *syspage = allPages.pages[0]->creator(commonData);

    // Read all calibration data settings from config
    calibrationData.readConfig(config, logger);

    // Check user setting for true wind calculation
    bool calcTrueWnds = api->getConfig()->getBool(api->getConfig()->calcTrueWnds, false);
    // bool simulation = api->getConfig()->getBool(api->getConfig()->useSimuData, false);

    // Initialize history buffer for certain boat data
    initHstryBuf(api, &boatValues, hstryBufList);

    // Display screenshot handler for HTTP request
    // http://192.168.15.1/api/user/OBP60Task/screenshot
    api->registerRequestHandler("screenshot", [api, &pageNumber, pages](AsyncWebServerRequest *request) {
        doImageRequest(api, &pageNumber, pages, request);
    });

    //now we have prepared the page data
    //we start a separate task that will fetch our keys...
    MyData allParameters;
    allParameters.logger=api->getLogger();
    allParameters.page0=3;
    allParameters.queue=xQueueCreate(10,sizeof(int));
    allParameters.sensitivity= api->getConfig()->getInt(GwConfigDefinitions::tSensitivity);
    #ifdef BOARD_OBP40S3
    allParameters.use_syspage = syspage_enabled;
    #endif
    xTaskCreate(keyboardTask,"keyboard",2000,&allParameters,configMAX_PRIORITIES-1,NULL);
    SharedData *shared=new SharedData(api);
    createSensorTask(shared);

    // Task Loop
    //####################################################################################

    // Configuration values for main loop
    String gpsFix = api->getConfig()->getConfigItem(api->getConfig()->flashLED,true)->asString();
    String gpsOn=api->getConfig()->getConfigItem(api->getConfig()->useGPS,true)->asString();
    float tz = api->getConfig()->getConfigItem(api->getConfig()->timeZone,true)->asFloat();

    commonData.backlight.mode = backlightMapping(config->getConfigItem(config->backlight,true)->asString());
    commonData.backlight.color = colorMapping(config->getConfigItem(config->blColor,true)->asString());
    commonData.backlight.brightness = 2.55 * uint(config->getConfigItem(config->blBrightness,true)->asInt());
    commonData.powermode = api->getConfig()->getConfigItem(api->getConfig()->powerMode,true)->asString();

    bool uvoltage = api->getConfig()->getConfigItem(api->getConfig()->underVoltage,true)->asBoolean();
    String cpuspeed = api->getConfig()->getConfigItem(api->getConfig()->cpuSpeed,true)->asString();
    uint hdopAccuracy = uint(api->getConfig()->getConfigItem(api->getConfig()->hdopAccuracy,true)->asInt());

    double homelat = commonData.config->getString(commonData.config->homeLAT).toDouble();
    double homelon = commonData.config->getString(commonData.config->homeLON).toDouble();
    bool homevalid = homelat >= -180.0 and homelat <= 180 and homelon >= -90.0 and homelon <= 90.0;
    if (homevalid) {
        logger->logDebug(GwLog::LOG, "Home location set to lat=%f, lon=%f", homelat, homelon);
    } else {
        logger->logDebug(GwLog::LOG, "No valid home location found");
    }

    // refreshmode defined in init section

    // Boat values for main loop
    GwApi::BoatValue *date = boatValues.findValueOrCreate("GPSD");      // Load GpsDate
    GwApi::BoatValue *time = boatValues.findValueOrCreate("GPST");      // Load GpsTime
    GwApi::BoatValue *lat = boatValues.findValueOrCreate("LAT");        // Load GpsLatitude
    GwApi::BoatValue *lon = boatValues.findValueOrCreate("LON");        // Load GpsLongitude
    GwApi::BoatValue *hdop = boatValues.findValueOrCreate("HDOP");       // Load GpsHDOP

    LOG_DEBUG(GwLog::LOG,"obp60task: start mainloop");
    
    commonData.time = boatValues.findValueOrCreate("GPST");      // Load GpsTime
    commonData.date = boatValues.findValueOrCreate("GPSD");      // Load GpsTime
    bool delayedDisplayUpdate = false;  // If select a new pages then make a delayed full display update
    bool cpuspeedsetted = false;    // Marker for change CPU speed 
    long firststart = millis();     // First start
    long starttime0 = millis();     // Mainloop
    long starttime1 = millis();     // Full display refresh for the first 5 min (more often as normal)
    long starttime2 = millis();     // Full display refresh after 5 min
    long starttime3 = millis();     // Display update all 1s
    long starttime4 = millis();     // Delayed display update after 4s when select a new page
    long starttime5 = millis();     // Calculate sunrise and sunset all 1s

    pages[pageNumber].page->setupKeys(); // Initialize keys for first page

    // Main loop runs with 100ms
    //####################################################################################

    bool systemPage = false;
    bool systemPageNew = false;
    Page *currentPage;
    while (true){
        delay(100);     // Delay 100ms (loop time)
        bool keypressed = false;

        // Undervoltage detection
        if(uvoltage == true){
            underVoltageDetection(api, commonData);
        }

        // Set CPU speed after boot after 1min 
        if(millis() > firststart + (1 * 60 * 1000) && cpuspeedsetted == false){
            if(String(cpuspeed) == "80"){
                setCpuFrequencyMhz(80);
            }
            if(String(cpuspeed) == "160"){
                setCpuFrequencyMhz(160);
            }
            if(String(cpuspeed) == "240"){
                setCpuFrequencyMhz(240);
            }
            int freq = getCpuFrequencyMhz();
            api->getLogger()->logDebug(GwLog::LOG,"CPU speed: %i MHz", freq);
            cpuspeedsetted = true;
        }

        if(millis() > starttime0 + 100){
            starttime0 = millis();
            commonData.data=shared->getSensorData();
            commonData.data.actpage = pageNumber + 1;
            commonData.data.maxpage = numPages;
            
            // If GPS fix then LED off (HDOP)
            if(String(gpsFix) == "GPS Fix Lost" && hdop->value <= hdopAccuracy && hdop->valid == true){
                setFlashLED(false);
            }
            // If missing GPS fix then LED on
            if((String(gpsFix) == "GPS Fix Lost" && hdop->value > hdopAccuracy && hdop->valid == true) || (String(gpsFix) == "GPS Fix Lost" && hdop->valid == false)){
                setFlashLED(true);
            }
         
            // Check the keyboard message
            int keyboardMessage=0;
            while (xQueueReceive(allParameters.queue,&keyboardMessage,0)){
                LOG_DEBUG(GwLog::LOG,"new key from keyboard %d",keyboardMessage);
                keypressed = true;

                if (keyboardMessage == 12 and !systemPage) {
                    LOG_DEBUG(GwLog::LOG, "Calling system page");
                    systemPage = true; // System page is out of band
                    syspage->setupKeys();
                    keyboardMessage = 0;
                    systemPageNew = true;
                }
                else {
                    currentPage = pages[pageNumber].page;
                    if (systemPage && keyboardMessage == 1) {
                        // exit system mode with exit key number 1
                        systemPage = false;
                        currentPage->setupKeys();
                        keyboardMessage = 0;
                    }
                }
                if (systemPage) {
                    keyboardMessage = syspage->handleKey(keyboardMessage);
                } else if (currentPage) {
                    keyboardMessage = currentPage->handleKey(keyboardMessage);
                }
                if (keyboardMessage > 0) // not handled by page
                {
                    // Decoding all key codes
                    // #6 Backlight on if key controled
                    if (commonData.backlight.mode == BacklightMode::KEY) {
                    // if(String(backlight) == "Control by Key"){
                        if(keyboardMessage == 6){
                            LOG_DEBUG(GwLog::LOG,"Toggle Backlight LED");
                            toggleBacklightLED(commonData.backlight.brightness, commonData.backlight.color);
                        }
                    }
                    #ifdef BOARD_OBP40S3
                    // #3 Deep sleep mode for OBP40
                    if ((keyboardMessage == 3) and !syspage_enabled){
                        deepSleep(commonData);
                    }
                    #endif
                    // #9 Swipe right or #4 key right
                    if ((keyboardMessage == 9) or (keyboardMessage == 4))
                    {
                        pageNumber++;
                        if (pageNumber >= numPages){
                            pageNumber = 0;
                        }
                        commonData.data.actpage = pageNumber + 1;
                        commonData.data.maxpage = numPages;
                    }
                    // #10 Swipe left or #3 key left
                    if ((keyboardMessage == 10) or (keyboardMessage == 3))
                    {
                        pageNumber--;
                        if (pageNumber < 0){
                            pageNumber = numPages - 1;
                        }
                        commonData.data.actpage = pageNumber + 1;
                        commonData.data.maxpage = numPages;
                    }
                  
                    // #9 or #10 Refresh display after a new page after 4s waiting time and if refresh is disabled
                    if(refreshmode == true && (keyboardMessage == 9 || keyboardMessage == 10 || keyboardMessage == 4 || keyboardMessage == 3)){
                        starttime4 = millis();
                        starttime2 = millis();      // Reset the timer for full display update
                        delayedDisplayUpdate = true;
                    }                 
                }
                LOG_DEBUG(GwLog::LOG,"set pagenumber to %d",pageNumber);
            }

            // Calculate sunrise, sunset and backlight control with sun status all 1s
            if(millis() > starttime5 + 1000){
                starttime5 = millis();
                if(time->valid == true && date->valid == true && lat->valid == true && lon->valid == true){
                    // Provide sundata to all pages
                    commonData.sundata = calcSunsetSunrise(time->value , date->value, lat->value, lon->value, tz);
                    // Backlight with sun control
                    if (commonData.backlight.mode == BacklightMode::SUN) {
                    // if(String(backlight) == "Control by Sun"){
                        if(commonData.sundata.sunDown == true){
                            setBacklightLED(commonData.backlight.brightness, commonData.backlight.color);
                        }
                        else{
                            setBacklightLED(0, COLOR_BLUE); // Backlight LEDs off (blue without britghness)
                        }
                    }
                } else if (homevalid and commonData.data.rtcValid) {
                    // No gps fix but valid home location and time configured
                    commonData.sundata = calcSunsetSunriseRTC(&commonData.data.rtcTime, homelat, homelon, tz);
                }
            }
            
            // Full display update afer a new selected page and 4s wait time
            if(millis() > starttime4 + 4000 && delayedDisplayUpdate == true){
                starttime1 = millis();
                starttime2 = millis();
                epd->setFullWindow();    // Set full update
                if(fastrefresh == "true"){
                    epd->nextPage();                     // Full update
                }
                else{
                    epd->fillScreen(commonData.fgcolor); // Clear display
                    #ifdef DISPLAY_GDEY042T81
                        epd->init(115200, true, 2, false); // Init for Waveshare boards with "clever" reset circuit, 2ms reset pulse
                    #else
                        epd->init(115200);               // Init for normal displays
                    #endif
                    epd->firstPage();                    // Full update
                    epd->nextPage();                     // Full update
//                    epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
//                    epd->fillScreen(commonData.bgcolor); // Clear display
//                    epd->nextPage();                     // Partial update
//                    epd->nextPage();                     // Partial update
                }
                delayedDisplayUpdate = false;
            }

            // Subtask E-Ink full refresh all 1 min for the first 5 min after power on or restart
            // This needs for a better display contrast after power on in cold or warm environments
            if(millis() < firststart + (5 * 60 * 1000) && millis() > starttime1 + (60 * 1000)){
                starttime1 = millis();
                starttime2 = millis();
                LOG_DEBUG(GwLog::DEBUG,"E-Ink full refresh first 5 min");
                epd->setFullWindow();    // Set full update
                if(fastrefresh == "true"){
                    epd->nextPage();                     // Full update
                }
                else{
                    epd->fillScreen(commonData.fgcolor); // Clear display
                    #ifdef DISPLAY_GDEY042T81
                        epd->init(115200, true, 2, false); // Init for Waveshare boards with "clever" reset circuit, 2ms reset pulse
                    #else
                        epd->init(115200);               // Init for normal displays
                    #endif
                    epd->firstPage();                    // Full update
                    epd->nextPage();                     // Full update
//                    epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
//                    epd->fillScreen(commonData.bgcolor); // Clear display
//                    epd->nextPage();                     // Partial update
//                    epd->nextPage();                     // Partial update
                }
            }

            // Subtask E-Ink full refresh
            if(millis() > starttime2 + fullrefreshtime * 60 * 1000){
                starttime2 = millis();
                LOG_DEBUG(GwLog::DEBUG,"E-Ink full refresh");
                epd->setFullWindow();    // Set full update
                if(fastrefresh == "true"){
                    epd->nextPage();                     // Full update
                }
                else{
                    epd->fillScreen(commonData.fgcolor); // Clear display
                    #ifdef DISPLAY_GDEY042T81
                        epd->init(115200, true, 2, false); // Init for Waveshare boards with "clever" reset circuit, 2ms reset pulse
                    #else
                        epd->init(115200);               // Init for normal displays
                    #endif
                    epd->firstPage();                    // Full update
                    epd->nextPage();                     // Full update
//                    epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
//                    epd->fillScreen(commonData.bgcolor); // Clear display
//                    epd->nextPage();                     // Partial update
//                    epd->nextPage();                     // Partial update
                }
            }        
                
            // Refresh display data, default all 1s
            currentPage = pages[pageNumber].page;
            int pagetime = 1000;
            if ((lastPage == pageNumber) and (!keypressed)) {
                // same page we use page defined time
                pagetime = currentPage->refreshtime;
            }
            if(millis() > starttime3 + pagetime){
                LOG_DEBUG(GwLog::DEBUG,"Page with refreshtime=%d", pagetime);
                starttime3 = millis();

                //refresh data from api
                api->getBoatDataValues(boatValues.numValues,boatValues.allBoatValues);
                api->getStatus(commonData.status);

                if (calcTrueWnds) {
                    addTrueWind(api, &boatValues);
                }
                // Handle history buffers for TWD, TWS for wind plot page and other usage
                 handleHstryBuf(api, &boatValues, hstryBufList);

                // Clear display
                // epd->fillRect(0, 0, epd->width(), epd->height(), commonData.bgcolor);
                epd->fillScreen(commonData.bgcolor);  // Clear display

                // Show header if enabled
                if (pages[pageNumber].description && pages[pageNumber].description->header or systemPage){
                    // build header using commonData
                    displayHeader(commonData, symbolmode, date, time, hdop);  // Show page header
                }

                // Call the particular page
                if (systemPage) {
                    displayFooter(commonData);
                    PageData sysparams; // empty
                    sysparams.api = api;
                    if (systemPageNew) {
                        syspage->displayNew(sysparams);
                        systemPageNew = false;
                    }
                    syspage->displayPage(sysparams);
                }
                else {
                    if (currentPage == NULL){
                        LOG_DEBUG(GwLog::ERROR,"page number %d not found", pageNumber);
                        // Error handling for missing page
                        epd->setPartialWindow(0, 0, epd->width(), epd->height()); // Set partial update
                        epd->fillScreen(commonData.bgcolor);  // Clear display
                        epd->drawXBitmap(200 - unknown_width / 2, 150 - unknown_height / 2, unknown_bits, unknown_width, unknown_height, commonData.fgcolor);
                        epd->setCursor(140, 250);
                        epd->setFont(&Atari16px);
                        epd->print("Here be dragons!");
                        epd->nextPage(); // Partial update (fast)
                    }
                    else{
                        if (lastPage != pageNumber){
                            if (hasFRAM) fram.write(FRAM_PAGE_NO, pageNumber); // remember page for device restart
                            currentPage->setupKeys();
                            currentPage->displayNew(pages[pageNumber].parameters);
                            lastPage=pageNumber;
                        }
                        //call the page code
                        LOG_DEBUG(GwLog::DEBUG,"calling page %d",pageNumber);
                        // Show footer if enabled (together with header)
                        if (pages[pageNumber].description && pages[pageNumber].description->header){
                            displayFooter(commonData);
                        }
                        int ret = currentPage->displayPage(pages[pageNumber].parameters);
                        if (commonData.alarm.active) {
                            displayAlarm(commonData);
                        }
                        if (ret & PAGE_UPDATE) {
                            epd->nextPage(); // Partial update (fast)
                        }
                        if (ret & PAGE_HIBERNATE) {
                            epd->hibernate();
                        }
                    }

                }
            } // refresh display all 1s
        }
    }
    vTaskDelete(NULL);

}

#endif
