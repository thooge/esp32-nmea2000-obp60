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
        return key;
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        // Old values for hold function
        static String svalue1old = "";
        static String unit1old = "";
        static String svalue2old = "";
        static String unit2old = "";
        static String svalue3old = "";
        static String unit3old = "";
        static String svalue4old = "";
        static String unit4old = "";

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        // bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
        
        // Get boat values #1
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list (only one value by PageOneValue)
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        name1 = name1.substring(0, 6);                  // String length limit for value name
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Get boat values #2
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list (only one value by PageOneValue)
        String name2 = xdrDelete(bvalue2->getName());   // Value name
        name2 = name2.substring(0, 6);                  // String length limit for value name
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = formatValue(bvalue2, *commonData).unit;        // Unit of value

        // Get boat values #3
        GwApi::BoatValue *bvalue3 = pageData.values[2]; // Second element in list (only one value by PageOneValue)
        String name3 = xdrDelete(bvalue3->getName());   // Value name
        name3 = name3.substring(0, 6);                  // String length limit for value name
        double value3 = bvalue3->value;                 // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information 
        String svalue3 = formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = formatValue(bvalue3, *commonData).unit;        // Unit of value

        // Get boat values #4
        GwApi::BoatValue *bvalue4 = pageData.values[3]; // Second element in list (only one value by PageOneValue)
        String name4 = xdrDelete(bvalue4->getName());   // Value name
        name4 = name4.substring(0, 6);                  // String length limit for value name
        double value4 = bvalue4->value;                 // Value as double in SI unit
        bool valid4 = bvalue4->valid;                   // Valid information 
        String svalue4 = formatValue(bvalue4, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit4 = formatValue(bvalue4, *commonData).unit;        // Unit of value

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        if (bvalue1 == NULL) return PAGE_OK; // WTF why this statement?
        LOG_DEBUG(GwLog::LOG,"Drawing at PageNavigation, %s: %f, %s: %f, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3, name4.c_str(), value4);

        // Load navigation map
        //***********************************************************
        // Rotate the picture in 1° steps
        int angle = 25;

        // Server settings 
        String server = "norbert-walter.dnshome.de";
        int port = 80;

        // URL to OBP Maps Converter
        // For more details see: https://github.com/norbert-walter/maps-converter
        String url = String("http://") + server + ":" + port +  // OBP Server
                    String("/get_image_json?") +               // Service: Output B&W picture as JSON (Base64 + gzip)
                    "zoom=15" +        // Zoom level: 15
                    "&lat=53.9028" +   // Latitude
                    "&lon=11.4441" +   // Longitude
                    "&mrot=" + angle + // Rotation angle navigation map
                    "&mtype=9" +       // Free Nautical Charts with depth
                    "&dtype=1" +       // Dithering type: Threshold dithering
                    "&width=400" +     // With navigation map
                    "&height=250" +    // Height navigation map
                    "&cutout=0" +      // No picture cutouts
                    "&tab=0" +         // No tab size
                    "&border=2" +      // Border line size: 2 pixel
                    "&symbol=2" +      // Symbol: Triangle
                    "&srot=" + angle + // Symbol rotation angle
                    "&ssize=15" +      // Symbole size: 15 pixel
                    "&grid=1"          // Show grid: On
                    ;         
        
        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // If a network connection to URL
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
                Serial.println("ERROR: PSRAM alloc base64 failed");
                return PAGE_UPDATE;
            }
            memcpy(b64, b64src, b64len + 1);    // Copy Base64 content in PSRAM

            // Set image buffer in PSRAM
            //size_t imgSize = getdisplay().width() * getdisplay().height();                  // Calculate image size
            size_t imgSize = numPix;    // Calculate image size
            uint8_t* imageData = (uint8_t*) heap_caps_malloc(imgSize, MALLOC_CAP_SPIRAM);   // Allocate PSRAM for image
            if (!imageData) {
                Serial.println("ERROR: PSRAM alloc image buffer failed");
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
  
        
        // Draw page
        //***********************************************************


        // ############### Draw Navigation Map ################
        getdisplay().setFont(&Ubuntu_Bold12pt8b);

        getdisplay().setCursor(20, 60);
        getdisplay().print(name1);
        getdisplay().setCursor(80, 60);
        getdisplay().print(svalue1);

        getdisplay().setCursor(20, 80);
        getdisplay().print(name2);
        getdisplay().setCursor(80, 80);
        getdisplay().print(svalue2);

        getdisplay().setCursor(20, 100);
        getdisplay().print(name3);
        getdisplay().setCursor(80, 100);
        getdisplay().print(svalue3);

        getdisplay().setCursor(20, 120);
        getdisplay().print(name4);
        getdisplay().setCursor(80, 120);
        getdisplay().print(svalue4);

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
    {"LAT","LON","HDT","SOG"},  // Bus values we need in the page
    true                // Show display header on/off
);

#endif
