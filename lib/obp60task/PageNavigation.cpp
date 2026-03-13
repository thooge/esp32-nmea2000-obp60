#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "NetworkClient.h"      // Network connection
#include "ImageDecoder.h"       // Image decoder for navigation map
#include <mbedtls/base64.h>

#include "Logo_OBP_400x300_sw.h"

// Defines for reading of navigation map 
#define JSON_BUFFER 30000       // Max buffer size for JSON content (30 kB picture + values)
NetworkClient net(JSON_BUFFER); // Define network client
ImageDecoder decoder;           // Define image decoder        

#ifdef DISPLAY_ST7796
// Set to true to render a generated RGB565 color-bar test image.
static constexpr bool kShowRgb565StripeTestImage = false;

static void drawRgb565Image(int16_t x, int16_t y, const uint16_t *img, int16_t w, int16_t h) {
    if (img == nullptr || w <= 0 || h <= 0) {
        return;
    }
    for (int16_t yy = 0; yy < h; ++yy) {
        const uint16_t *row = img + ((size_t)yy * (size_t)w);
        for (int16_t xx = 0; xx < w; ++xx) {
            getdisplay().drawPixel(x + xx, y + yy, row[xx]);
        }
        if ((yy & 0x0F) == 0) {
            yield();
        }
    }
}

static void createRgb565StripeImage(uint16_t *img, int16_t w, int16_t h) {
    if (img == nullptr || w <= 0 || h <= 0) {
        return;
    }
    static const uint16_t stripes[] = {
        0xF800, // red
        0xFD20, // orange
        0xFFE0, // yellow
        0x07E0, // green
        0x07FF, // cyan
        0x001F, // blue
        0xF81F, // magenta
        0xFFFF  // white
    };
    const int stripeCount = (int)(sizeof(stripes) / sizeof(stripes[0]));

    for (int16_t y = 0; y < h; ++y) {
        uint16_t *row = img + ((size_t)y * (size_t)w);
        for (int16_t x = 0; x < w; ++x) {
            int idx = ((int)x * stripeCount) / (int)w;
            if (idx >= stripeCount) {
                idx = stripeCount - 1;
            }
            row[x] = stripes[idx];
        }
    }
}
#endif

class PageNavigation : public Page
{
// Values for buttons
bool firstRun = true;    // Detect the first page run
int zoom = 15;           // Default zoom level
bool showValues = false; // Show values HDT, SOG, DBT in navigation map

    private:
    uint8_t* imageBackupData = nullptr;
    int imageBackupWidth = 0;
    int imageBackupHeight = 0;
    size_t imageBackupSize = 0;
    size_t imageBackupCapacity = 0;
    bool hasImageBackup = false;
    bool imageBackupIsRgb565 = false;

    public:
    PageNavigation(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageNavigation");
        imageBackupCapacity = (size_t)GxEPD_WIDTH * (size_t)GxEPD_HEIGHT;
        #ifdef DISPLAY_ST7796
        imageBackupCapacity *= 2U;
        #endif
        imageBackupData = (uint8_t*)heap_caps_malloc(imageBackupCapacity, MALLOC_CAP_SPIRAM);
    }

    // Set botton labels
    virtual void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "ZOOM -";
        commonData->keydata[1].label = "ZOOM +";
        commonData->keydata[4].label = "VALUES";
    }
    
    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        // Code for zoom -
        if(key == 1){
            zoom --;                    // Zoom -
            if(zoom <7){
                zoom = 7;
            }
            return 0;                   // Commit the key
        }
        // Code for zoom -
        if(key == 2){
            zoom ++;                    // Zoom +
            if(zoom >17){
                zoom = 17;
            }
            return 0;                   // Commit the key
        }
        if(key == 5){
            showValues = !showValues;   // Toggle show values
            return 0;                   // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
        String mapsource = config->getString(config->mapsource);
        String ipAddress = config->getString(config->ipAddress);
        int localPort = config->getInt(config->localPort);
        String mapType = config->getString(config->maptype);
        int zoomLevel = config->getInt(config->zoomlevel);
        bool grid = config->getBool(config->grid);
        String orientation = config->getString(config->orientation);
        int refreshDistance = config->getInt(config->refreshDistance);
        bool showValuesMap = config->getBool(config->showvalues);
        bool ownHeading = config->getBool(config->ownheading);

        if(firstRun == true){
            zoom = zoomLevel;           // Over write zoom level with setup value
            showValues = showValuesMap; // Over write showValues with setup value 
            firstRun = false;           // Restet variable
        }

        // Local variables
        String server = "norbert-walter.dnshome.de";
        int port = 80;
        int mType = 1;
        int dType = 1;
        int mapRot = 0;
        int symbolRot = 0;
        int mapGrid = 0;

        
        // Old values for hold function
        static double value1old = 0;
        static String svalue1old = "";
        static String unit1old = "";
        static double value2old = 0;
        static String svalue2old = "";
        static String unit2old = "";
        static double value3old = 0;    // Deg
        static String svalue3old = "";
        static String unit3old = "";
        static double value4old = 0;
        static String svalue4old = "";
        static String unit4old = "";
        static double value5old = 0;
        static String svalue5old = "";
        static String unit5old = "";
        static double value6old = 0;
        static String svalue6old = "";
        static String unit6old = "";

        static double latitude = 0;
        static double latitudeold = 0;
        static double longitude = 0;
        static double longitudeold = 0;
        static double trueHeading = 0;
        static double magneticHeading = 0;
        static double speedOverGround = 0;
        static double depthBelowTransducer = 0;
        static int lostCounter = 0; // Counter for connection lost to the map server (increment by each page refresh)
        int imgWidth = 0;
        int imgHeight = 0;

        // Get boat values #1 Latitude
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Get boat values #2 Longitude
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list (only one value by PageOneValue)
        String name2 = xdrDelete(bvalue2->getName());   // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = formatValue(bvalue2, *commonData).unit;        // Unit of value

        // Get boat values #3 HDT
        GwApi::BoatValue *bvalue3 = pageData.values[2]; // Second element in list (only one value by PageOneValue)
        String name3 = xdrDelete(bvalue3->getName());   // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        double value3 = bvalue3->value;                 // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information 
        String svalue3 = formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = formatValue(bvalue3, *commonData).unit;        // Unit of value

        // Get boat values #4 HDM
        GwApi::BoatValue *bvalue4 = pageData.values[3]; // Second element in list (only one value by PageOneValue)
        String name4 = xdrDelete(bvalue4->getName());   // Value name
        name4 = name4.substring(0, 6);                  // String length limit for value name
        double value4 = bvalue4->value;                 // Value as double in SI unit
        bool valid4 = bvalue4->valid;                   // Valid information 
        String svalue4 = formatValue(bvalue4, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit4 = formatValue(bvalue4, *commonData).unit;        // Unit of value

        // Get boat values #5 SOG
        GwApi::BoatValue *bvalue5 = pageData.values[4]; // Second element in list (only one value by PageOneValue)
        String name5 = xdrDelete(bvalue5->getName());   // Value name
        name5 = name5.substring(0, 6);                  // String length limit for value name
        double value5 = bvalue5->value;                 // Value as double in SI unit
        bool valid5 = bvalue5->valid;                   // Valid information 
        String svalue5 = formatValue(bvalue5, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit5 = formatValue(bvalue5, *commonData).unit;        // Unit of value

        // Get boat values #6 DBT
        GwApi::BoatValue *bvalue6 = pageData.values[5]; // Second element in list (only one value by PageOneValue)
        String name6 = xdrDelete(bvalue6->getName());   // Value name
        name6 = name6.substring(0, 6);                  // String length limit for value name
        double value6 = bvalue6->value;                 // Value as double in SI unit
        bool valid6 = bvalue6->valid;                   // Valid information 
        String svalue6 = formatValue(bvalue6, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit6 = formatValue(bvalue6, *commonData).unit;        // Unit of value

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        LOG_DEBUG(GwLog::LOG,"Drawing at PageNavigation, %s: %f, %s: %f, %s: %f, %s: %f, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3, name4.c_str(), value4, name5.c_str(), value5, name6.c_str(), value6);

        // Set variables
        //***********************************************************

        // Latitude
        if(valid1){
            latitude = value1;
            latitudeold = value1;
            value3old = value1;
        }
        else{
            latitude = value1old;
        }
        // Longitude
        if(valid2){
            longitude = value2;
            longitudeold = value2;
            value2old = value2;
        }
        else{
            longitude = value2old;
        }
        // HDT value (True Heading, GPS)
        if(valid3){
            trueHeading = (value3 * 360) / (2 * PI);
            value3old = trueHeading;
        }
        else{
            trueHeading = value3old;
        }
        // HDM value (Magnetic Heading)
        if(valid4){
            magneticHeading = (value4 * 360) / (2 * PI);
            value4old = magneticHeading;
        }
        else{
            speedOverGround = value4old;
        }
        // SOG value (Speed Over Ground)
        if(valid5){
            speedOverGround = value5;
            value5old = value5;
        }
        else{
            speedOverGround = value5old;
        }
        // DBT value (Depth Below Transducer)
        if(valid6){
            depthBelowTransducer = value6;
            value6old = value6;
        }
        else{
            depthBelowTransducer = value6old;
        }

        // Prepare config values for URL
        //***********************************************************

        // Server settings
        if(mapsource == "OBP Service"){
            server = "norbert-walter.dnshome.de";
            port = 80;
        }
        else if(mapsource == "Local Service"){
            server = String(ipAddress);
            port = localPort;
        }
        else{
            server = "norbert-walter.dnshome.de";
            port = 80;
        }

        // Type of navigation map
        if(mapType == "Open Street Map"){
            mType = 1;  // Map type
            dType = 1;  // Dithering type
        }
        else if(mapType == "Google Street"){
            mType = 3;
            dType = 2;
        }
        else if(mapType == "Open Topo Map"){
            mType = 5;
            dType = 2;
        }
        else if(mapType == "Stadimaps Toner"){
            mType = 7;
            dType = 1;
        }
        else if(mapType == "Free Nautical Chart"){
            mType = 9;
            dType = 1;
        }
        else{
            mType = 1;
            dType = 1;
        }

        // Map grid on/off
        if(grid == true){
            mapGrid = 1;
        }
        else{
            mapGrid = 0;
        }

        // Map orientation
        if(orientation == "North Direction"){
            mapRot = 0;
            // If true heading available then use HDT oterwise HDM
            if(valid3 == true){
                symbolRot = trueHeading;
            }
            else{
                symbolRot = magneticHeading;
            }
        }
        else if(orientation == "Travel Direction"){
            // If true heading available then use HDT oterwise HDM
            if(valid3 == true){
                mapRot = trueHeading;
                symbolRot = trueHeading;
            }
            else{
                mapRot = magneticHeading;
                symbolRot = magneticHeading;
            }
        }
        else{
            mapRot = 0;
            // If true heading available then use HDT oterwise HDM
            if(valid3 == true){
                symbolRot = trueHeading;
            }
            else{
                symbolRot = magneticHeading;
            }
        }

        // Load navigation map
        //***********************************************************

        // URL to OBP Maps Converter
        // For more details see: https://github.com/norbert-walter/maps-converter
        String url = String("http://") + server + ":" + port +  // OBP Server
                    String("/get_image_json?") +                // Service: Output B&W picture as JSON (Base64 + gzip)
                    #ifdef DISPLAY_ST7796
                    "oformat=3" +       // Image output format in JSON: 3=RGB565 format
                    #else
                    "oformat=4" +       // Image output format in JSON: 4=b/w 1-Bit format
                    #endif                 
                    "&zoom=" + zoom +    // Default zoom level: 15
                    "&lat=" + String(latitude, 6) +   // Latitude
                    "&lon=" + String(longitude, 6) +  // Longitude
                    "&mrot=" + mapRot + // Rotation angle navigation map in degree
                    "&mtype=" + mType + // Default Map: Open Street Map
                    #ifdef DISPLAY_ST7796
                    "&itype=1" +        // Image type: 1=Color
                    #else
                    "&itype=4" +        // Image type: 4=b/w with dithering
                    #endif              
                    "&dtype=" + dType + // Dithering type: Atkinson dithering (only activ when itype=4 otherwise inactive)                   
                    "&width=400" +      // With navigation map
                    "&height=250" +     // Height navigation map
                    "&cutout=0" +       // No picture cutouts (tab, border and alpha are unused when cutout=0)
                    "&tab=0" +          // No tab size (only available when sqare cutouts selected coutout=3...7)
                    "&border=2" +       // Border line size: 2 pixel (only available when sqare cutouts selected)
                    "&alpha=80" +       // Alpha for tabs: 80% visible (only available when sqare cutouts selected)
                    "&symbol=2" +       // Symbol: Triangle
                    "&srot=" + symbolRot + // Symbol rotation angle
                    "&ssize=15" +       // Symbole size: 15 pixel (center pointer)
                    "&grid=" + mapGrid  // Show grid: On
                    ;         
        
        // Draw page
        //***********************************************************

        // ############### Draw Navigation Map ################
        
        // Set display in partial refresh mode
        displaySetPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // NEW: simple exponential backoff for 1 Hz polling (prevents connection-refused storms)
        static uint32_t nextAllowedMs = 0;
        static uint8_t  failCount = 0;

        uint32_t now = millis();

        // NEW: if we are in backoff window, skip network call and use backup immediately
        bool allowFetch = ((int32_t)(now - nextAllowedMs) >= 0);
        
        // If a network connection to URL then load the navigation map
        if (allowFetch && net.fetchAndDecompressJson(url)) {

            // NEW: reset backoff on success
            failCount = 0;
            nextAllowedMs = now + 1000; // keep 1 Hz on success

            auto& json = net.json();                // Extract JSON content
            int numPix = json["number_pixels"] | 0; // Read number of pixels
            imgWidth = json["width"] | 0;           // Read width of image
            imgHeight = json["height"] | 0;         // Read height og image
            size_t requiredBytesMono = 0;
            size_t requiredBytesRgb565 = 0;
            if (imgWidth > 0 && imgHeight > 0){
                requiredBytesMono = (size_t)((imgWidth + 7) / 8) * (size_t)imgHeight;
                requiredBytesRgb565 = (size_t)imgWidth * (size_t)imgHeight * 2U;
            }
            if (requiredBytesMono == 0){
                LOG_DEBUG(GwLog::ERROR,"Error PageNavigation: invalid image geometry w=%d h=%d",imgWidth,imgHeight);
                return PAGE_UPDATE;
            }

            const char* b64src = json["picture_base64"].as<const char*>();  // Read picture as Base64 content
            if (b64src == nullptr){
                LOG_DEBUG(GwLog::ERROR,"Error PageNavigation: picture_base64 missing");
                return PAGE_UPDATE;
            }
            size_t b64len = strlen(b64src);                                 // Calculate length of Base64 content
            // Copy Base64 content in PSRAM
            char* b64 = (char*) heap_caps_malloc(b64len + 1, MALLOC_CAP_SPIRAM);    // Allcate PSRAM for Base64 content
            if (!b64) {
                LOG_DEBUG(GwLog::ERROR,"Error PageNavigation: PSRAM alloc base64 failed");
                return PAGE_UPDATE;
            }
            memcpy(b64, b64src, b64len + 1);    // Copy Base64 content in PSRAM

            // Set image buffer in PSRAM
            //size_t imgSize = getdisplay().width() * getdisplay().height();
            size_t imgSize = (numPix > 0) ? (size_t)numPix : requiredBytesMono;    // Calculate image size
            if (imgSize < requiredBytesMono){
                imgSize = requiredBytesMono;
            }
            #ifdef DISPLAY_ST7796
            if (imgSize < requiredBytesRgb565){
                imgSize = requiredBytesRgb565;
            }
            #endif
            uint8_t* imageData = (uint8_t*) heap_caps_malloc(imgSize, MALLOC_CAP_SPIRAM);           // Allocate PSRAM for image
            if (!imageData) {
                LOG_DEBUG(GwLog::ERROR,"Error PageNavigation: PSRAM alloc image buffer failed");
                free(b64);
                return PAGE_UPDATE;
            }

            // Decode Base64 content to image
            size_t decodedSize = 0;
            bool decodeOk = decoder.decodeBase64(b64, b64len, imageData, imgSize, decodedSize);
            if (!decodeOk || decodedSize < requiredBytesMono){
                int base64Ret = mbedtls_base64_decode(
                    nullptr,
                    0,
                    &decodedSize,
                    (const unsigned char*)b64,
                    b64len
                );
                LOG_DEBUG(GwLog::ERROR,
                    "Error PageNavigation: decode failed (ok=%d, decoded=%u, required=%u, b64ret=%d)",
                    decodeOk ? 1 : 0,
                    (unsigned int)decodedSize,
                    (unsigned int)requiredBytesMono,
                    base64Ret
                );
                free(b64);
                free(imageData);
                return PAGE_UPDATE;
            }

            bool imageIsRgb565 = false;
            #ifdef DISPLAY_ST7796
            imageIsRgb565 = (decodedSize >= requiredBytesRgb565);
            #endif

            #ifdef DISPLAY_ST7796
            if (kShowRgb565StripeTestImage) {
                createRgb565StripeImage(reinterpret_cast<uint16_t*>(imageData), imgWidth, imgHeight);
                decodedSize = requiredBytesRgb565;
                imageIsRgb565 = true;
            }
            #endif

            // Copy actual navigation man to ackup map
            imageBackupWidth  = imgWidth;
            imageBackupHeight = imgHeight;
            imageBackupSize   = imgSize;
            if (decodedSize > 0 && imageBackupData != nullptr) {
                size_t copySize = (decodedSize > imageBackupCapacity) ? imageBackupCapacity : decodedSize;
                memcpy(imageBackupData, imageData, copySize);
                imageBackupSize = copySize;
            }
            imageBackupIsRgb565 = imageIsRgb565;
            hasImageBackup = (imageBackupData != nullptr);
            lostCounter = 0;

            // Show image (navigation map)
            #ifdef DISPLAY_ST7796
            if (imageIsRgb565) {
                drawRgb565Image(0, 25, reinterpret_cast<const uint16_t*>(imageData), imgWidth, imgHeight);
            } else {
                displayDrawBitmap(0, 25, imageData, imgWidth, imgHeight, commonData->fgcolor);
            }
            #else
            displayDrawBitmap(0, 25, imageData, imgWidth, imgHeight, commonData->fgcolor);
            #endif

            // Clean PSRAM
            free(b64);
            free(imageData);
        }
        // If no network connection then use backup navigation map
        else{

            // NEW: update backoff only if we actually attempted a fetch (not when skipping due to backoff)
            if (allowFetch) {
                // NEW: exponential backoff: 1s,2s,4s,8s,16s,30s (capped)
                if (failCount < 6) failCount++;
                uint32_t backoffMs = 1000u << failCount;
                if (backoffMs > 30000u) backoffMs = 30000u;
                nextAllowedMs = now + backoffMs;
            } else {
                // NEW: we are currently backing off; do not increase failCount further
                // nextAllowedMs stays unchanged
            }

            // Show backup image (backup navigation map)
            if (hasImageBackup) {
                #ifdef DISPLAY_ST7796
                if (imageBackupIsRgb565) {
                    drawRgb565Image(0, 25, reinterpret_cast<const uint16_t*>(imageBackupData), imageBackupWidth, imageBackupHeight);
                } else {
                    displayDrawBitmap(0, 25, imageBackupData, imageBackupWidth, imageBackupHeight, commonData->fgcolor);
                }
                #else
                displayDrawBitmap(0, 25, imageBackupData, imageBackupWidth, imageBackupHeight, commonData->fgcolor);
                #endif
            }    

            // Show connection lost info when 5 page refreshes has a connection lost to the map server
            // Short connection losts are uncritical
            if(lostCounter >= 5){
                getdisplay().setFont(&Ubuntu_Bold12pt8b);
                getdisplay().fillRect(200, 250 , 200, 25, commonData->fgcolor); // Black rect
                getdisplay().fillRect(202, 252 , 196, 21, commonData->bgcolor); // White rect
                getdisplay().setCursor(210, 270);
                getdisplay().print("Map server lost");
            }

            lostCounter++; // Increment lost counter
        }

        // ############### Draw Values ################
        getdisplay().setFont(&Ubuntu_Bold12pt8b);

        // Show zoom level
        getdisplay().fillRect(355, 25 , 45, 25, commonData->fgcolor);   // Black rect
        getdisplay().fillRect(357, 27 , 41, 21, commonData->bgcolor);   // White rect
        getdisplay().setCursor(364, 45);
        getdisplay().print(zoom);
        // If true heading available then use HDT oterwise HDM
        if(showValues == true){
            // Frame
            getdisplay().fillRect(0, 25 , 130, 65, commonData->fgcolor);   // Black rect
            getdisplay().fillRect(2, 27 , 126, 61, commonData->bgcolor);   // White rect
            if(valid3 == true){
                // HDT
                getdisplay().setCursor(10, 45);
                getdisplay().print(name3);
                getdisplay().setCursor(70, 45);
                getdisplay().print(svalue3);
            }
            else{
                // HDM
                getdisplay().setCursor(10, 45);
                getdisplay().print(name4);
                getdisplay().setCursor(70, 45);
                getdisplay().print(svalue4);
            }
            // SOG
            getdisplay().setCursor(10, 65);
            getdisplay().print(name5);
            getdisplay().setCursor(70, 65);
            getdisplay().print(svalue5);
            // DBT
            getdisplay().setCursor(10, 85);
            getdisplay().print(name6);
            getdisplay().setCursor(70, 85);
            getdisplay().print(svalue6);
        }
        
        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageNavigation(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageNavigation(
    "Navigation",       // Page name
    createPage,         // Action
    0,                  // Number of bus values depends on selection in Web configuration
    {"LAT","LON","HDT","HDM","SOG","DBT"},  // Bus values we need in the page
    true                // Show display header on/off
);

#endif
