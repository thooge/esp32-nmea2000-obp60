#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

/*
 * PageClock: Clock page with
 *  - Analog mode  (mode == 'A')
 *  - Digital mode (mode == 'D')
 *  - Countdown timer mode (mode == 'T')
 *  - Keys in mode analog and digital clock:
 *      K1: MODE (A/D/T)
 *      K2: POS  (select field: HH / MM / SS)
 *      K3: 
 *      K4: 
 *      K5: TZ  (Local/UTC)
 *
 * Regatta timer mode:
 *  - Format HH:MM:SS (24h, leading zeros)
 *  - Keys in timer mode:
 *      K1: MODE (A/D/T)
 *      K2: POS  (select field: HH / MM / SS)
 *      K3: +    (increment selected field)
 *      K4: -    (decrement selected field)
 *      K5: RUN  (start/stop countdown)
 *  - Selection marker: line under active field (width 2px, not wider than digits)
 *  - Editing only possible when timer is not running
 *  - When page is left, running timer continues in background using RTC time
 *    (on re-entry, remaining time is recalculated from RTC)
 */

class PageClock : public Page
{
    bool simulation = false;
    int simtime;
    bool keylock = false;
#ifdef BOARD_OBP60S3
    char source = 'G';  // Time source (R)TC | (G)PS | (N)TP
#endif
#ifdef BOARD_OBP40S3
    char source = 'R';  // time source (R)TC | (G)PS | (N)TP
#endif
    char mode = 'A';    // Display mode (A)nalog | (D)igital | race (T)imer
    char tz = 'L';      // Time zone (L)ocal | (U)TC
    double timezone = 0; // There are timezones with non int offsets, e.g. 5.5 or 5.75
    double homelat;
    double homelon;
    bool homevalid = false; // Homelat and homelon are valid

    // Timer state (static so it survives page switches)
    static bool timerInitialized;
    static bool timerRunning;
    static int timerHours;
    static int timerMinutes;
    static int timerSeconds;
    // Preset seconds for sync button (default 4 minutes)
    static const int timerPresetSeconds = 4 * 60;
    // Initial timer setting at start (so we can restore it)
    static int timerStartHours;
    static int timerStartMinutes;
    static int timerStartSeconds;
    static int selectedField;       // 0 = hours, 1 = minutes, 2 = seconds
    static bool showSelectionMarker;
    static time_t timerEndEpoch;    // Absolute end time based on RTC

    void setupTimerDefaults()
    {
        if (!timerInitialized) {
            timerInitialized = true;
            timerRunning = false;
            timerHours = 0;
            timerMinutes = 0;
            timerSeconds = 0;
            timerStartHours = 0;
            timerStartMinutes = 0;
            timerStartSeconds = 0;
            selectedField = 0;
            showSelectionMarker = true;
            timerEndEpoch = 0;
        }
    }

    // Limiter for overrun settings values
    static int clamp(int value, int minVal, int maxVal)
    {
        if (value < minVal) return maxVal;
        if (value > maxVal) return minVal;
        return value;
    }

    void incrementSelected()
    {
        if (selectedField == 0) {
            timerHours = clamp(timerHours + 1, 0, 23);
        } else if (selectedField == 1) {
            timerMinutes = clamp(timerMinutes + 1, 0, 59);
        } else {
            timerSeconds = clamp(timerSeconds + 1, 0, 59);
        }
    }

    void decrementSelected()
    {
        if (selectedField == 0) {
            timerHours = clamp(timerHours - 1, 0, 23);
        } else if (selectedField == 1) {
            timerMinutes = clamp(timerMinutes - 1, 0, 59);
        } else {
            timerSeconds = clamp(timerSeconds - 1, 0, 59);
        }
    }

    int totalTimerSeconds() const
    {
        return timerHours * 3600 + timerMinutes * 60 + timerSeconds;
    }

public:
    PageClock(CommonData& common)
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG, "Instantiate PageClock");
        simulation = common.config->getBool(common.config->useSimuData);
        timezone = common.config->getString(common.config->timeZone).toDouble();
        homelat = common.config->getString(common.config->homeLAT).toDouble();
        homelon = common.config->getString(common.config->homeLON).toDouble();
        homevalid = homelat >= -180.0 and homelat <= 180 and homelon >= -90.0 and homelon <= 90.0;
        simtime = 38160; // time value 11:36
        setupTimerDefaults();
    }

    virtual void setupKeys()
    {
        Page::setupKeys();

        if (mode == 'T') {
            // Timer mode: MODE, POS, +, -, RUN
            commonData->keydata[0].label = "MODE";
            commonData->keydata[1].label = "POS";
            // K3: '+' while editing, 'SYNC' while running to set a preset countdown
            commonData->keydata[2].label = timerRunning ? "SYNC" : "+";
            commonData->keydata[3].label = "-";
            commonData->keydata[4].label = timerRunning ? "RESET" : "START";
        } else {
            // Clock modes: like original
            commonData->keydata[0].label = "MODE";
            commonData->keydata[1].label = "SRC";
            commonData->keydata[4].label = "TZ";
        }
    }

    // Key functions
    virtual int handleKey(int key)
    {
        setupTimerDefaults();

        // Keylock function
        if (key == 11) {              // Code for keylock
            keylock = !keylock;       // Toggle keylock
            return 0;                 // Commit the key
        }

        if (mode == 'T') {
            // Timer mode key handling

            // MODE (K1): cycle display mode A/D/T
            if (key == 1) {
                switch (mode) {
                case 'A': mode = 'D'; break;
                case 'D': mode = 'T'; break;
                case 'T': mode = 'A'; break;
                default:  mode = 'A'; break;
                }
                setupKeys();
                return 0;
            }

            // POS (K2): select field HH / MM / SS (only if timer not running)
            if (key == 2 && !timerRunning) {
                selectedField = (selectedField + 1) % 3;
                showSelectionMarker = true;
                return 0;
            }

            // + (K3): increment selected field (only if timer not running)
            if (key == 3 && !timerRunning) {
                incrementSelected();
                return 0;
            }
            if (key == 3 && timerRunning) {
                // When timer is running, K3 acts as a synchronization button:
                // set remaining countdown to the preset value (e.g. 4 minutes).
                if (commonData->data.rtcValid) {
                    int preset = timerPresetSeconds;
                    // update start-setting so STOP will restore this preset
                    timerStartHours = preset / 3600;
                    timerStartMinutes = (preset % 3600) / 60;
                    timerStartSeconds = preset % 60;

                    struct tm rtcCopy = commonData->data.rtcTime;
                    time_t nowEpoch = mktime(&rtcCopy);
                    timerEndEpoch = nowEpoch + preset;

                    // Update visible timer fields immediately
                    timerHours = timerStartHours;
                    timerMinutes = timerStartMinutes;
                    timerSeconds = timerStartSeconds;
//                    commonData->keydata[4].label = "RESET";
                }
                return 0;
            }

            // - (K4): decrement selected field (only if timer not running)
            if (key == 4 && !timerRunning) {
                decrementSelected();
                return 0;
            }
            if (key == 4 && timerRunning) { // No action if timer running
                return 0;
            }

            // RUN (K5): start/stop timer
            if (key == 5) {
                if (!timerRunning) {
                    // Start timer if a non-zero duration is set
                    int total = totalTimerSeconds();
                    if (total > 0 && commonData->data.rtcValid) {
                        // Remember initial timer setting at start
                        timerStartHours = timerHours;
                        timerStartMinutes = timerMinutes;
                        timerStartSeconds = timerSeconds;

                        struct tm rtcCopy = commonData->data.rtcTime;
                        time_t nowEpoch = mktime(&rtcCopy);
                        timerEndEpoch = nowEpoch + total;
                        timerRunning = true;
                        showSelectionMarker = false;
                    }
                } else {
                    // Stop timer: restore initial start setting
                    timerHours = timerStartHours;
                    timerMinutes = timerStartMinutes;
                    timerSeconds = timerStartSeconds;
                    timerRunning = false;
                    showSelectionMarker = true;
                    // marker will become visible again only after POS press
                }
                return 0;
            }

            // In timer mode, other keys are passed through
            return key;
        }

        // Clock (A/D) modes key handling – like original PageClock

        // MODE (K1)
        if (key == 1) {
            switch (mode) {
            case 'A': mode = 'D'; break;
            case 'D': mode = 'T'; break;
            case 'T': mode = 'A'; break;
            default:  mode = 'A'; break;
            }
            setupKeys();
            return 0;
        }
        
        // Time source (K2)
        if (key == 2) {
            switch (source) {
            case 'G': source = 'R'; break;
            case 'R': source = 'G'; break;
            default:  source = 'G'; break;
            }
            return 0;
        }

        // Time zone: Local / UTC (K5)
        if (key == 5) {
            switch (tz) {
            case 'L': tz = 'U'; break;
            case 'U': tz = 'L'; break;
            default:  tz = 'L'; break;
            }
            return 0;
        }

        return key;
    }

    int displayPage(PageData& pageData)
    {
        GwConfigHandler* config = commonData->config;
        GwLog* logger = commonData->logger;

        setupTimerDefaults();
        setupKeys(); // Ensure correct key labels for current mode

        static String svalue1old = "";
        static String unit1old = "";
        static String svalue2old = "";
        static String unit2old = "";
        static String svalue3old = "";
        static String unit3old = "";

        static String svalue5old = "";
        static String svalue6old = "";

        double value1 = 0;
        double value2 = 0;
        double value3 = 0;

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        String dateformat = config->getString(config->dateFormat);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        // Get boat values for GPS time
        GwApi::BoatValue* bvalue1 = pageData.values[0]; // First element in list
        String name1 = bvalue1->getName().c_str();      // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        if (simulation == false) {
            value1 = bvalue1->value;                    // Value as double in SI unit
        } else {
            value1 = simtime++;                         // Simulation data for time value 11:36 in seconds
        }                                               // Other simulation data see OBP60Formatter.cpp
        bool valid1 = bvalue1->valid;                   // Valid information
        String svalue1 = formatValue(bvalue1, *commonData).svalue;    // Formatted value
        String unit1 = formatValue(bvalue1, *commonData).unit;        // Unit of value
        if (valid1 == true) {
            svalue1old = svalue1;                       // Save old value
            unit1old = unit1;                           // Save old unit
        }

        // Get boat values for GPS date
        GwApi::BoatValue* bvalue2 = pageData.values[1]; // Second element in list
        String name2 = bvalue2->getName().c_str();      // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        value2 = bvalue2->value;                        // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid informationgetdisplay().print("RTC");
        String svalue2 = formatValue(bvalue2, *commonData).svalue;    // Formatted value
        String unit2 = formatValue(bvalue2, *commonData).unit;        // Unit of value
        if (valid2 == true) {
            svalue2old = svalue2;                       // Save old value
            unit2old = unit2;                           // Save old unit
        }

        // Get boat values for HDOP
        GwApi::BoatValue* bvalue3 = pageData.values[2]; // Third element in list
        String name3 = bvalue3->getName().c_str();      // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        value3 = bvalue3->value;                        // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information
        String svalue3 = formatValue(bvalue3, *commonData).svalue;    // Formatted value
        String unit3 = formatValue(bvalue3, *commonData).unit;        // Unit of value
        if (valid3 == true) {
            svalue3old = svalue3;                       // Save old value
            unit3old = unit3;                           // Save old unit
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK;
        LOG_DEBUG(GwLog::LOG, "Drawing at PageClock, %s:%f,  %s:%f", name1.c_str(), value1, name2.c_str(), value2);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        getdisplay().setTextColor(commonData->fgcolor);

        time_t tv = mktime(&commonData->data.rtcTime) + timezone * 3600;
        struct tm* local_tm = localtime(&tv);

        if (mode == 'T') {
            // REGATTA TIMER MODE: countdown timer HH:MM:SS in the center with 7-segment font
            //*******************************************************************************

            int dispH = timerHours;
            int dispM = timerMinutes;
            int dispS = timerSeconds;

            // Update remaining time if timer is running (based on RTC)
            if (timerRunning && commonData->data.rtcValid) {
                struct tm rtcCopy = commonData->data.rtcTime;
                time_t nowEpoch = mktime(&rtcCopy);
                time_t remaining = timerEndEpoch - nowEpoch;
                if(remaining <= 5 && remaining != 0){
                    // Short pre buzzer alarm (100% power)
                    setBuzzerPower(100);
                    buzzer(TONE2, 75);
                    setBuzzerPower(config->getInt(config->buzzerPower));
                }
                if (remaining <= 0) {
                    remaining = 0;
                    timerRunning = false;
                    commonData->keydata[3].label = "-";
                    commonData->keydata[4].label = "START";
                    showSelectionMarker = true;
                    // Buzzer alarm (100% power)
                    setBuzzerPower(100);
                    buzzer(TONE2, 800);
                    setBuzzerPower(config->getInt(config->buzzerPower));

                    // When countdown is finished, restore the initial start time
                    timerHours = timerStartHours;
                    timerMinutes = timerStartMinutes;
                    timerSeconds = timerStartSeconds;
                }
                else{
                    commonData->keydata[3].label = "";
                    commonData->keydata[4].label = "RESET";
                }
                int rem = static_cast<int>(remaining);
                dispH = rem / 3600;
                rem -= dispH * 3600;
                dispM = rem / 60;
                dispS = rem % 60;
            }

            char buf[9]; // "HH:MM:SS"
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dispH, dispM, dispS);
            String timeStr = String(buf);

            // Clear central area and draw large digital time
            getdisplay().fillRect(0, 110, getdisplay().width(), 80, commonData->bgcolor);

            getdisplay().setFont(&DSEG7Classic_BoldItalic30pt7b);

            // Determine widths for digits and colon to position selection underline exactly
            int16_t x0, y0;
            uint16_t wDigit, hDigit;
            uint16_t wColon, hColon;

            getdisplay().getTextBounds("00", 0, 0, &x0, &y0, &wDigit, &hDigit);
            getdisplay().getTextBounds(":", 0, 0, &x0, &y0, &wColon, &hColon);

            uint16_t totalWidth = 3 * wDigit + 2 * wColon;

            int16_t baseX = (static_cast<int16_t>(getdisplay().width()) - static_cast<int16_t>(totalWidth)) / 2;
            int16_t centerY = 150;

            // Draw time string centered
            int16_t x1b, y1b;
            uint16_t wb, hb;
            getdisplay().getTextBounds(timeStr, 0, 0, &x1b, &y1b, &wb, &hb);
            int16_t textX = (static_cast<int16_t>(getdisplay().width()) - static_cast<int16_t>(wb)) / 2;
            int16_t textY = centerY + hb / 2;

            //getdisplay().setCursor(textX, textY); // horzontal jitter
            getdisplay().setCursor(47, textY);      // static X position
            getdisplay().print(timeStr);

            // Selection marker (only visible when not running and POS pressed)
            if (!timerRunning && showSelectionMarker) {
                int16_t selX = baseX - 8; // Hours start
                if (selectedField == 1) {
                    selX = baseX + wDigit + wColon; // Minutes start
                } else if (selectedField == 2) {
                    selX = baseX + 2 * wDigit + 2 * wColon + 12; // Seconds start
                }

                int16_t underlineY = centerY + hb / 2 + 5;
                //getdisplay().fillRect(selX, underlineY, wDigit, 6, commonData->fgcolor);
                getdisplay().fillRoundRect(selX, underlineY, wDigit, 6, 2, commonData->fgcolor);
            }

            // Page label
            getdisplay().setFont(&Ubuntu_Bold16pt8b);
            getdisplay().setCursor(100, 70);
            getdisplay().print("Regatta Timer");

        } else if (mode == 'D') {
            // DIGITAL CLOCK MODE: large 7-segment time based on GPS/RTC
            //**********************************************************

            int hour24 = 0;
            int minute24 = 0;
            int second24 = 0;

            if (source == 'R' && commonData->data.rtcValid) {
                time_t tv2 = mktime(&commonData->data.rtcTime);
                if (tz == 'L') {
                    tv2 += static_cast<time_t>(timezone * 3600);
                }
                struct tm* tm2 = localtime(&tv2);
                hour24 = tm2->tm_hour;
                minute24 = tm2->tm_min;
                second24 = tm2->tm_sec;
            } else {
                double t = value1;
                if (tz == 'L') {
                    t += timezone * 3600;
                }
                if (t >= 86400) t -= 86400;
                if (t < 0) t += 86400;
                hour24 = static_cast<int>(t / 3600.0);
                int rest = static_cast<int>(t) - hour24 * 3600;
                minute24 = rest / 60;
                second24 = rest % 60;
            }

            char buf[9]; // "HH:MM:SS"
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour24, minute24, second24);
            String timeStr = String(buf);

            getdisplay().fillRect(0, 110, getdisplay().width(), 80, commonData->bgcolor);

            getdisplay().setFont(&DSEG7Classic_BoldItalic30pt7b);

            int16_t x1b, y1b;
            uint16_t wb, hb;
            getdisplay().getTextBounds(timeStr, 0, 0, &x1b, &y1b, &wb, &hb);

            int16_t x = (static_cast<int16_t>(getdisplay().width()) - static_cast<int16_t>(wb)) / 2;
            int16_t y = 150 + hb / 2;

            //getdisplay().setCursor(x, y); // horizontal jitter
            getdisplay().setCursor(47, y);  // static X position
            getdisplay().print(timeStr); // Display actual time

            // Small indicators: timezone and source
            getdisplay().setFont(&Ubuntu_Bold8pt8b);

            getdisplay().setCursor(47, 110);
            if (source == 'G') {
                getdisplay().print("GPS");
            } else {
                getdisplay().print("RTC");
            }

            getdisplay().setCursor(47 + 40, 110);
            if (holdvalues == false) {
                getdisplay().print(tz == 'L' ? "LOT" : "UTC");
            } else {
                getdisplay().print(unit2old); // date unit
            }

            // Page label
            getdisplay().setFont(&Ubuntu_Bold16pt8b);
            getdisplay().setCursor(100, 70);
            getdisplay().print("Digital Clock");

        } else {
            // ANALOG CLOCK MODE (mode == 'A')
            //********************************

            // Show values GPS date
            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(10, 65);
            if (holdvalues == false) {
                if (source == 'G') {
                    // GPS value
                    getdisplay().print(svalue2);
                } else if (commonData->data.rtcValid) {
                    // RTC value
                    if (tz == 'L') {
                        getdisplay().print(formatDate(dateformat, local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday));
                    } else {
                        getdisplay().print(formatDate(dateformat, commonData->data.rtcTime.tm_year + 1900, commonData->data.rtcTime.tm_mon + 1, commonData->data.rtcTime.tm_mday));
                    }
                } else {
                    getdisplay().print("---");
                }
            } else {
                getdisplay().print(svalue2old);
            }
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(10, 95);
            getdisplay().print("Date");                          // Name

            // Horizontal separator left
            getdisplay().fillRect(0, 149, 60, 3, commonData->fgcolor);

            // Show values GPS time (small text bottom left)
            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(10, 250);
            if (holdvalues == false) {
                if (source == 'G') {
                    getdisplay().print(svalue1); // Value
                } else if (commonData->data.rtcValid) {
                    if (tz == 'L') {
                        getdisplay().print(formatTime('s', local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec));
                    } else {
                        getdisplay().print(formatTime('s', commonData->data.rtcTime.tm_hour, commonData->data.rtcTime.tm_min, commonData->data.rtcTime.tm_sec));
                    }
                } else {
                    getdisplay().print("---");
                }
            } else {
                getdisplay().print(svalue1old);
            }
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(10, 220);
            getdisplay().print("Time");                          // Name

            // Show values sunrise
            String sunrise = "---";
            if ((valid1 and valid2 and valid3 == true) or (homevalid and commonData->data.rtcValid)) {
                sunrise = String(commonData->sundata.sunriseHour) + ":" + String(commonData->sundata.sunriseMinute + 100).substring(1);
                svalue5old = sunrise;
            } else if (simulation) {
                sunrise = String("06:42");
            }

            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(335, 65);
            if (holdvalues == false) getdisplay().print(sunrise); // Value
            else getdisplay().print(svalue5old);
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(335, 95);
            getdisplay().print("SunR");                          // Name

            // Horizontal separator right
            getdisplay().fillRect(340, 149, 80, 3, commonData->fgcolor);

            // Show values sunset
            String sunset = "---";
            if ((valid1 and valid2 and valid3 == true) or (homevalid and commonData->data.rtcValid)) {
                sunset = String(commonData->sundata.sunsetHour) + ":" + String(commonData->sundata.sunsetMinute + 100).substring(1);
                svalue6old = sunset;
            } else if (simulation) {
                sunset = String("21:03");
            }

            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(335, 250);
            if (holdvalues == false) getdisplay().print(sunset);  // Value
            else getdisplay().print(svalue6old);
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(335, 220);
            getdisplay().print("SunS");                          // Name
            
            int rInstrument = 110;     // Radius of clock
            float pi = 3.141592;

            getdisplay().fillCircle(200, 150, rInstrument + 10, commonData->fgcolor);    // Outer circle
            getdisplay().fillCircle(200, 150, rInstrument + 7, commonData->bgcolor);     // Outer circle

            for (int i = 0; i < 360; i = i + 1)
            {
                // Scaling values
                float x = 200 + (rInstrument - 30) * sin(i / 180.0 * pi);  //  x-coordinate dots
                float y = 150 - (rInstrument - 30) * cos(i / 180.0 * pi);  //  y-coordinate dots
                const char* ii = "";
                switch (i)
                {
                case 0: ii = "12"; break;
                case 90: ii = "3"; break;
                case 180: ii = "6"; break;
                case 270: ii = "9"; break;
                default: break;
                }

                // Print text centered on position x, y
                int16_t x1c, y1c;     // Return values of getTextBounds
                uint16_t wc, hc;      // Return values of getTextBounds
                getdisplay().getTextBounds(ii, int(x), int(y), &x1c, &y1c, &wc, &hc); // Calc width of new string
                getdisplay().setCursor(x - wc / 2, y + hc / 2);
                if (i % 90 == 0) {
                    getdisplay().setFont(&Ubuntu_Bold12pt8b);
                    getdisplay().print(ii);
                }

                // Draw sub scale with dots
                float sinx = 0;
                float cosx = 0;
                if (i % 6 == 0) {
                    float x1d = 200 + rInstrument * sin(i / 180.0 * pi);
                    float y1d = 150 - rInstrument * cos(i / 180.0 * pi);
                    getdisplay().fillCircle((int)x1d, (int)y1d, 2, commonData->fgcolor);
                    sinx = sin(i / 180.0 * pi);
                    cosx = cos(i / 180.0 * pi);
                }

                // Draw sub scale with lines (two triangles)
                if (i % 30 == 0) {
                    float dx = 2;   // Line thickness = 2*dx+1
                    float xx1 = -dx;
                    float xx2 = +dx;
                    float yy1 = -(rInstrument - 10);
                    float yy2 = -(rInstrument + 10);
                    getdisplay().fillTriangle(200 + (int)(cosx * xx1 - sinx * yy1), 150 + (int)(sinx * xx1 + cosx * yy1),
                        200 + (int)(cosx * xx2 - sinx * yy1), 150 + (int)(sinx * xx2 + cosx * yy1),
                        200 + (int)(cosx * xx1 - sinx * yy2), 150 + (int)(sinx * xx1 + cosx * yy2), commonData->fgcolor);
                    getdisplay().fillTriangle(200 + (int)(cosx * xx2 - sinx * yy1), 150 + (int)(sinx * xx2 + cosx * yy1),
                        200 + (int)(cosx * xx1 - sinx * yy2), 150 + (int)(sinx * xx1 + cosx * yy2),
                        200 + (int)(cosx * xx2 - sinx * yy2), 150 + (int)(sinx * xx2 + cosx * yy2), commonData->fgcolor);
                }
            }

            // Print Unit in clock
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(175, 110);
            if (holdvalues == false) {
                getdisplay().print(tz == 'L' ? "LOT" : "UTC");
            } else {
                getdisplay().print(unit2old); // date unit
            }

            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(185, 190);
            if (source == 'G') {
                getdisplay().print("GPS");
            } else {
                getdisplay().print("RTC");
            }

            // Clock values
            double hour = 0;
            double minute = 0;
            if (source == 'R') {
                if (tz == 'L') {
                    time_t tv2 = mktime(&commonData->data.rtcTime) + timezone * 3600;
                    struct tm* local_tm2 = localtime(&tv2);
                    minute = local_tm2->tm_min;
                    hour = local_tm2->tm_hour;
                } else {
                    minute = commonData->data.rtcTime.tm_min;
                    hour = commonData->data.rtcTime.tm_hour;
                }
                hour += minute / 60;
            } else {
                if (tz == 'L') {
                    value1 += timezone * 3600;
                }
                if (value1 > 86400) { value1 -= 86400; }
                if (value1 < 0) { value1 += 86400; }
                hour = (value1 / 3600.0);
                // minute = (hour - int(hour)) * 3600.0 / 60.0;        // Analog minute pointer smooth moving
                minute = int((hour - int(hour)) * 3600.0 / 60.0);   // Jumping minute pointer from minute to minute
            }
            if (hour > 12) {
                hour -= 12.0;
            }
            LOG_DEBUG(GwLog::DEBUG, "... PageClock, value1: %f hour: %f minute:%f", value1, hour, minute);

            // Draw hour pointer
            float startwidth = 8;       // Start width of pointer
            if (valid1 == true || (source == 'R' && commonData->data.rtcValid) || holdvalues == true || simulation == true) {
                float sinx = sin(hour * 30.0 * pi / 180);     // Hour
                float cosx = cos(hour * 30.0 * pi / 180);
                // Normal pointer
                // Pointer as triangle with center base 2*width
                float xx1 = -startwidth;
                float xx2 = startwidth;
                float yy1 = -startwidth;
                float yy2 = -(rInstrument * 0.5);
                getdisplay().fillTriangle(200 + (int)(cosx * xx1 - sinx * yy1), 150 + (int)(sinx * xx1 + cosx * yy1),
                    200 + (int)(cosx * xx2 - sinx * yy1), 150 + (int)(sinx * xx2 + cosx * yy1),
                    200 + (int)(cosx * 0 - sinx * yy2), 150 + (int)(sinx * 0 + cosx * yy2), commonData->fgcolor);
                // Inverted pointer
                // Pointer as triangle with center base 2*width
                float endwidth = 2;         // End width of pointer
                float ix1 = endwidth;
                float ix2 = -endwidth;
                float iy1 = -(rInstrument * 0.5);
                float iy2 = -endwidth;
                getdisplay().fillTriangle(200 + (int)(cosx * ix1 - sinx * iy1), 150 + (int)(sinx * ix1 + cosx * iy1),
                    200 + (int)(cosx * ix2 - sinx * iy1), 150 + (int)(sinx * ix2 + cosx * iy1),
                    200 + (int)(cosx * 0 - sinx * iy2), 150 + (int)(sinx * 0 + cosx * iy2), commonData->fgcolor);
            }

            // Draw minute pointer
            startwidth = 8;       // Start width of pointer
            if (valid1 == true || (source == 'R' && commonData->data.rtcValid) || holdvalues == true || simulation == true) {
                float sinx = sin(minute * 6.0 * pi / 180);     // Minute
                float cosx = cos(minute * 6.0 * pi / 180);
                // Normal pointer
                // Pointer as triangle with center base 2*width
                float xx1 = -startwidth;
                float xx2 = startwidth;
                float yy1 = -startwidth;
                float yy2 = -(rInstrument - 15);
                getdisplay().fillTriangle(200 + (int)(cosx * xx1 - sinx * yy1), 150 + (int)(sinx * xx1 + cosx * yy1),
                    200 + (int)(cosx * xx2 - sinx * yy1), 150 + (int)(sinx * xx2 + cosx * yy1),
                    200 + (int)(cosx * 0 - sinx * yy2), 150 + (int)(sinx * 0 + cosx * yy2), commonData->fgcolor);
                // Inverted pointer
                // Pointer as triangle with center base 2*width
                float endwidth = 2;         // End width of pointer
                float ix1 = endwidth;
                float ix2 = -endwidth;
                float iy1 = -(rInstrument - 15);
                float iy2 = -endwidth;
                getdisplay().fillTriangle(200 + (int)(cosx * ix1 - sinx * iy1), 150 + (int)(sinx * ix1 + cosx * iy1),
                    200 + (int)(cosx * ix2 - sinx * iy1), 150 + (int)(sinx * ix2 + cosx * iy1),
                    200 + (int)(cosx * 0 - sinx * iy2), 150 + (int)(sinx * 0 + cosx * iy2), commonData->fgcolor);
            }

            // Center circle
            getdisplay().fillCircle(200, 150, startwidth + 6, commonData->bgcolor);
            getdisplay().fillCircle(200, 150, startwidth + 4, commonData->fgcolor);
        }

        return PAGE_UPDATE;
    };
};

// Static member definitions
bool PageClock::timerInitialized = false;
bool PageClock::timerRunning = false;
int PageClock::timerHours = 0;
int PageClock::timerMinutes = 0;
int PageClock::timerSeconds = 0;
int PageClock::timerStartHours = 0;
int PageClock::timerStartMinutes = 0;
int PageClock::timerStartSeconds = 0;
int PageClock::selectedField = 0;
bool PageClock::showSelectionMarker = true;
time_t PageClock::timerEndEpoch = 0;

static Page* createPage(CommonData& common)
{
    return new PageClock(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * we provide the number of user parameters we expect (0 here)
 * and we provide the names of the fixed values we need
 */
PageDescription registerPageClock(
    "Clock",             // Page name
    createPage,          // Action
    0,                   // Number of bus values depends on selection in Web configuration
    {"GPST", "GPSD", "HDOP"},   // Bus values we need in the page
    true                 // Show display header on/off
);

#endif

