// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _OBP60FORMATTER_H
#define _OBP60FORMATTER_H

#include "GwApi.h"
#include "Pagedata.h"
#include <unordered_map>

/*

XDR types
    A 	Angular displacement
    C 	Temperature
    D 	Linear displacement
    F 	Frequency
    G   Generic
    H 	Humidity
    I   Current
    L   Salinity
    N 	Force
    P 	Pressure
    R 	Flow
    S   Switch or valve
    T   Tachometer
    U   Voltage
    V   Volume

XDR units
    A   Ampere 
    B 	Bar
    C 	Celsius
    D 	Degrees
    H   Hertz
    I   Liter per second?
    M 	Meter / Cubic meter
    N 	Newton
    P 	Percent
    R   RPM
    V   Volt
 
 */

enum class fmtType {
    // Formatter names as defined in BoatItemBase
    COURSE,
    KNOTS,
    WIND,
    LATITUDE,
    LONGITUDE,
    XTE,
    FIXED0,
    DEPTH,
    DOP,       // dilution of precision
    ROT,
    DATE,
    TIME,
    NAME,

    kelvinToC, // TODO not a format but conversion
    mtr2nm,    // TODO not a format but conversion

    // XDR Formatter names
    XDR_PP,     // pressure percent
    XDR_PB,     // pressure bar
    XDR_UV,     // voltage volt
    XDR_IA,     // current ampere
    XDR_CK,     // temperature kelvin
    XDR_CC,     // temperature celsius
    XDR_HP,     // humidity percent
    XDR_VP,     // volume percent
    XDR_VM,     // volume cubic meters
    XDR_RI,     // flow liter per second?
    XDR_G,      // generic
    XDR_AP,     // angle percent
    XDR_AD,     // angle degrees
    XDR_TR      // tachometer rpm
};

// Hint: String is not supported
static std::unordered_map<const char*, fmtType> formatMap PROGMEM = {
    {"formatCourse", fmtType::COURSE},
    {"formatKnots", fmtType::KNOTS},
    {"formatWind", fmtType::WIND},
    {"formatLatitude", fmtType::LATITUDE},
    {"formatLongitude", fmtType::LONGITUDE},
    {"formatXte", fmtType::XTE},
    {"formatFixed0", fmtType::FIXED0},
    {"formatDepth", fmtType::DEPTH},
    {"formatDop", fmtType::DOP},
    {"formatRot", fmtType::ROT},
    {"formatDate", fmtType::DATE},
    {"formatTime", fmtType::TIME},
    {"formatName", fmtType::NAME},
    {"kelvinToC", fmtType::kelvinToC},
    {"mtr2nm", fmtType::mtr2nm},
    {"formatXdr:P:P", fmtType::XDR_PP},
    {"formatXdr:P:B", fmtType::XDR_PB},
    {"formatXdr:U:V", fmtType::XDR_UV},
    {"formatXdr:I:A", fmtType::XDR_IA},
    {"formatXdr:C:K", fmtType::XDR_CK},
    {"formatXdr:C:C", fmtType::XDR_CC},
    {"formatXdr:H:P", fmtType::XDR_HP},
    {"formatXdr:V:P", fmtType::XDR_VP},
    {"formatXdr:V:M", fmtType::XDR_VM},
    {"formatXdr:R:I", fmtType::XDR_RI},
    {"formatXdr:G:", fmtType::XDR_G},
    {"formatXdr:A:P", fmtType::XDR_AP},
    {"formatXdr:A:D", fmtType::XDR_AD},
    {"formatXdr:T:R", fmtType::XDR_TR}
};

// Possible formats as scoped enums
enum class fmtDate {DE, GB, US, ISO};
enum class fmtTime {MMHH, MMHHSS};
enum class fmtLength {METER, FEET, FATHOM, CABLE};
enum class fmtDepth {METER, FEET, FATHOM};
enum class fmtWind {KMH, MS, KN, BFT};
enum class fmtCourse {DEG, RAD};
enum class fmtRot {DEGS, RADS};
enum class fmtXte {M, KM, NM, CABLE};
enum class fmtPress {PA, BAR};
enum class fmtTemp {KELVIN, CELSUIS, FAHRENHEIT};

// Conversion factors
#define CONV_M_FT 3.2808399 // meter too feet
#define CONV_M_FM 0.5468    // meter to fathom
#define CONV_M_CBL 0.0053961182483768 // meter to cable
#define CONV_CBL_FT 608    // cable to feet
#define CONV_FM_FT 6       // fathom to feet

// Structure for formatted boat values
typedef struct {
    double value;
    String svalue;
    String unit;
} FormattedData;

// Formatter for boat values
class Formatter {
private:
    String stimeZone = "0";
    double timeZone = 0.0;         // [UTC -14.00...+12.00]
    String lengthFormat = "m";     // [m|ft]
    String distanceFormat = "nm";  // [m|km|nm]
    String speedFormat = "kn";     // [m/s|km/h|kn]
    String windspeedFormat = "kn"; // [m/s|km/h|kn|bft]
    String tempFormat = "C";       // [K|°C|°F]
    String dateFormat = "ISO";     // [DE|GB|US|ISO]
    fmtDate dateFmt;
    bool usesimudata = false;      // [on|off]

    String precision = "2";        // [1|2]
    const char* fmt_dec_1;
    const char* fmt_dec_10;
    const char* fmt_dec_100;

public:
    Formatter(GwConfigHandler *config);
    fmtType stringToFormat(const char* formatStr);
    fmtDate getDateFormat(String sformat);
    fmtTime getTimeFormat(String sformat);
    FormattedData formatValue(GwApi::BoatValue *value, CommonData &commondata);
    String placeholder = "---";
};

// Standard format functions without class and overhead
String formatDate(fmtDate fmttype, uint16_t year, uint8_t month, uint8_t day);
String formatTime(fmtTime fmttype, uint8_t hour, uint8_t minute, uint8_t second);
String formatLatitude(double lat);
String formatLongitude(double lon);

#endif
