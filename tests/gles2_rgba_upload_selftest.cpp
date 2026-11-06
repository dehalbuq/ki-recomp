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
    // Test19: compare the AArch64 NEON ST4 byte output against a purely
    // scalar reference, including every 16-bit source value (bit15 ignored),
    // source/destination misalignment, odd scan widths and driver row pitch.
    for (size_t width : {size_t(1), size_t(3), size_t(7), size_t(8),
                         size_t(9), size_t(15), size_t(16), size_t(31),
                         size_t(320)}) {
        for (size_t rows : {size_t(1), size_t(3), size_t(240)}) {
            const size_t n = width * rows;
            const size_t pitch = width * 4 + 13;
            std::vector<uint8_t> source(n * 2 + 2, 0);
            for (size_t k=0; k<n; ++k) {
                const uint16_t v = static_cast<uint16_t>((k * 12713u + 0xa538u) & 0xffffu);
                source[1 + 2*k] = static_cast<uint8_t>(v);
                source[1 + 2*k + 1] = static_cast<uint8_t>(v >> 8);
            }
            auto preserved = source;
            std::vector<uint8_t> expect(rows * pitch + 8, 0xa5);
            std::vector<uint8_t> actual(rows * pitch + 8, 0xa5);
            ki::rgb555_to_abgr8888_pitched(
                source.data() + 1, expect.data() + 3, width, rows, pitch, false);
            ki::rgb555_to_abgr8888_pitched(
                source.data() + 1, actual.data() + 3, width, rows, pitch, true);
            if (actual != expect || preserved != source) {
                std::fprintf(stderr, "Test19 NEON/scalar pitched mismatch %zu x %zu\n",
                             width, rows);
                return 4;
            }
            for (size_t row=0; row<rows; ++row)
                for (size_t x=0; x<width; ++x) {
                    const size_t off = 3 + row*pitch + x*4;
                    const uint16_t v = static_cast<uint16_t>(
                        source[1 + 2*(row*width+x)] |
                        (source[1 + 2*(row*width+x)+1] << 8));
                    const uint32_t p = ki::rgb555_to_abgr8888(v);
                    for (size_t b=0; b<4; ++b)
                        if (actual[off+b] != static_cast<uint8_t>(p >> (8*b))) {
                            std::fprintf(stderr, "Test19 incorrect pixel %zu/%zu\n", x, row);
                            return 5;
                        }
                }
        }
    }
    // Exhaustive contiguous SIMD lanes, including every bit15 combination,
    // contrasted with scalar output instead of only repeating 0..32767.
    std::vector<uint8_t> exhaustive(65536u * 2u);
    for (uint32_t v=0; v<65536u; ++v) {
        exhaustive[2*v] = static_cast<uint8_t>(v);
        exhaustive[2*v+1] = static_cast<uint8_t>(v >> 8);
    }
    std::vector<uint8_t> scalar_out(65536u*4u);
    std::vector<uint8_t> simd_out(65536u*4u);
    ki::rgb555_to_abgr8888_image_bytes(exhaustive.data(), scalar_out.data(), 65536u, false);
    ki::rgb555_to_abgr8888_image_bytes(exhaustive.data(), simd_out.data(), 65536u, true);
    if (scalar_out != simd_out) return 6;
    std::printf("GLES2 RGBA upload exact colors: 32768 input colors + %u full-frame pixels PASS\n",
        320u*240u);
    return 0;
}
