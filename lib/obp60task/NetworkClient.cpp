#include "NetworkClient.h"
#include "GWWifi.h"             // WiFi management (thread-safe)

extern GwWifi gwWifi;   // Extern declaration of global WiFi instance

extern "C" {
    #include "puff.h"
}

// Constructor
NetworkClient::NetworkClient(size_t reserveSize)
    : _doc(reserveSize),
      _valid(false),
      _jsonRaw(nullptr),
      _jsonRawLen(0)
{
}

NetworkClient::~NetworkClient() {
    if (_jsonRaw != nullptr) {
        free(_jsonRaw);
        _jsonRaw = nullptr;
        _jsonRawLen = 0;
    }
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

    const size_t capacity = READLIMIT;          // Read limit for data (can be adjusted in NetworkClient.h)
    uint8_t* buffer = (uint8_t*)malloc(capacity);

    // If not with WiFi connectetd then return without any activities
    if (!gwWifi.clientConnected()) {
        if (DEBUGING) {Serial.println("No WiFi connection");}
        return false;
    }
    
    // If frame buffer not correct allocated then return without any activities
    if (!buffer) {
        if (DEBUGING) {Serial.println("Malloc failed buffer");}
        return false;
    }

    HTTPClient http;

    // Timeouts to prevent hanging connections
    http.setConnectTimeout(CONNECTIONTIMEOUT);  // Connect timeout in ms (can be adjusted in NetworkClient.h)
    http.setTimeout(TCPREADTIMEOUT);            // Read timeout in ms (can be adjusted in NetworkClient.h)

    http.begin(url);

    // NEW: force server to close the connection after the response (prevents "stuck" keep-alive reads)
    http.addHeader("Connection", "close");

    // NEW: request gzip, but we will only decompress if the server actually answers with gzip
    http.addHeader("Accept-Encoding", "gzip");

    // NEW: register headers BEFORE GET() (more reliable with Arduino HTTPClient)
    if (DEBUGING) {
        // We need follow key words
        const char* keys[] = {
            "Content-Encoding",
            "Transfer-Encoding",
            "Content-Length"
        };
        // Read header
        http.collectHeaders(keys, 3);
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP Client ERROR: %d (%s)\n", code, http.errorToString(code).c_str());

        // Hard reset HTTP + socket
        WiFiClient* tmp = http.getStreamPtr();
        if (tmp) tmp->stop();   // Force close TCP socket
        
        http.end();
        free(buffer);
        return false;
    }
    else{       
        if (DEBUGING) {
            String ce = http.header("Content-Encoding");
            String te = http.header("Transfer-Encoding");
            String cl = http.header("Content-Length");

            // Print header informations
            Serial.printf("Content-Encoding=%s Transfer-Encoding=%s Content-Length=%s\n",
                        ce.c_str(),
                        te.c_str(),
                        cl.c_str());
        }                
    }

    WiFiClient* stream = http.getStreamPtr();

    size_t len = 0;
    uint32_t lastData = millis();
    const uint32_t READ_TIMEOUT = READDATATIMEOUT;   // Timeout for reading data (can be adjusted in NetworkClient.h)

    bool complete = false;
    bool aborting = false;   // NEW: remember if we must force-close socket

    // NEW: detect if server really sent gzip
    String ce = http.header("Content-Encoding");
    bool isGzip = ce.equalsIgnoreCase("gzip");

    // NEW: read expected body size if provided by server (prevents waiting forever for missing bytes)
    int total = http.getSize();  // returns Content-Length, or -1 if unknown/chunked

    // NEW: fail fast if server claims something larger than our buffer
    if (total > 0 && (size_t)total > capacity) {
        Serial.println("Response exceeds READLIMIT.");
        aborting = true;
    }

    // NEW: if not gzip, we will not try to decompress (prevents false "Decompress OK" / random success)
    // You can either handle plain JSON here or just fail-fast.
    if (!isGzip && !aborting) {
        if (DEBUGING) {
            Serial.println("Server response is NOT gzip (Content-Encoding != gzip).");
            Serial.println("Either disable Accept-Encoding: gzip or add plain-body handling here.");
        }

        // --- Plain-body handling (recommended): read full body into outData as-is ---
        // NEW: try to read Content-Length bytes if available (more robust)
        if (total > 0 && (size_t)total > capacity) {
            Serial.println("Plain response exceeds READLIMIT.");
            aborting = true;
        } else {
            // Read until we have all bytes (Content-Length) or until connection closes + buffer drains
            while ((http.connected() || (stream && stream->available())) && !aborting) {
                size_t avail = stream ? stream->available() : 0;
                if (avail == 0) {
                    if (millis() - lastData > READ_TIMEOUT) {
                        Serial.println("TIMEOUT waiting for data (plain)!");
                        aborting = true;
                        break;
                    }
                    delay(1);
                    continue;
                }

                if (len >= capacity) {
                    Serial.println("READLIMIT reached, aborting (plain).");
                    aborting = true;
                    break;
                }

                if (len + avail > capacity)
                    avail = capacity - len;

                int read = stream->readBytes(buffer + len, avail);
                if (read > 0) {
                    len += (size_t)read;
                    lastData = millis();
                }

                // NEW: stop reading as soon as we have the full response
                if (total > 0 && (int)len >= total) {
                    break; // we got full body
                }
            }
        }

        if (aborting) {
            // --- Added: Force-close connection only if aborted to avoid TCP RST storms ---
            if (stream) stream->stop();   // Force close TCP socket
            http.end();
            free(buffer);
            return false;
        }

        // Return plain body to caller
        outData = (uint8_t*)malloc(len);
        if (!outData) {
            Serial.println("Malloc failed outData (plain).");
            // --- Added: Force-close connection only if aborted to avoid TCP RST storms ---
            if (stream) stream->stop();   // Force close TCP socket
            http.end();
            free(buffer);
            return false;
        }
        memcpy(outData, buffer, len);
        outLen = len;

        http.end();
        free(buffer);
        return true;
    }

    // --- GZIP path (only if Content-Encoding is gzip) ---
    if (!aborting) {

        // NEW: read exactly Content-Length bytes when available (prevents partial-body timeout loops)
        while ((http.connected() || (stream && stream->available())) && !complete && !aborting) {

            size_t avail = stream ? stream->available() : 0;

            if (avail == 0) {
                // NEW: if Content-Length is known and we already read it all, stop immediately
                if (total > 0 && (int)len >= total) {
                    break;
                }

                if (millis() - lastData > READ_TIMEOUT) {
                    Serial.println("TIMEOUT waiting for data!");
                    aborting = true;   // NEW: mark abnormal exit
                    break;
                }
                delay(1);
                continue;
            }

            // NEW: safety check if buffer limit is reached
            if (len >= capacity) {
                Serial.println("READLIMIT reached, aborting.");
                aborting = true;
                break;
            }

            // NEW: if Content-Length is known, do not read beyond it
            if (total > 0) {
                size_t remaining = (size_t)total - len;
                if (avail > remaining) avail = remaining;
            }

            if (len + avail > capacity)
                avail = capacity - len;

            int read = stream->readBytes(buffer + len, avail);
            if (read <= 0) {
                // NEW: avoid tight loop if read returns zero
                delay(1);
                continue;
            }

            len += (size_t)read;
            lastData = millis();

            if (DEBUGING) {Serial.printf("Read chunk: %d (total: %d)\n", read, (int)len);}

            // NEW: if Content-Length is known and fully received, we can stop reading
            if (total > 0 && (int)len >= total) {
                break;
            }
        }

        // NEW: only attempt gzip parse/decompress after we have a complete body (when Content-Length is known)
        // This avoids wasting heap with repeated malloc/free and reduces fragmentation over long runtimes.
        if (!aborting) {
            if (len < 20) {
                aborting = true;
            } else {
                int headerOffset = skipGzipHeader(buffer, len);
                if (headerOffset < 0) {
                    aborting = true;
                } else {
                    unsigned long testLen = len * 8; // Dynamic expansion
                    uint8_t* test = (uint8_t*)malloc(testLen);

                    if (!test) {
                        Serial.println("Malloc failed test buffer, aborting.");
                        aborting = true;
                    } else {
                        unsigned long srcLen = (unsigned long)(len - (size_t)headerOffset);
                        int res = puff(test, &testLen, buffer + headerOffset, &srcLen);

                        if (res == 0) {
                            if (DEBUGING) {Serial.printf("Decompress OK! Size: %lu bytes\n", testLen);}
                            outData = test;
                            outLen = (size_t)testLen;
                            complete = true;
                        } else {
                            free(test);
                        }
                    }
                }
            }
        }
    }

    // --- Added: Force-close connection only if aborted to avoid TCP RST storms ---
    if (aborting && stream) stream->stop();   // NEW: stop() only on abnormal termination

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
    _doc.clear();

    if (_jsonRaw != nullptr) {
        free(_jsonRaw);
        _jsonRaw = nullptr;
        _jsonRawLen = 0;
    }

    uint8_t* raw = nullptr;
    size_t rawLen = 0;

    if (!httpGetGzip(url, raw, rawLen)) {
        Serial.println("GZIP download/decompress failed.");
        return false;
    }

    // Parse in zero-copy mode and keep the backing buffer alive in the class.
    DeserializationError err = deserializeJson(_doc, reinterpret_cast<char*>(raw), rawLen);

    if (err) {
        Serial.printf("JSON ERROR: %s\n", err.c_str());
        free(raw);
        return false;
    }

    _jsonRaw = raw;
    _jsonRawLen = rawLen;

    if (DEBUGING) {Serial.println("JSON OK!");}
    _valid = true;
    return true;
}

JsonDocument& NetworkClient::json() {
    return _doc;
}

bool NetworkClient::isValid() const {
    return _valid;
}
