#pragma once
#include "GwSynchronized.h"
#include "GwApi.h"
#include "freertos/semphr.h"
#include "Pagedata.h"

class SharedData{
    private:
        SemaphoreHandle_t locker;
        SensorData sensors;
        tBoatHstryData boatHstry;
    public:
        GwApi *api=NULL;
        SharedData(GwApi *api){
            locker=xSemaphoreCreateMutex();
            this->api=api;
        }
        void setSensorData(SensorData &values){
            GWSYNCHRONIZED(&locker);
            sensors=values;
        }
        SensorData getSensorData(){
            GWSYNCHRONIZED(&locker);
            return sensors;
        }
        void setHstryBuf(RingBuffer<int16_t> twdHstry,RingBuffer<int16_t> twsHstry,RingBuffer<int16_t> dbtHstry) {
            GWSYNCHRONIZED(&locker);
            boatHstry={&twdHstry, &twsHstry, &dbtHstry};
//            api->getLogger()->logDebug(GwLog::ERROR, "SharedData setHstryBuf: TWD: %p, TWS: %p, STW: %p", boatHstry.twdHstry, boatHstry.twsHstry, boatHstry.dbtHstry);
        }
        tBoatHstryData getHstryBuf() {
            GWSYNCHRONIZED(&locker);
            return boatHstry;
        }
};

void createSensorTask(SharedData *shared);


