#ifndef _GWWIFI_H
#define _GWWIFI_H
#include <WiFi.h>
#include <GWConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class GwWifi{
    private:
        const GwConfigHandler *config;
        GwLog *logger;
        const GwConfigInterface *wifiClient;
        const GwConfigInterface *wifiSSID;
        const GwConfigInterface *wifiPass;
        bool connectInternal();
        long lastConnectStart=0;
        unsigned long lastApAccess=0;
        unsigned long apShutdownTime=0;
        bool apActive=false;
        bool fixedApPass=true;
        bool clientIsConnected=false;
        SemaphoreHandle_t wifiMutex=nullptr;
        static const TickType_t WIFI_MUTEX_TIMEOUT=pdMS_TO_TICKS(1000);
        bool acquireMutex();
        void releaseMutex();
    public:
        const char *AP_password = "esp32nmea2k"; 
        GwWifi(const GwConfigHandler *config,GwLog *log, bool fixedApPass=true);
        ~GwWifi();
        void setup();
        void loop();
        bool clientConnected();
        bool connectClient();          // Blocking version
        bool connectClientAsync();     // Non-blocking version for other tasks
        String apIP();
        bool isApActive(){return apActive;}
        bool isClientActive(){return wifiClient->asBoolean();}}
};
#endif