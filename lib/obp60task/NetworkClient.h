#pragma once
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define DEBUGING true             // Debug flag for NetworkClient for more live information
#define READLIMIT 200000        // HTTP read limit in byte for gzip content (can be adjusted)
#define CONNECTIONTIMEOUT 3000  // Timeout in ms for HTTP connection 
#define TCPREADTIMEOUT 2000     // Timeout in ms for read HTTP client stack
#define READDATATIMEOUT 2000    // Timeout in ms for read data

class NetworkClient {
public:
    NetworkClient(size_t reserveSize = 0);
    ~NetworkClient();

    bool fetchAndDecompressJson(const String& url);
    JsonDocument& json();
    int imageWidth() const;
    int imageHeight() const;
    int numberPixels() const;
    const char* pictureBase64() const;
    size_t pictureBase64Len() const;
    bool isValid() const;

private:
    DynamicJsonDocument _doc;
    bool _valid;
    uint8_t* _jsonRaw;
    size_t _jsonRawLen;
    int _imageWidth;
    int _imageHeight;
    int _numberPixels;
    char* _pictureBase64;
    size_t _pictureBase64Len;

    int skipGzipHeader(const uint8_t* data, size_t len);
    bool httpGetGzip(const String& url, uint8_t*& outData, size_t& outLen);
    static bool findJsonIntField(const char* json, size_t len, const char* key, int& outValue);
    static bool extractJsonStringInPlace(char* json, size_t len, const char* key, char*& outValue, size_t& outLen);
};

