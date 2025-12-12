#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "NetworkClient.h"      // Network connection
#include "ImageDecoder.h"       // Image decoder for navigation map

#include "Logo_OBP_400x300_sw.h"

// Defines for reading of navigation map 
#define JSON_BUFFER 30000       // Max buffer size for JSON content (30 kB picture + values)
NetworkClient net(JSON_BUFFER); // Define network client
ImageDecoder decoder;           // Define image decoder        

class PageNavigation : public Page
{
// Values for buttons
bool firstRun = true;    // Detect the first page run
int zoom = 15;           // Default zoom level
bool showValues = false; // Show values HDT, SOG, DBT in navigation map

public:
    PageNavigation(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageNavigation");
    }

    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        // Cood for zoom -
        if(key == 1){
            zoom --;                    // Zoom -
            if(zoom <7){
                zoom = 7;
            }
            return 0;                   // Commit the key
        }
        // Cood for zoom -
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
        // bool simulation = config->getBool(config->useSimuData);
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
                    String("/get_image_json?") +               // Service: Output B&W picture as JSON (Base64 + gzip)
                    "zoom=" + zoom +   // Default zoom level: 15
                    "&lat=" + String(latitude, 6) +   // Latitude
                    "&lon=" + String(longitude, 6) +  // Longitude
                    "&mrot=" + mapRot + // Rotation angle navigation map in degree
                    "&mtype=" + mType + // Default Map: Open Street Map
                    "&dtype=" + dType + // Dithering type: Atkinson dithering
                    "&width=400" +     // With navigation map
                    "&height=250" +    // Height navigation map
                    "&cutout=0" +      // No picture cutouts
                    "&tab=0" +         // No tab size
                    "&border=2" +      // Border line size: 2 pixel
                    "&symbol=2" +      // Symbol: Triangle
                    "&srot=" + symbolRot + // Symbol rotation angle
                    "&ssize=15" +      // Symbole size: 15 pixel
                    "&grid=" + mapGrid // Show grid: On
                    ;         
        
        // Draw page
        //***********************************************************

        // ############### Draw Navigation Map ################
        
        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // If a network connection to URL then load the navigation map
        if (net.fetchAndDecompressJson(url)) {

            auto& json = net.json();                // Extract JSON content
            int numPix = json["number_pixels"] | 0; // Read number of pixels
            int imgWidth = json["width"] | 0;       // Read width of image
            int imgHeight = json["height"] | 0;     // Read height og image

            const char* b64src = json["picture_base64"].as<const char*>();  // Read picture as Base64 content
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
            size_t imgSize = numPix;    // Calculate image size
            uint8_t* imageData = (uint8_t*) heap_caps_malloc(imgSize, MALLOC_CAP_SPIRAM);           // Allocate PSRAM for image
            if (!imageData) {
                LOG_DEBUG(GwLog::ERROR,"Error PageNavigation: PPSRAM alloc image buffer failed");
                free(b64);
                return PAGE_UPDATE;
            }

            // Decode Base64 content to image
            size_t decodedSize = 0;
            decoder.decodeBase64(b64, imageData, imgSize, decodedSize);

            // Show image (navigation map)
            getdisplay().drawBitmap(0, 25, imageData, imgWidth, imgHeight, commonData->fgcolor);

            // Clean PSRAM
            free(b64);
            free(imageData);
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

        // Set botton labels
        commonData->keydata[0].label = "ZOOM -";
        commonData->keydata[1].label = "ZOOM +";
        commonData->keydata[4].label = "VALUES";

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
