#include "GwUpdate.h"
#include <Update.h>

static String jsonError(String e){
    e.replace('"',' ');
    return "{\"status\":\""+e+"\"}";
}
class UpdateParam{
    public:
        String error;
        size_t uplodaded=0;
        bool otherUpdate=false;
        bool hasError(){
            return ! error.isEmpty();
        }

};
bool GwUpdate::delayedRestart(){
  return xTaskCreate([](void *p){
    GwLog *logRef=(GwLog *)p;
    logRef->logDebug(GwLog::LOG,"delayed reset started");
    delay(800);
    ESP.restart();
    vTaskDelete(NULL);
  },"reset",2000,logger,0,NULL) == pdPASS;
}
GwUpdate::GwUpdate(GwLog *log, GwWebServer *webserver, PasswordChecker ckr)
{
    this->checker=ckr;
    this->logger = log;
    this->server = webserver->getServer();
    server->on("/api/update", HTTP_POST, [&](AsyncWebServerRequest *request) {
                // the request handler is triggered after the upload has finished... 
                // create the response, add header, and send response
                String result="{\"status\":\"OK\"}";
                bool updateOk=true;
                if ( request->_tempObject == NULL){
                    //no data
                    LOG_DEBUG(GwLog::ERROR,"no data in update request");
                    result=jsonError("no data uploaded");
                    updateOk=false;
                }
                else{
                    UpdateParam *param=(UpdateParam *)(request->_tempObject);
                    updateOk=!param->hasError();
                    LOG_DEBUG(GwLog::LOG,"update finished status=%s %s (%d)",
                        (updateOk?"ok":"error"),
                        param->error.c_str(),
                        param->uplodaded
                        );
                    if (!updateOk){
                        result=jsonError(String("Update failed: ")+param->error);
                    }
                    if (! param->otherUpdate) {
                        updateRunning=0;
                    }
                }
                AsyncWebServerResponse *response = request->beginResponse(200, "application/json",result);
                response->addHeader("Connection", "close");
                request->send(response);
                if (updateOk) delayedRestart();
            }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                //Upload handler chunks in data
                LOG_DEBUG(GwLog::DEBUG,"update chunk f=%s,idx=%d,len=%d,final=%d",
                    filename.c_str(),(int)index,(int)len,(int)final
                );
                UpdateParam *param=(UpdateParam *)request->_tempObject;
                if (!index) {
                    if (param == NULL) {
                        param=new UpdateParam();
                        request->_tempObject=param;
                    }
                    if (updateRunning > 0 && (updateRunning + 60000) >= millis()){
                        param->error="another update is running";
                        param->otherUpdate=true;
                    }
                    else{
                        updateRunning=millis();
                    }
                    if (!param->hasError())
                    {
                        const AsyncWebParameter *hash=request->getParam("_hash");
                        if (! hash){
                            hash=request->getParam("_hash",true);
                        }
                        if (!hash)
                        {
                            LOG_DEBUG(GwLog::ERROR, "missing _hash in update");
                            param->error = "missing _hash";
                        }
                        else
                        {
                            String shash=hash->value();
                            LOG_DEBUG(GwLog::DEBUG,"checking hash %s",shash.c_str());
                            if (!checker(shash))
                            {
                                LOG_DEBUG(GwLog::ERROR, "invalid _hash in update %s",shash.c_str());
                                param->error = "invalid password";
                            }
                        }
                    }
                    if (! param->hasError()){
                        Update.abort(); //abort any dangling update
                        int cmd=U_FLASH;
                        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) { // Start with max available size
                            LOG_DEBUG(GwLog::ERROR,"unable to start update %s",Update.errorString());
                            param->error=Update.errorString();
                        }
                    }
                }
                if (param && !param->hasError())
                {
                    // Write chunked data to the free sketch space
                    if (len)
                    {
                        size_t wr = Update.write(data, len);
                        if (wr != len)
                        {
                            LOG_DEBUG(GwLog::ERROR, "invalid write, expected %d got %d", (int)len, (int)wr);
                            param->error="unable to write";
                            Update.abort();
                        }
                        else{
                            param->uplodaded+=wr;
                        }
                    }

                    if (final && ! param->hasError())
                    { // if the final flag is set then this is the last frame of data
                        LOG_DEBUG(GwLog::DEBUG,"finalizing update");
                        if (!Update.end(true))
                        { //true to set the size to the current progress
                            LOG_DEBUG(GwLog::ERROR, "unable to end update %s", Update.errorString());
                            param->error=String("unable to end update:") + String(Update.errorString());
                            Update.abort();
                        }
                    }
                    else
                    {
                        return;
                    }
                }
            });
}
