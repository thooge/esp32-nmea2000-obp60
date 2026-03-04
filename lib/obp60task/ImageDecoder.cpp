#include "ImageDecoder.h"
#include <mbedtls/base64.h>

// Decoder for Base64 content
bool ImageDecoder::decodeBase64(const char* base64, size_t base64Len, uint8_t* outBuffer, size_t outSize, size_t& decodedSize) {
    if (base64 == nullptr) {
        decodedSize = 0;
        return false;
    }
    int ret = mbedtls_base64_decode(
        outBuffer,
        outSize,
        &decodedSize,
        (const unsigned char*)base64,
        base64Len
    );
    return (ret == 0);
}

// Decoder for Base64 content
bool ImageDecoder::decodeBase64(const String& base64, uint8_t* outBuffer, size_t outSize, size_t& decodedSize) {
    return decodeBase64(base64.c_str(), base64.length(), outBuffer, outSize, decodedSize);
}
