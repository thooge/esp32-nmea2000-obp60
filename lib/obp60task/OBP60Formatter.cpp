#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include <Arduino.h>
#include "GwApi.h"
#include "Pagedata.h"

// ToDo
// simulation data
// hold values by missing data

String formatDate(String fmttype, uint16_t year, uint8_t month, uint8_t day) {
    char buffer[12];
    if (fmttype == "GB") {
        snprintf(buffer, 12, "%02d/%02d/%04d", day , month, year);
    }
    else if (fmttype == "US") {
        snprintf(buffer, 12, "%02d/%02d/%04d", month, day, year);
    }
    else if (fmttype == "ISO") {
        snprintf(buffer, 12, "%04d-%02d-%02d", year, month, day);
    }
    else {
        snprintf(buffer, 12, "%02d.%02d.%04d", day, month, year);
    }
    return String(buffer);
}

String formatTime(char fmttype, uint8_t hour, uint8_t minute, uint8_t second) {
    // fmttype: s: with seconds, m: only minutes
    char buffer[10];
    if (fmttype == 'm') {
        snprintf(buffer, 10, "%02d:%02d", hour , minute);
    }
    else {
        snprintf(buffer, 10, "%02d:%02d:%02d", hour, minute, second);
    }
    return String(buffer);
}

String formatLatitude(double lat) {
    float degree = abs(int(lat));
    float minute = abs((lat - int(lat)) * 60);
    return String(degree, 0) + "\x90 " + String(minute, 4) + "' " + ((lat > 0) ? "N" : "S");
}

String formatLongitude(double lon) {
    float degree = abs(int(lon));
    float minute = abs((lon - int(lon)) * 60);
    return String(degree, 0) + "\x90 " + String(minute, 4) + "' " + ((lon > 0) ? "E" : "W");
}

FormattedData formatValue(GwApi::BoatValue *value, CommonData &commondata){
    GwLog *logger = commondata.logger;
    FormattedData result;
    static int dayoffset = 0;
    double rawvalue = 0;

    // Load configuration values
    String stimeZone = commondata.config->getString(commondata.config->timeZone);               // [UTC -14.00...+12.00]
    double timeZone = stimeZone.toDouble();
    String lengthFormat = commondata.config->getString(commondata.config->lengthFormat);        // [m|ft]
    String distanceFormat = commondata.config->getString(commondata.config->distanceFormat);    // [m|km|nm]
    String speedFormat = commondata.config->getString(commondata.config->speedFormat);          // [m/s|km/h|kn]
    String windspeedFormat = commondata.config->getString(commondata.config->windspeedFormat);  // [m/s|km/h|kn|bft]
    String tempFormat = commondata.config->getString(commondata.config->tempFormat);            // [K|°C|°F]
    String dateFormat = commondata.config->getString(commondata.config->dateFormat);            // [DE|GB|US]
    bool usesimudata = commondata.config->getBool(commondata.config->useSimuData);              // [on|off]
    String precision = commondata.config->getString(commondata.config->valueprecision);         // [1|2]

    // If boat value not valid
    if (! value->valid && !usesimudata){
        result.svalue = "---";
        return result;
    }

    const char* fmt_dec_1;
    const char* fmt_dec_10;
    const char* fmt_dec_100;
    if (precision == "1") {
        fmt_dec_1 = "%3.1f";
        fmt_dec_10 = "%3.0f";
        fmt_dec_100 = "%3.0f";
    } else {
        fmt_dec_1 = "%3.2f";
        fmt_dec_10 = "%3.1f";
        fmt_dec_100 = "%3.0f";
    }

//    LOG_DEBUG(GwLog::DEBUG,"formatValue init: getFormat: %s date->value: %f time->value: %f", value->getFormat(), commondata.date->value, commondata.time->value);
    static const int bsize = 30;
    char buffer[bsize+1];
    buffer[0]=0;

    //########################################################
    // Formats for several boat data
    //########################################################
    if (value->getFormat() == "formatDate"){
        
        int dayoffset = 0;
        if (commondata.time->value + int(timeZone*3600) > 86400) {dayoffset = 1;}
        if (commondata.time->value + int(timeZone*3600) < 0) {dayoffset = -1;}

//        LOG_DEBUG(GwLog::DEBUG,"... formatDate value->value: %f tz: %f dayoffset: %d", value->value, timeZone, dayoffset);

        tmElements_t parts;
        time_t tv=tNMEA0183Msg::daysToTime_t(value->value + dayoffset);
        tNMEA0183Msg::breakTime(tv,parts);
        if (usesimudata == false) { 
            if (String(dateFormat) == "DE") {
                snprintf(buffer,bsize, "%02d.%02d.%04d", parts.tm_mday, parts.tm_mon+1, parts.tm_year+1900);
            }
            else if(String(dateFormat) == "GB") {
                snprintf(buffer, bsize, "%02d/%02d/%04d", parts.tm_mday, parts.tm_mon+1, parts.tm_year+1900);
            }
            else if(String(dateFormat) == "US") {
                snprintf(buffer, bsize, "%02d/%02d/%04d", parts.tm_mon+1, parts.tm_mday, parts.tm_year+1900);
            }
            else if(String(dateFormat) == "ISO") {
                snprintf(buffer, bsize, "%04d-%02d-%02d", parts.tm_year+1900, parts.tm_mon+1, parts.tm_mday);
            }
            else {
                snprintf(buffer, bsize, "%02d.%02d.%04d", parts.tm_mday, parts.tm_mon+1, parts.tm_year+1900);  
            }
        }
        else{
            snprintf(buffer, bsize, "01.01.2022"); 
        }
        if(timeZone == 0){
            result.unit = "UTC";
        }
        else{
            result.unit = "LOT";
        }
    }
    //########################################################
    else if(value->getFormat() == "formatTime"){
        double timeInSeconds = 0;
        double inthr = 0;
        double intmin = 0;
        double intsec = 0;
        double val = 0;

        timeInSeconds = value->value + int(timeZone * 3600);
        if (timeInSeconds > 86400) {timeInSeconds = timeInSeconds - 86400;}
        if (timeInSeconds <  0) {timeInSeconds = timeInSeconds + 86400;}
//        LOG_DEBUG(GwLog::DEBUG,"... formatTime value: %f tz: %f corrected timeInSeconds: %f ", value->value, timeZone, timeInSeconds);
        if (usesimudata == false) {
            val = modf(timeInSeconds/3600.0, &inthr);
            val = modf(val*3600.0/60.0, &intmin);
            modf(val*60.0,&intsec);
            snprintf(buffer, bsize, "%02.0f:%02.0f:%02.0f", inthr, intmin, intsec);
        }
        else{
            static long sec;
            static long lasttime;
            if(millis() > lasttime + 990){
                sec ++;
            }
            sec = sec % 60;
            snprintf(buffer, bsize, "11:36:%02i", int(sec));
            lasttime = millis();
        }
        if(timeZone == 0){
            result.unit = "UTC";
        }
        else{
            result.unit = "LOT";
        }
    }
    //########################################################
    else if (value->getFormat() == "formatFixed0"){
        if(usesimudata == false) {
            snprintf(buffer, bsize, "%3.0f", value->value);
            rawvalue = value->value;
        }
        else{
            rawvalue = 8.0 + float(random(0, 10)) / 10.0;
            snprintf(buffer, bsize, "%3.0f", rawvalue);
        }
        result.unit = "";
    }
    //########################################################
    else if (value->getFormat() == "formatCourse" || value->getFormat() == "formatWind"){
        double course = 0;
        if (usesimudata == false) {
            course = value->value;
            rawvalue = value->value;
        }
        else {
            course = 2.53 + float(random(0, 10) / 100.0);
            rawvalue = course;
        }
        course = course * 57.2958;      // Unit conversion form rad to deg

        // Format 3 numbers with prefix zero
        snprintf(buffer,bsize,"%03.0f",course);
        result.unit = "Deg";
    }
    //########################################################
    else if (value->getFormat() == "formatKnots" && (value->getName() == "SOG" || value->getName() == "STW")){
        double speed = 0;
        if (usesimudata == false) {
            speed = value->value;
            rawvalue = value->value;
        }
        else{
            rawvalue = 4.0 + float(random(0, 40));
            speed = rawvalue;
        }
        if (String(speedFormat) == "km/h"){
        speed = speed * 3.6;            // Unit conversion form m/s to km/h
            result.unit = "km/h";
        }
        else if (String(speedFormat) == "kn"){
            speed = speed * 1.94384;    // Unit conversion form m/s to kn
            result.unit = "kn";
        }
        else {
            speed = speed;              // Unit conversion form m/s to m/s
            result.unit = "m/s";
        }
        if(speed < 10) {
            snprintf(buffer, bsize, fmt_dec_1, speed);
        }
        else if (speed < 100) {
            snprintf(buffer, bsize, fmt_dec_10, speed);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, speed);
        }
    }
    //########################################################
    else if (value->getFormat() == "formatKnots" && (value->getName() == "AWS" || value->getName() == "TWS" || value->getName() == "MaxAws" || value->getName() == "MaxTws")){
        double speed = 0;
        if (usesimudata == false) {
            speed = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 4.0 + float(random(0, 40));
            speed = rawvalue;
        }
        if (String(windspeedFormat) == "km/h"){
            speed = speed * 3.6;        // Unit conversion form m/s to km/h
            result.unit = "km/h";
        }
        else if (String(windspeedFormat) == "kn"){
            speed = speed * 1.94384;      // Unit conversion form m/s to kn
            result.unit = "kn";
        }
        else if(String(windspeedFormat) == "bft"){
            if (speed < 0.3) {
                speed = 0;
            }
            else if (speed < 1.5) {
                speed = 1;
            }
            else if (speed < 3.3) {
                speed = 2;
            }
            else if (speed < 5.4) {
                speed = 3;
            }
            else if (speed < 7.9) {
                speed = 4;
            }
            else if (speed < 10.7) {
                speed = 5;
            }
            else if (speed < 13.8) {
                speed = 6;
            }
            else if (speed < 17.1) {
                speed = 7;
            }
            else if (speed < 20.7) {
                speed = 8;
            }
            else if (speed < 24.4) {
                speed = 9;
            }
            else if (speed < 28.4) {
                speed = 10;
            }
            else if (speed < 32.6) {
                speed = 11;
            }
            else {
                speed = 12;
            }
            result.unit = "bft";
        }
        else{
            speed = speed;              // Unit conversion form m/s to m/s
            result.unit = "m/s";
        }
        if (String(windspeedFormat) == "bft"){
            snprintf(buffer, bsize, "%2.0f", speed);
        }
        else{
            if (speed < 10){
                snprintf(buffer, bsize, fmt_dec_1, speed);
            }
            else if (speed < 100){
                snprintf(buffer, bsize, fmt_dec_10, speed);
            }
            else {
                snprintf(buffer, bsize, fmt_dec_100, speed);
            }
        }
    }
    //########################################################
    else if (value->getFormat() == "formatRot"){
        double rotation = 0;
        if (usesimudata == false) {
            rotation = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 0.04 + float(random(0, 10)) / 100.0;
            rotation = rawvalue;
        }
        rotation = rotation * 57.2958;      // Unit conversion form rad/s to deg/s
        result.unit = "Deg/s";
        if (rotation < -100){
            rotation = -99;
        }
        if (rotation > 100){
            rotation = 99;
        }
        if (rotation > -10 && rotation < 10){
            snprintf(buffer, bsize, "%3.2f", rotation);
        }
        if (rotation <= -10 || rotation >= 10){
            snprintf(buffer, bsize, "%3.0f", rotation);
        }
    }
    //########################################################
    else if (value->getFormat() == "formatDop"){
        double dop = 0;
        if (usesimudata == false) {
            dop = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 2.0 + float(random(0, 40)) / 10.0;
            dop = rawvalue;
        }
        result.unit = "m";
        if (dop > 99.9){
            dop = 99.9;
        }
        if (dop < 10){
            snprintf(buffer, bsize, fmt_dec_1, dop);
        }
        else if(dop < 100){
            snprintf(buffer, bsize, fmt_dec_10, dop);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, dop);
        }
    }
    //########################################################
    else if (value->getFormat() == "formatLatitude"){
        if (usesimudata == false) {
            double lat = value->value;
            rawvalue = value->value;
            String latitude = "";
            String latdir = "";
            float degree = abs(int(lat));
            float minute = abs((lat - int(lat)) * 60);
            if (lat > 0){
                latdir = "N";
            }
            else {
                latdir = "S";
            }
            latitude = String(degree,0) + "\x90 " + String(minute,4) + "' " + latdir;
            result.unit = "";
            strcpy(buffer, latitude.c_str());
        }
        else{
            rawvalue = 35.0 + float(random(0, 10)) / 10000.0;
            snprintf(buffer, bsize, " 51\" %2.4f' N", rawvalue);
        }
    }
    //########################################################
    else if (value->getFormat() == "formatLongitude"){
        if (usesimudata == false) {
            double lon = value->value;
            rawvalue = value->value;
            String longitude = "";
            String londir = "";
            float degree = abs(int(lon));
            float minute = abs((lon - int(lon)) * 60);
            if (lon > 0){
                londir = "E";
            }
            else {
                londir = "W";
            }
            longitude = String(degree,0) + "\x90 " + String(minute,4) + "' " + londir;
            result.unit = "";
            strcpy(buffer, longitude.c_str());
        }
        else {
            rawvalue = 6.0 + float(random(0, 10)) / 100000.0;
            snprintf(buffer, bsize, " 15\" %2.4f'", rawvalue);
        }
    }
    //########################################################
    else if (value->getFormat() == "formatDepth"){
        double depth = 0;
        if (usesimudata == false) {
            depth = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 18.0 + float(random(0, 100)) / 10.0;
            depth = rawvalue;
        }
        if(String(lengthFormat) == "ft"){
            depth = depth * 3.28084;
            result.unit = "ft";
        }
        else{
            result.unit = "m";
        }
        if (depth < 10) {
            snprintf(buffer, bsize, fmt_dec_1, depth);
        }
        else if (depth < 100){
            snprintf(buffer, bsize, fmt_dec_10, depth);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, depth);
        }
    }
    //########################################################
    else if (value->getFormat() == "formatXte"){
        double xte = 0;
        if (!usesimudata) {
            xte = abs(value->value);
            rawvalue = value->value;
        } else {
            rawvalue = 6.0 + float(random(0, 4));
            xte = rawvalue;
        }
        if (xte >= 100) {
            snprintf(buffer, bsize, fmt_dec_100, value->value);
        } else if (xte >= 10) {
            snprintf(buffer, bsize, fmt_dec_10, value->value);
        } else {
            snprintf(buffer, bsize, fmt_dec_1, value->value);
        }
        result.unit = "nm";
    }
    //########################################################
    else if (value->getFormat() == "kelvinToC"){
        double temp = 0;
        if (usesimudata == false) {
            temp = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 296.0 + float(random(0, 10)) / 10.0;
            temp = rawvalue;
        }
        if (String(tempFormat) == "C") {
            temp = temp - 273.15;
            result.unit = "C";
        }
        else if (String(tempFormat) == "F") {
            temp = (temp - 273.15) * 9 / 5 + 32;
            result.unit = "F";
        }
        else{
            result.unit = "K";
        }
        if(temp < 10) {
            snprintf(buffer, bsize, fmt_dec_1, temp);
        }
        else if (temp < 100) {
            snprintf(buffer, bsize, fmt_dec_10, temp);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, temp);
        }
    }
    //########################################################
    else if (value->getFormat() == "mtr2nm"){
        double distance = 0;
        if (usesimudata == false) {
            distance = value->value;
            rawvalue = value->value;
        }
        else{
            rawvalue = 2960.0 + float(random(0, 10));
            distance = rawvalue;
        }
        if (String(distanceFormat) == "km") {
            distance = distance * 0.001;
            result.unit = "km";
        }
        else if (String(distanceFormat) == "nm") {
            distance = distance * 0.000539957;
            result.unit = "nm";
        }
        else {
            result.unit = "m";
        }
        if (distance < 10){
            snprintf(buffer, bsize, fmt_dec_1, distance);
        }
        else if (distance < 100){
            snprintf(buffer, bsize, fmt_dec_10, distance);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, distance);
        }
    }
    //########################################################
    // Special XDR formats
    // Refer XDR formats in GwXDRMappings.cpp line 40
    //########################################################
     else if (value->getFormat() == "formatXdr:P:P"){
        double pressure = 0;
        if (usesimudata == false) {
            pressure = value->value;
            rawvalue = value->value;
            pressure = pressure / 100.0;        // Unit conversion form Pa to hPa
        }
        else {
            rawvalue = 968 + float(random(0, 10));
            pressure = rawvalue;
        }
        snprintf(buffer, bsize, "%4.0f", pressure);
        result.unit = "hPa";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:P:B"){
        double pressure = 0;
        if (usesimudata == false) {
            pressure = value->value;
            rawvalue = value->value;
            pressure = pressure / 100.0;      // Unit conversion form Pa to mBar
        }
        else {
            rawvalue = value->value;
            pressure = 968 + float(random(0, 10));
        }
        snprintf(buffer, bsize, "%4.0f", pressure);
        result.unit = "mBar";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:U:V"){
        double voltage = 0;
        if (usesimudata == false) {
            voltage = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 12 + float(random(0, 30)) / 10.0;
            voltage = rawvalue;
        }
        if (voltage < 10) {
            snprintf(buffer, bsize, fmt_dec_1, voltage);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_10, voltage);
        }
        result.unit = "V";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:I:A"){
        double current = 0;
        if (usesimudata == false) {
            current = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 8.2 + float(random(0, 50)) / 10.0;
            current = rawvalue;
        }
        if (current < 10) {
            snprintf(buffer, bsize, fmt_dec_1, current);
        }
        else if(current < 100) {
            snprintf(buffer, bsize, fmt_dec_10, current);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, current);
        }
        result.unit = "A";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:C:K"){
        double temperature = 0;
        if (usesimudata == false) {
            temperature = value->value - 273.15;    // Convert K to C
            rawvalue = value->value - 273.15;
        }
        else {
            rawvalue = 21.8 + float(random(0, 50)) / 10.0;
            temperature = rawvalue;
        }
        if (temperature < 10) {
            snprintf(buffer, bsize, fmt_dec_1, temperature);
        }
        else if (temperature < 100) {
            snprintf(buffer, bsize, fmt_dec_10, temperature);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, temperature);
        }
        result.unit = "Deg C";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:C:C"){
        double temperature = 0;
        if (usesimudata == false) {
            temperature = value->value;    // Value in C
            rawvalue = value->value;
        }
        else {
            rawvalue = 21.8 + float(random(0, 50)) / 10.0;
            temperature = rawvalue;
        }
        if (temperature < 10) {
            snprintf(buffer, bsize, fmt_dec_1, temperature);
        }
        else if(temperature < 100) {
            snprintf(buffer, bsize, fmt_dec_10, temperature);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, temperature);
        }
        result.unit = "Deg C";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:H:P"){
        double humidity = 0;
        if (usesimudata == false) {
            humidity = value->value;    // Value in %
            rawvalue = value->value;
        }
        else{
            rawvalue = 41.3 + float(random(0, 50)) / 10.0;
            humidity = rawvalue;
        }
        if (humidity < 10) {
            snprintf(buffer, bsize, fmt_dec_1, humidity);
        }
        else if(humidity < 100) {
            snprintf(buffer, bsize, fmt_dec_10, humidity);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, humidity);
        }
        result.unit = "%";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:V:P"){
        double volume = 0;
        if (usesimudata == false) {
            volume = value->value;    // Value in %
            rawvalue = value->value;
        }
        else {
            rawvalue = 85.8 + float(random(0, 50)) / 10.0;
            volume = rawvalue;
        }
        if (volume < 10) {
            snprintf(buffer, bsize, fmt_dec_1, volume);
        }
        else if (volume < 100) {
            snprintf(buffer, bsize, fmt_dec_10, volume);
        }
        else if (volume >= 100) {
            snprintf(buffer, bsize, fmt_dec_100, volume);
        }
        result.unit = "%";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:V:M"){
        double volume = 0;
        if (usesimudata == false) {
            volume = value->value;    // Value in l
            rawvalue = value->value;
        }
        else {
            rawvalue = 75.2 + float(random(0, 50)) / 10.0;
            volume = rawvalue;
        }
        if (volume < 10) {
            snprintf(buffer, bsize, fmt_dec_1, volume);
        }
        else if (volume < 100) {
            snprintf(buffer, bsize, fmt_dec_10, volume);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, volume);
        }
        result.unit = "l";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:R:I"){
        double flow = 0;
        if (usesimudata == false) {
            flow = value->value;    // Value in l/min
            rawvalue = value->value;
        }
        else {
            rawvalue = 7.5 + float(random(0, 20)) / 10.0;
            flow = rawvalue;
        }
        if (flow < 10) {
            snprintf(buffer, bsize, fmt_dec_1, flow);
        }
        else if (flow < 100) {
            snprintf(buffer, bsize, fmt_dec_10, flow);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, flow);
        }
        result.unit = "l/min";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:G:"){
        double generic = 0;
        if (usesimudata == false) {
            generic = value->value;
            rawvalue = value->value;
        }
        else {
            rawvalue = 18.5 + float(random(0, 20)) / 10.0;
            generic = rawvalue;
        }
        if (generic < 10) {
            snprintf(buffer, bsize, fmt_dec_1, generic);
        }
        else if (generic < 100) {
            snprintf(buffer, bsize, fmt_dec_10, generic);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, generic);
        }
        result.unit = "";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:A:P"){
        double dplace = 0;
        if (usesimudata == false) {
            dplace = value->value;    // Value in %
            rawvalue = value->value;
        }
        else {
            rawvalue = 55.3 + float(random(0, 20)) / 10.0;
            dplace = rawvalue;
        }
        if (dplace < 10) {
            snprintf(buffer, bsize, fmt_dec_1, dplace);
        }
        else if (dplace < 100) {
            snprintf(buffer, bsize, fmt_dec_10, dplace);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, dplace);
        }
        result.unit = "%";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:A:D"){
        double angle = 0;
        if (usesimudata == false) {
            angle = value->value;
            angle = angle * 57.2958;      // Unit conversion form rad to deg
            rawvalue = value->value;
        }
        else {
            rawvalue = PI / 100 + (random(-5, 5) / 360 * 2* PI);
            angle = rawvalue * 57.2958;
        }
        if (angle > -10 && angle < 10) {
            snprintf(buffer,bsize,"%3.1f",angle);
        }
        else {
            snprintf(buffer,bsize,"%3.0f",angle);
        }
        result.unit = "Deg";
    }
    //########################################################
    else if (value->getFormat() == "formatXdr:T:R"){
        double rpm = 0;
        if (usesimudata == false) {
            rpm = value->value;    // Value in rpm
            rawvalue = value->value;
        }
        else {
            rawvalue = 2505 + random(0, 20);
            rpm = rawvalue;
        }
        if (rpm < 10) {
            snprintf(buffer, bsize, fmt_dec_1, rpm);
        }
        else if (rpm < 100) {
            snprintf(buffer, bsize, fmt_dec_10, rpm);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, rpm);
        }
        result.unit = "rpm";
    }
    //########################################################
    // Default format
    //########################################################
    else {
        if (value->value < 10) {
            snprintf(buffer, bsize, fmt_dec_1, value->value);
        }
        else if (value->value < 100) {
            snprintf(buffer, bsize, fmt_dec_10, value->value);
        }
        else {
            snprintf(buffer, bsize, fmt_dec_100, value->value);
        }
        result.unit = "";
    }
    buffer[bsize] = 0;
    result.value = rawvalue;        // Return value is only necessary in case of simulation of graphic pointer
    result.svalue = String(buffer);
    return result;
}

#endif
