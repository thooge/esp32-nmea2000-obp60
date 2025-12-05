#include "NetworkClient.h"

extern "C" {
    #include "puff.h"
}

// Constructor
NetworkClient::NetworkClient(size_t reserveSize)
    : _doc(reserveSize),
      _valid(false)
{
}

// Skip GZIP Header an goto DEFLATE content
int NetworkClient::skipGzipHeader(const uint8_t* data, size_t len) {
    if (len < 10) return -1;

    if (data[0] != 0x1F || data[1] != 0x8B || data[2] != 8) {
        return -1;
    }

    size_t pos = 10;
    uint8_t flags = data[3];

    if (flags & 4) {
        if (pos + 2 > len) return -1;
        uint16_t xlen = data[pos] | (data[pos+1] << 8);
        pos += 2 + xlen;
    }

    if (flags & 8) {
        while (pos < len && data[pos] != 0) pos++;
        pos++;
    }

    if (flags & 16) {
        while (pos < len && data[pos] != 0) pos++;
        pos++;
    }

    if (flags & 2) pos += 2;

    if (pos >= len) return -1;

    return pos;
}

// HTTP GET + GZIP Decompression (reading in chunks)
bool NetworkClient::httpGetGzip(const String& url, uint8_t*& outData, size_t& outLen) {

    const size_t capacity = READLIMIT;   // Read limit for data (can be adjusted in NetworkClient.h)
    uint8_t* buffer = (uint8_t*)malloc(capacity);

    if (!buffer) {
        if (DEBUG) {Serial.println("Malloc failed (buffer");}
        return false;
    }

    HTTPClient http;

    // Timeouts to prevent hanging connections
    http.setConnectTimeout(CONNECTIONTIMEOUT);  // Connect timeout in ms (can be adjusted in NetworkClient.h)
    http.setTimeout(TCPREADTIMEOUT);            // Read timeout in ms (can be adjusted in NetworkClient.h)

    http.begin(url);
    http.addHeader("Accept-Encoding", "gzip");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP ERROR: %d\n", code);

        // Hard reset HTTP + socket
        WiFiClient* tmp = http.getStreamPtr();
        if (tmp) tmp->stop();   // Force close TCP socket
        http.end();

        free(buffer);
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();

    size_t len = 0;
    uint32_t lastData = millis();
    const uint32_t READ_TIMEOUT = READDATATIMEOUT;   // Timeout for reading data (can be adjusted in NetworkClient.h)

    bool complete = false;

    while (http.connected() && !complete) {

        size_t avail = stream->available();

        if (avail == 0) {
            if (millis() - lastData > READ_TIMEOUT) {
                Serial.println("TIMEOUT waiting for data!");
                break;
            }
            delay(1);
            continue;
        }

        if (len + avail > capacity)
            avail = capacity - len;

        int read = stream->readBytes(buffer + len, avail);
        len += read;
        lastData = millis();

        if (DEBUG) {Serial.printf("Read chunk: %d (total: %d)\n", read, (int)len);}

        if (len < 20) continue;  // Not enough data for header

        int headerOffset = skipGzipHeader(buffer, len);
        if (headerOffset < 0) continue;

        unsigned long testLen = len * 8; // Dynamic expansion
        uint8_t* test = (uint8_t*)malloc(testLen);

        if (!test) continue;

        unsigned long srcLen = len - headerOffset;

        int res = puff(test, &testLen, buffer + headerOffset, &srcLen);
        if (res == 0) {
            if (DEBUG) {Serial.printf("Decompress OK! Size: %lu bytes\n", testLen);}
            outData = test;
            outLen = testLen;
            complete = true;
            break;
        }

        free(test);
    }

    // --- Added: Force-close connection in all cases to avoid stuck TCP sockets ---
    if (stream) stream->stop();

    http.end();
    free(buffer);

    if (!complete) {
        Serial.println("Failed to complete decompress.");
        return false;
    }

    return true;
}

// Decompress JSON
bool NetworkClient::fetchAndDecompressJson(const String& url) {

    _valid = false;

    uint8_t* raw = nullptr;
    size_t rawLen = 0;

    if (!httpGetGzip(url, raw, rawLen)) {
        Serial.println("GZIP download/decompress failed.");
        return false;
    }

    DeserializationError err = deserializeJson(_doc, raw, rawLen);
    free(raw);

    if (err) {
        Serial.printf("JSON ERROR: %s\n", err.c_str());
        return false;
    }

    if (DEBUG) {Serial.println("JSON OK!");}
    _valid = true;
    return true;
}

JsonDocument& NetworkClient::json() {
    return _doc;
}

bool NetworkClient::isValid() const {
    return _valid;
}
