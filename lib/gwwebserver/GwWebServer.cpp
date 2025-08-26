#include <ESPmDNS.h>
#include "GwWebServer.h"
#include <map>

class EmbeddedFile;
static std::map<String,EmbeddedFile*> embeddedFiles;
class EmbeddedFile {
  public:
    const uint8_t *start;
    int len;
    String contentType;
    EmbeddedFile(String name,String contentType, const uint8_t *start,int len){
      this->start=start;
      this->len=len;
      this->contentType=contentType;
      embeddedFiles[name]=this;
    }
} ;
#define EMBED_GZ_FILE(fileName, binName, contentType) \
  extern const uint8_t  binName##_File[] asm("_binary_" #binName "_start"); \
  extern const uint8_t  binName##_FileLen[] asm("_binary_" #binName "_size"); \
  const EmbeddedFile binName##_Config(fileName,contentType,(const uint8_t*)binName##_File,(int)binName##_FileLen);

#include "GwEmbeddedFiles.h"

void sendEmbeddedFile(String name,String contentType,AsyncWebServerRequest *request){
    std::map<String,EmbeddedFile*>::iterator it=embeddedFiles.find(name);
    if (it != embeddedFiles.end()){
      EmbeddedFile* found=it->second;
      AsyncWebServerResponse *response=request->beginResponse(200, contentType, found->start, found->len);
      response->addHeader(F("Content-Encoding"), F("gzip"));
      request->send(response);
    }
    else{
      request->send(404, "text/plain", "Not found");
    }  
  }


GwWebServer::GwWebServer(GwLog* logger,GwRequestQueue *queue,int port){
    server=new AsyncWebServer(port);
    this->queue=queue;
    this->logger=logger;
}
void GwWebServer::begin(){
    server->onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      sendEmbeddedFile("index.html","text/html",request);
    });
    for (auto it=embeddedFiles.begin();it != embeddedFiles.end();it++){
      String uri=String("/")+it->first;
      server->on(uri.c_str(), HTTP_GET, [it](AsyncWebServerRequest *request){
      sendEmbeddedFile(it->first,it->second->contentType,request);
    });
    }
    
    server->begin();
    LOG_DEBUG(GwLog::LOG,"HTTP server started");
    MDNS.addService("_http","_tcp",80);

}
GwWebServer::~GwWebServer(){
    server->end();
    delete server;
}
void GwWebServer::handleAsyncWebRequest(AsyncWebServerRequest *request, GwRequestMessage *msg)
{
  GwRequestQueue::MessageSendStatus st=queue->sendAndWait(msg,msg->getTimeout());      
  if (st == GwRequestQueue::MSG_ERR)
  {
    msg->unref(); //our
    request->send(500, "text/plain", "queue full");
    return;
  }
  if (st == GwRequestQueue::MSG_OK)
  {
    request->send(200, msg->getContentType(), msg->getResult());
    msg->unref();
    return;
  }
  LOG_DEBUG(GwLog::DEBUG + 1, "switching to async for %s",msg->getName().c_str());
  //msg is handed over to async handling
  bool finished = false;
  AsyncWebServerResponse *r = request->beginChunkedResponse(
      msg->getContentType(), [this,msg, finished](uint8_t *ptr, size_t len, size_t len2) -> size_t
      {
        LOG_DEBUG(GwLog::DEBUG + 1, "try read for %s",msg->getName().c_str());
        if (msg->isHandled() || msg->wait(1))
        {
          int rt = msg->consume(ptr, len);
          LOG_DEBUG(GwLog::DEBUG + 1, "async response available, return %d\n", rt);
          return rt;
        }
        else
          return RESPONSE_TRY_AGAIN;
      },
      NULL);
  request->onDisconnect([this,msg](void)
                        {
                          LOG_DEBUG(GwLog::DEBUG + 1, "onDisconnect");
                          msg->unref();
                        });
  request->send(r);
}
bool GwWebServer::registerMainHandler(const char *url,RequestCreator creator){
    server->on(url,HTTP_GET, [this,creator,url](AsyncWebServerRequest *request){
        GwRequestMessage *msg=(*creator)(request);
        if (!msg){
            LOG_DEBUG(GwLog::DEBUG,"creator returns NULL for %s",url);
            request->send(404, "text/plain", "Not found");
            return;
        }
        handleAsyncWebRequest(request,msg);
    });
    return true;
}
bool GwWebServer::registerHandler(const char * url,GwWebServer::HandlerFunction handler){
  server->on(url,HTTP_GET,handler);
  return true;
}

bool GwWebServer::registerPostHandler(const char *url, ArRequestHandlerFunction requestHandler,
 ArBodyHandlerFunction bodyHandler){
    server->on(url,HTTP_POST,requestHandler,
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){},
    bodyHandler);
    return true;
}


