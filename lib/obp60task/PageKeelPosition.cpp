#ifdef BOARD_OBP60S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageKeelPosition : public Page
{
public:
    PageKeelPosition(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageKeelPosition");
    }

    // Key functions
    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;               // Commit the key
        }
        return key;
    }

    virtual void displayPage(PageData &pageData)
    {
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

        double value1 = 0;
        double value1old = 0;

        // Get config data
        String lengthformat = config->getString(config->lengthFormat);
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);
        String rotsensor = config->getString(config->useRotSensor);
        String rotfunction = config->getString(config->rotFunction);

        // Get boat values for Keel position
        bool valid1 = commonData->data.validRotAngle;    // Valid information 
        if(simulation == false && rotsensor == "AS5600" && rotfunction == "Keel"){
            value1 = commonData->data.rotationAngle; // Raw value without unit convertion
        }
        else{
            value1 = 0;
        }
        if(simulation == true){
            value1 = (170 + float(random(0, 40)) / 10.0) * 2 * PI / 360; // Simulation data in radiant
        }

        String unit1 = "Deg";                           // Unit of value
        if(valid1 == true){
            value1old = value1;   	                    // Save old value
        }

        // Optical warning by limit violation (unused)
        if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        }

        // Logging boat values
        LOG_DEBUG(GwLog::LOG,"Drawing at PageKeelPosition, Keel:%f", value1);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

//*******************************************************************************************
        
        // Draw KeelPosition
        int rInstrument = 110;     // Radius of KeelPosition
        float pi = 3.141592;

        getdisplay().fillCircle(200, 150, rInstrument + 10, commonData->fgcolor);       // Outer circle
        getdisplay().fillCircle(200, 150, rInstrument + 7, commonData->bgcolor);        // Outer circle
        getdisplay().fillRect(0, 30, 400, 122, commonData->bgcolor);                    // Delete half top circle

        for(int i=90; i<=270; i=i+10)
        {
            // Scaling values
            float x = 200 + (rInstrument-30)*sin(i/180.0*pi);  //  x-coordinate dots
            float y = 150 - (rInstrument-30)*cos(i/180.0*pi);  //  y-coordinate cots 
            const char *ii = " ";
            switch (i)
            {
            case 0: ii=" "; break;      // Use a blank for a empty scale value
            case 30 : ii=" "; break;
            case 60 : ii=" "; break;
            case 90 : ii="45"; break;
            case 120 : ii="30"; break;
            case 150 : ii="15"; break;
            case 180 : ii="0"; break;
            case 210 : ii="15"; break;
            case 240 : ii="30"; break;
            case 270 : ii="45"; break;
            case 300 : ii=" "; break;
            case 330 : ii=" "; break;
            default: break;
            }

            // Print text centered on position x, y
            int16_t x1, y1;     // Return values of getTextBounds
            uint16_t w, h;      // Return values of getTextBounds
            getdisplay().getTextBounds(ii, int(x), int(y), &x1, &y1, &w, &h); // Calc width of new string
            getdisplay().setCursor(x-w/2, y+h/2);
            if(i % 30 == 0){
                getdisplay().setFont(&Ubuntu_Bold8pt7b);
                getdisplay().print(ii);
            }

            // Draw sub scale with dots
            float x1c = 200 + rInstrument*sin(i/180.0*pi);
            float y1c = 150 - rInstrument*cos(i/180.0*pi);
            getdisplay().fillCircle((int)x1c, (int)y1c, 2, commonData->fgcolor);
            float sinx=sin(i/180.0*pi);
            float cosx=cos(i/180.0*pi); 

            // Draw sub scale with lines (two triangles)
            if(i % 30 == 0){
                float dx=2;   // Line thickness = 2*dx+1
                float xx1 = -dx;
                float xx2 = +dx;
                float yy1 =  -(rInstrument-10);
                float yy2 =  -(rInstrument+10);
                getdisplay().fillTriangle(200+(int)(cosx*xx1-sinx*yy1),150+(int)(sinx*xx1+cosx*yy1),
                        200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                        200+(int)(cosx*xx1-sinx*yy2),150+(int)(sinx*xx1+cosx*yy2),commonData->fgcolor);
                getdisplay().fillTriangle(200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                        200+(int)(cosx*xx1-sinx*yy2),150+(int)(sinx*xx1+cosx*yy2),
                        200+(int)(cosx*xx2-sinx*yy2),150+(int)(sinx*xx2+cosx*yy2),commonData->fgcolor);
            }

        }

        // Angle limits to +/-45° (Attention: 180° offset!)
        if(value1 < (3 * PI / 4)){
            value1 = 3 * PI / 4;
        }
        if(value1 > (5 * PI / 4)){
            value1 = 5 * PI / 4;
        }
        
        if(holdvalues == true && valid1 == false){
            value1 = value1old;
        }

        // Calculate keel position
        value1 = (value1 * 2) + PI;

        // Draw keel position pointer
        float startwidth = 8;       // Start width of pointer

        if((rotsensor == "AS5600" && rotfunction == "Keel" && (valid1 == true || holdvalues == true)) || simulation == true){
            float sinx=sin(value1);
            float cosx=cos(value1);
            // Normal pointer
            // Pointer as triangle with center base 2*width
            float xx1 = -startwidth;
            float xx2 = startwidth;
            float yy1 = -startwidth;
            float yy2 = -(rInstrument * 0.6); 
            getdisplay().fillTriangle(200+(int)(cosx*xx1-sinx*yy1),150+(int)(sinx*xx1+cosx*yy1),
                200+(int)(cosx*xx2-sinx*yy1),150+(int)(sinx*xx2+cosx*yy1),
                200+(int)(cosx*0-sinx*yy2),150+(int)(sinx*0+cosx*yy2),commonData->fgcolor);
            // Inverted pointer
            // Pointer as triangle with center base 2*width
            float endwidth = 2;         // End width of pointer
            float ix1 = endwidth;
            float ix2 = -endwidth;
            float iy1 = -(rInstrument * 0.6);
            float iy2 = -endwidth;
            getdisplay().fillTriangle(200+(int)(cosx*ix1-sinx*iy1),150+(int)(sinx*ix1+cosx*iy1),
                200+(int)(cosx*ix2-sinx*iy1),150+(int)(sinx*ix2+cosx*iy1),
                200+(int)(cosx*0-sinx*iy2),150+(int)(sinx*0+cosx*iy2),commonData->fgcolor);

            // Draw counterweight
            getdisplay().fillCircle(200+(int)(cosx*0-sinx*yy2),150+(int)(sinx*0+cosx*yy2), 5, commonData->fgcolor);
        }

        // Center circle
        getdisplay().fillCircle(200, 140, startwidth + 22, commonData->bgcolor);
        getdisplay().fillCircle(200, 140, startwidth + 20, commonData->fgcolor);      // Boat circle
        getdisplay().fillRect(200 - 30, 140 - 30, 2 * 30, 30, commonData->bgcolor);   // Delete half top of boat circle
        getdisplay().fillRect(150, 150, 100, 4, commonData->fgcolor);                 // Water line

        // Print label
        getdisplay().setFont(&Ubuntu_Bold16pt7b);
        getdisplay().setCursor(100, 70);
        getdisplay().print("Keel Position");                 // Label

        if((rotsensor == "AS5600" && rotfunction == "Keel" && (valid1 == true || holdvalues == true)) || simulation == true){
            // Print Unit of keel position
            getdisplay().setFont(&Ubuntu_Bold12pt7b);
            getdisplay().setCursor(175, 110);
            getdisplay().print(unit1);                       // Unit
        }
        else{
            // Print Unit of keel position
            getdisplay().setFont(&Ubuntu_Bold8pt7b);
            getdisplay().setCursor(145, 110);
            getdisplay().print("No sensor data");            // Info missing sensor
        }

        // Update display
        getdisplay().nextPage();    // Partial update (fast)

    };
};

static Page *createPage(CommonData &common){
    return new PageKeelPosition(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageKeelPosition(
    "KeelPosition",   // Page name
    createPage,       // Action
    0,                // Number of bus values depends on selection in Web configuration
    {},               // Bus values we need in the page
    true              // Show display header on/off
);

#endif
