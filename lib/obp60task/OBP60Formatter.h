// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _OBP60FORMATTER_H
#define _OBP60FORMATTER_H

/*

Formatter names as defined in BoatItemBase
    formatCourse
    formatKnots
    formatWind
    formatLatitude
    formatLongitude
    formatXte
    formatFixed0
    formatDepth
    kelvinToC      TODO not a format but conversion
    mtr2nm         TODO not a format but conversion
    formatDop      dilution of precision
    formatRot
    formatDate
    formatTime
    formatName

XDR Formatter names
    formatXdr:P:P // pressure percent
    formatXdr:P:B // pressure bar
    formatXdr:U:V // voltage volt
    formatXdr:I:A // current ampere
    formatXdr:C:K // temperature kelvin
    formatXdr:C:C // temperature celsius
    formatXdr:H:P // humidity percent
    formatXdr:V:P // volume percent
    formatXdr:V:M // volume cubic meters
    formatXdr:R:I // flow liter per second?
    formatXdr:G:  // generic
    formatXdr:A:P // angle percent
    formatXdr:A:D // angle degrees
    formatXdr:T:R // tachometer rpm

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
    bool usesimudata = false;      // [on|off]

    String precision = "2";        // [1|2]
    const char* fmt_dec_1;
    const char* fmt_dec_10;
    const char* fmt_dec_100;

public:
    Formatter(GwConfigHandler *config);
    FormattedData formatValue(GwApi::BoatValue *value, CommonData &commondata);
    String placeholder = "---";
};

// Standard format functions without overhead
String formatDate(String fmttype, uint16_t year, uint8_t month, uint8_t day);
String formatTime(char fmttype, uint8_t hour, uint8_t minute, uint8_t second);
String formatLatitude(double lat);
String formatLongitude(double lon);

#endif
