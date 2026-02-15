#include "ImageDecoder.h"
#include <mbedtls/base64.h>

// Decoder for Base64 content
bool ImageDecoder::decodeBase64(const String& base64, uint8_t* outBuffer, size_t outSize, size_t& decodedSize) {
    int ret = mbedtls_base64_decode(
        outBuffer,
        outSize,
        &decodedSize,
        (const unsigned char*)base64.c_str(),
        base64.length()
    );
    return (ret == 0);
}
