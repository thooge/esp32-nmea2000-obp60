// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <Arduino.h>
#include "GwApi.h"
#include <functional>
#include <vector>
#include "LedSpiTask.h"
#include "OBPRingBuffer.h"
#include "OBPDataOperations.h"

#define MAX_PAGE_NUMBER 10    // Max number of pages for show data

typedef std::vector<GwApi::BoatValue *> ValueList;

typedef struct{
  GwApi *api;
  String pageName;
  uint8_t pageNumber; // page number in sequence of visible pages
  //the values will always contain the user defined values first
  ValueList values;
  tBoatHstryData boatHstry;
} PageData;

// Sensor data structure (only for extended sensors, not for NMEA bus sensors)
typedef struct{
  int actpage = 0;
  int maxpage = 0;
  double batteryVoltage = 0;
  double batteryCurrent = 0;
  double batteryPower = 0;
  double batteryVoltage10 = 0;  // Sliding average over 10 values
  double batteryCurrent10 = 0;
  double batteryPower10 = 0;
  double batteryVoltage60 = 0;  // Sliding average over 60 values
  double batteryCurrent60 = 0;
  double batteryPower60 = 0;
  double batteryVoltage300 = 0; // Sliding average over 300 values
  double batteryCurrent300 = 0;
  double batteryPower300 = 0;
  double batteryLevelLiPo = 0;  // Battery level for OBP40 LiPo accu
  int BatteryChargeStatus = 0;  // LiPo charge status: 0 = discharge, 1 = loading activ
  double solarVoltage = 0;
  double solarCurrent = 0;
  double solarPower = 0;
  double generatorVoltage = 0;
  double generatorCurrent = 0;
  double generatorPower = 0;
  double airTemperature = 0;
  double airHumidity = 0;
  double airPressure = 0;
  double onewireTemp[8] = {0,0,0,0,0,0,0,0};
  double rotationAngle = 0;     // Rotation angle in radiant
  bool validRotAngle = false;   // Valid flag magnet present for rotation sensor
  struct tm rtcTime;            // UTC time from internal RTC
  bool rtcValid = false;
  int sunsetHour = 0;
  int sunsetMinute = 0;
  int sunriseHour = 0;
  int sunriseMinute = 0;
  bool sunDown = true;
} SensorData;

typedef struct{
  int sunsetHour = 0;
  int sunsetMinute = 0;
  int sunriseHour = 0;
  int sunriseMinute = 0;
  bool sunDown = true;
} SunData;

typedef struct{
  String label = "";
  bool selected = false;    // for virtual keyboard function
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
} TouchKeyData;

typedef struct{
    Color color;        // red, orange, yellow, green, blue, aqua, violet, white
    BacklightMode mode; // off, on, sun, bus, time, key
    uint8_t brightness; // 0% (off), user setting from 20% to 100% full power
    bool on;            // fast on/off detector
} BacklightData;

enum AlarmSource {
    Alarm_Generic,
    Alarm_Local,
    Alarm_NMEA0183,
    Alarm_NMEA2000
};

typedef struct{
    uint8_t id; // alarm-id e.g. 01..99 from NMEA0183
    AlarmSource source;
    String message; // single line of plain text
    bool active = false;
    uint8_t signal; // how to signal MESSAGE | LED | BUZZER
    uint8_t length_sec; // seconds until alarm disappeares without user interaction
} AlarmData;

typedef struct{
    int voltage = 0;
} AvgData;

class Formatter; // forward declaration
typedef struct{
    GwApi::Status status;
    GwLog *logger = nullptr;
    GwConfigHandler *config = nullptr;
    Formatter *fmt = nullptr;
    SensorData data;
    SunData sundata;
    TouchKeyData keydata[6];
    BacklightData backlight;
    AlarmData alarm;
    AvgData avgdata;
    GwApi::BoatValue *time = nullptr;
    GwApi::BoatValue *date = nullptr;
    uint16_t fgcolor;
    uint16_t bgcolor;
    bool keylock = false;
    String powermode;
} CommonData;

//a base class that all pages must inherit from
class Page{
protected:
    CommonData *commonData;
    GwConfigHandler *config;
    GwLog *logger;
    bool simulation = false;
    bool holdvalues = false;
    String flashLED;
    String backlightMode;
public:
    Page(CommonData &common) {
        commonData = &common;
        config = commonData->config;
        logger = commonData->logger;
        // preload generic configuration data
        simulation = config->getBool(config->useSimuData);
        holdvalues = config->getBool(config->holdvalues);
        flashLED = config->getString(config->flashLED);
        backlightMode = config->getString(config->backlight);
    }
    int refreshtime = 1000;
    virtual int displayPage(PageData &pageData)=0;
    virtual void displayNew(PageData &pageData){}
    virtual void leavePage(PageData &pageData){}
    virtual void setupKeys() {
#ifdef HARDWARE_V21
        commonData->keydata[0].label = "";
        commonData->keydata[1].label = "";
        commonData->keydata[2].label = "#LEFT";
        commonData->keydata[3].label = "#RIGHT";
        commonData->keydata[4].label = "";
        if ((commonData->backlight.mode == KEY) && !(commonData->powermode == "Min Power")) {
            commonData->keydata[5].label = "ILUM";
        } else {
            commonData->keydata[5].label = "";
        }
#endif
#ifdef BOARD_OBP40S3
        commonData->keydata[0].label = "";
        commonData->keydata[1].label = "";

#endif
    }
    //return -1 if handled by the page
    virtual int handleKey(int key){return key;}
};

typedef std::function<Page* (CommonData &)> PageFunction;
typedef std::vector<String> StringList;

/**
 * a class that describes a page
 * it contains the name (type)
 * the number of expected user defined boat Values
 * and a list of boatValue names that are fixed
 * for each page you define a variable of this type
 * and add this to registerAllPages in the obp60task
 */
class PageDescription{
    public:
        String pageName;
        int userParam=0;
        StringList fixedParam;
        PageFunction creator;
        bool header=true;
        PageDescription(String name, PageFunction creator,int userParam,StringList fixedParam,bool header=true){
            this->pageName=name;
            this->userParam=userParam;
            this->fixedParam=fixedParam;
            this->creator=creator;
            this->header=header;
        }
        PageDescription(String name, PageFunction creator,int userParam,bool header=true){
            this->pageName=name;
            this->userParam=userParam;
            this->creator=creator;
            this->header=header;
        }
};

class PageStruct{
    public:
        Page *page = nullptr;
        PageData parameters;
        PageDescription *description = nullptr;
};
