// R36S Test19: exact RGB555 -> RGBA8888 conversion with optional ARM NEON.
// The arcade produces BGR555 in SRAM. SDL_PIXELFORMAT_ABGR8888 stores
// R8 G8 B8 FF bytes on little-endian R36S ARM64. No temporal filtering,
// changed palette values, guest emulation clock, or sprite skipping.
#pragma once
#include <cstddef>
#include <cstdint>
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

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

// Byte-oriented output permits both std::vector<uint32_t> and SDL_LockTexture
// destinations, including texture rows with an extra driver-specific pitch.
// Scalar fallback and final pixel tail are byte-for-byte identical.
inline void rgb555_to_abgr8888_image_bytes(const uint8_t* src, uint8_t* dst,
                                            size_t pixels, bool simd = true) {
    size_t i = 0;
#if defined(__aarch64__) && defined(__ARM_NEON)
    if (simd) {
        const uint16x8_t m5 = vdupq_n_u16(31);
        for (; i + 8 <= pixels; i += 8) {
            // vld1q_u8 tolerates arbitrary byte alignment; interpreting its
            // lanes as uint16_t follows little-endian AArch64 pixel order.
            const uint16x8_t px = vreinterpretq_u16_u8(vld1q_u8(src + 2*i));
            const uint16x8_t r5 = vandq_u16(px, m5);
            const uint16x8_t g5 = vandq_u16(vshrq_n_u16(px, 5), m5);
            const uint16x8_t b5 = vandq_u16(vshrq_n_u16(px, 10), m5);
            const uint16x8_t r8 = vorrq_u16(vshlq_n_u16(r5, 3), vshrq_n_u16(r5, 2));
            const uint16x8_t g8 = vorrq_u16(vshlq_n_u16(g5, 3), vshrq_n_u16(g5, 2));
            const uint16x8_t b8 = vorrq_u16(vshlq_n_u16(b5, 3), vshrq_n_u16(b5, 2));
            uint8x8x4_t rgba;
            rgba.val[0] = vmovn_u16(r8);
            rgba.val[1] = vmovn_u16(g8);
            rgba.val[2] = vmovn_u16(b8);
            rgba.val[3] = vdup_n_u8(255);
            vst4_u8(dst + 4*i, rgba);
        }
    }
#else
    (void)simd;
#endif
    for (; i < pixels; ++i) {
        const uint16_t v = static_cast<uint16_t>(
            src[2*i] | (static_cast<uint16_t>(src[2*i+1]) << 8));
        const uint32_t packed = rgb555_to_abgr8888(v);
        dst[4*i] = static_cast<uint8_t>(packed);
        dst[4*i+1] = static_cast<uint8_t>(packed >> 8);
        dst[4*i+2] = static_cast<uint8_t>(packed >> 16);
        dst[4*i+3] = 255;
    }
}
inline void rgb555_to_abgr8888_image(const uint8_t* src,
                                      uint32_t* dst, size_t pixels) {
    rgb555_to_abgr8888_image_bytes(src, reinterpret_cast<uint8_t*>(dst), pixels);
}
// Direct texture upload: process each row without allocating / copying a
// 307200-byte intermediate CPU framebuffer. Accept any pitch >= width*4.
// 'simd=false' provides a reversible reference mode for on-device A/B tests.
inline void rgb555_to_abgr8888_pitched(const uint8_t* src, uint8_t* dst,
                                       size_t width, size_t height,
                                       size_t pitch, bool simd = true) {
    for (size_t row = 0; row < height; ++row)
        rgb555_to_abgr8888_image_bytes(src + row * width * 2,
                                        dst + row * pitch, width, simd);
}
}  // namespace ki
