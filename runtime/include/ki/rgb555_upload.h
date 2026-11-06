// R36S GPU Upload Test17: exact conversion of KI's packed BGR555
// framebuffer to SDL's native 32-bit ABGR8888 texture pixel format.
//
// On a little-endian ARM64 host, ABGR8888 is laid out in memory as
// R8,G8,B8,A8; SDL's GLES2 renderer can upload it as GL_RGBA without
// the costly SDL RGB24 (three-bytes-per-pixel) staging conversion.
//
// No color quantization is introduced: each 5-bit component uses the
// exact original frame conversion (v<<3)|(v>>2). The alpha is fully opaque.
#pragma once
#include <cstddef>
#include <cstdint>
namespace ki {
inline uint8_t rgb555_expand5(uint32_t v5) {
    return static_cast<uint8_t>((v5 << 3) | (v5 >> 2));
}
inline uint32_t rgb555_to_abgr8888(uint16_t v) {
    const uint32_t r = rgb555_expand5(v & 31u);
    const uint32_t g = rgb555_expand5((v >> 5) & 31u);
    const uint32_t b = rgb555_expand5((v >> 10) & 31u);
    return 0xff000000u | (b << 16) | (g << 8) | r;
}
inline void rgb555_to_abgr8888_image(const uint8_t* src,
                                      uint32_t* dst, size_t pixels) {
    for (size_t i = 0; i < pixels; ++i) {
        const uint16_t v = static_cast<uint16_t>(
            src[2 * i] | (static_cast<uint16_t>(src[2 * i + 1]) << 8));
        dst[i] = rgb555_to_abgr8888(v);
    }
}
} // namespace ki
