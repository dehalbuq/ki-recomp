// ROM-free Test17: validate native GLES2 ABGR8888 upload packing against
// original KI BGR555 -> RGB24 conversion for ALL 32,768 original pixel values,
// and the complete 320x240 framebuffer. No game assets or SDL required.
#include "ki/rgb555_upload.h"
#include "ki/kinst_board.h"
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    for (uint32_t raw = 0; raw < 32768u; ++raw) {
        const uint16_t v = static_cast<uint16_t>(raw);
        const uint32_t p = ki::rgb555_to_abgr8888(v);
        const uint8_t r = static_cast<uint8_t>(((v & 31u) << 3) | ((v & 31u) >> 2));
        const uint8_t g = static_cast<uint8_t>((((v >> 5) & 31u) << 3) | (((v >> 5) & 31u) >> 2));
        const uint8_t b = static_cast<uint8_t>((((v >> 10) & 31u) << 3) | (((v >> 10) & 31u) >> 2));
        if (static_cast<uint8_t>(p) != r ||
            static_cast<uint8_t>(p >> 8) != g ||
            static_cast<uint8_t>(p >> 16) != b ||
            static_cast<uint8_t>(p >> 24) != 255u) {
            std::fprintf(stderr, "BGR555->ABGR8888 mismatch at 0x%04x\n", v);
            return 1;
        }
    }
    // High bit is unused by arcade RGB555; all 16-bit values must agree.
    ki::Context context;
    ki::KinstBoard board({}, {});
    constexpr uint32_t base = 0x30000;
    for (uint32_t i=0; i<320u*240u; ++i) {
        const uint16_t v = static_cast<uint16_t>((i * 373u) & 0xffffu);
        context.sram[base + i*2] = static_cast<uint8_t>(v);
        context.sram[base + i*2+1] = static_cast<uint8_t>(v >> 8);
    }
    std::vector<uint8_t> rgb;
    std::vector<uint32_t> rgba;
    board.framebuffer_rgb_into(context, rgb);
    board.framebuffer_rgba_into(context, rgba);
    if (rgb.size() != 320u*240u*3u || rgba.size() != 320u*240u) return 2;
    for (uint32_t i=0; i<320u*240u; ++i) {
        const uint32_t packed = rgba[i];
        if (rgb[3*i] != static_cast<uint8_t>(packed) ||
            rgb[3*i+1] != static_cast<uint8_t>(packed >> 8) ||
            rgb[3*i+2] != static_cast<uint8_t>(packed >> 16) ||
            static_cast<uint8_t>(packed >> 24) != 255) {
            std::fprintf(stderr, "framebuffer mismatch pixel=%u\n", i);
            return 3;
        }
    }
    std::printf("GLES2 RGBA upload exact colors: 32768 input colors + %u full-frame pixels PASS\n",
        320u*240u);
    return 0;
}
