#pragma once
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define DEBUG false         // Debug flag for NetworkClient for more live information
#define READLIMIT 200000    // HTTP read limit in byte for gzip content (can be adjusted)
#define NETWORKTIMEOUT 8000 // 8s Network timeout

class NetworkClient {
public:
    NetworkClient(size_t reserveSize = 0);

    bool fetchAndDecompressJson(const String& url);
    JsonDocument& json();
    bool isValid() const;

private:
    DynamicJsonDocument _doc;
    bool _valid;

    int skipGzipHeader(const uint8_t* data, size_t len);
    bool httpGetGzip(const String& url, uint8_t*& outData, size_t& outLen);
};

