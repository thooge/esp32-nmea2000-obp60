
#pragma once
#include <Arduino.h>
#include <vector>

class ImageDecoder {
public:
    bool decodeBase64(const String& base64, uint8_t* outBuffer, size_t outSize, size_t& decodedSize);
};
