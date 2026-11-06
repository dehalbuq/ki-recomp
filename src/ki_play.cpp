// Plays the recompiled Killer Instinct program in a window (SDL2).
//
// usage: ki_play [--data <dir>] [--scale N] [--mute] [--opponent NAMES] [--stage N,...]
//        ki_play --boot <snapshot dir|rom> --disk <kinst.img> --rom <ki-l15d.u98>
//                [--sound-roms <dir>] [--scale N] [--mute]      (development)
//
// Normally the program starts like the arcade board, at the reset vector,
// from the player's MAME `kinst` set: a folder with kinst.zip (or its files)
// and kinst.chd, given with --data, dropped onto the window, or remembered
// from the last run. Every file is checked against MAME's SHA1. Sectors the
// game writes (settings, high scores) are kept in the user's preferences
// folder and put back on the next start.
//
// The guest runs on the main thread; at every VBLANK the board callback
// presents the framebuffer, reads input, queues the DCS samples produced
// during the frame and paces emulation to the arcade's refresh rate
// (6.25 MHz / (406 x 261) = 58.98 Hz).
//
// Keys (player 1): arrows move, A S D = high quick/medium/fierce,
// Z X C = low quick/medium/fierce, 1 = start, 5 = coin.
// Player 2: I J K L move, Q W E / T Y U attacks... see kKeys. F2 = service
// (test switch), Esc quits, Tab toggles unthrottled speed (no sound while on).
// Gamepads work too (Xbox layout by default, see ki_pad.cpp); G opens the
// controller setup screen. F5 saves the state, F9 loads it.
// P pauses, R resets. --opponent NAME[,NAME...] and --stage N[,N...] choose the coming
// fights (in turn, one per fight); F6 / F7 cycle the opponent / stage of the
// next fight (shown in the title bar).
#include <SDL.h>
#ifdef _WIN32
#include <process.h>
#define execv _execv
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ki/kinst_board.h"
#include "ki/runtime.h"
#include "ki_boot.h"
#include "ki_fight.h"
#include "ki_pad.h"
#include "ki_pad_ui.h"
#ifdef KI_R36S
#include "ki_r36s_menu.h"
#include "ki_r36s_intro.h"
#include "ki_r36s_host_profiler.h"
#include "ki_r36s_gfx_profiler.h"
#include "ki_r36s_audio_smooth.h"
#include "ki_r36s_blue_boot_wipe.h"
#include "ki/rgb555_upload.h"
#endif
#ifdef KI_HAVE_FILES
#include "ki_files.h"
#endif
#ifdef KI_HAVE_HD
#include "ki_hd.h"
#endif

namespace {

struct KeyBit {
    SDL_Scancode key;
    int player;  // 1 or 2
    uint32_t bit;
};

// Bits of the P1/P2 ports (active low), MAME kinst.cpp INPUT_PORTS_START(kinst).
const KeyBit kKeys[] = {
    {SDL_SCANCODE_A, 1, 0x001},     {SDL_SCANCODE_S, 1, 0x002},     {SDL_SCANCODE_D, 1, 0x004},
    {SDL_SCANCODE_Z, 1, 0x008},     {SDL_SCANCODE_X, 1, 0x010},     {SDL_SCANCODE_C, 1, 0x020},
    {SDL_SCANCODE_UP, 1, 0x040},    {SDL_SCANCODE_DOWN, 1, 0x080},  {SDL_SCANCODE_LEFT, 1, 0x100},
    {SDL_SCANCODE_RIGHT, 1, 0x200}, {SDL_SCANCODE_1, 1, 0x400},     {SDL_SCANCODE_5, 1, 0x800},
    {SDL_SCANCODE_F2, 1, 0x1000},   // service mode (test switch)
    {SDL_SCANCODE_Q, 2, 0x001},     {SDL_SCANCODE_W, 2, 0x002},     {SDL_SCANCODE_E, 2, 0x004},
    {SDL_SCANCODE_T, 2, 0x008},     {SDL_SCANCODE_Y, 2, 0x010},     {SDL_SCANCODE_U, 2, 0x020},
    {SDL_SCANCODE_I, 2, 0x040},     {SDL_SCANCODE_K, 2, 0x080},     {SDL_SCANCODE_J, 2, 0x100},
    {SDL_SCANCODE_L, 2, 0x200},     {SDL_SCANCODE_2, 2, 0x400},     {SDL_SCANCODE_6, 2, 0x800},
};

constexpr double kFrameSeconds = 1.0 / (6.25e6 / (406.0 * 261.0));
constexpr const char* kTitle = "Killer Instinct (recompiled)";
constexpr int kWideW = 427;  // 240 * 16 / 9

void write_ppm_file(const std::string& path, const std::vector<uint8_t>& rgb, int w, int h) {
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        std::fwrite(rgb.data(), 1, rgb.size(), f);
        std::fclose(f);
    }
}

std::string pref_path() {
    char* p = SDL_GetPrefPath("kilnan", "KIRecompiled");
    std::string s = p ? p : "./";
    SDL_free(p);
    return s;
}

std::string read_text(const std::string& path) {
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    return s;
}

void write_text(const std::string& path, const std::string& text) {
    std::ofstream f(path);
    f << text << "\n";
}

// Disk sectors written by the game: [u32 lba][512 bytes] records.
void apply_saved_sectors(const std::string& path, std::vector<uint8_t>& disk) {
    std::ifstream f(path, std::ios::binary);
    uint8_t rec[4 + 512];
    while (f.read(reinterpret_cast<char*>(rec), sizeof rec)) {
        uint32_t lba = rec[0] | rec[1] << 8 | rec[2] << 16 | static_cast<uint32_t>(rec[3]) << 24;
        if ((static_cast<uint64_t>(lba) + 1) * 512 <= disk.size()) std::memcpy(&disk[lba * 512ull], rec + 4, 512);
    }
}

void save_sectors(const std::string& path, const ki::KinstBoard& board, const std::vector<uint32_t>& earlier) {
    std::vector<uint32_t> all = earlier;
    all.insert(all.end(), board.written_sectors().begin(), board.written_sectors().end());
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary);
        for (uint32_t lba : all) {
            uint8_t head[4] = {static_cast<uint8_t>(lba), static_cast<uint8_t>(lba >> 8),
                               static_cast<uint8_t>(lba >> 16), static_cast<uint8_t>(lba >> 24)};
            f.write(reinterpret_cast<const char*>(head), 4);
            f.write(reinterpret_cast<const char*>(&board.disk()[lba * 512ull]), 512);
        }
    }
    std::rename(tmp.c_str(), path.c_str());
}

std::vector<uint32_t> saved_lbas(const std::string& path) {
    std::vector<uint32_t> out;
    std::ifstream f(path, std::ios::binary);
    uint8_t rec[4 + 512];
    while (f.read(reinterpret_cast<char*>(rec), sizeof rec))
        out.push_back(rec[0] | rec[1] << 8 | rec[2] << 16 | static_cast<uint32_t>(rec[3]) << 24);
    return out;
}

}  // namespace

char** g_argv = nullptr;  // for the reset key

int main(int argc, char** argv) {
    g_argv = argv;
    std::string boot, disk, rom, sound_roms, data, hd_pack;
    static ki::FightChoice fight;
    std::string aspect_arg;
    int scale = 3;
    bool mute = false;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (k == "--mute") { mute = true; continue; }
        if (i + 1 >= argc) break;
        std::string v = argv[++i];
        if (k == "--boot") boot = v;
        else if (k == "--disk") disk = v;
        else if (k == "--rom") rom = v;
        else if (k == "--scale") scale = std::stoi(v);
        else if (k == "--sound-roms") sound_roms = v;
        else if (k == "--data") data = v;
        else if (k == "--hd-pack") hd_pack = v;
        else if (k == "--aspect") aspect_arg = v;  // 16:9 or 4:3 (remembered; V switches while playing)
        else if (k == "--opponent" && !ki::parse_characters(v, fight.opponents)) {
            std::fprintf(stderr, "--opponent: names or numbers 0-10 (jago combo spinal thunder glacius fulgore cinder sabrewulf orchid riptor eyedol)\n");
            return 1;
        } else if (k == "--stage" && !ki::parse_numbers(v, ki::kStageCount, fight.stages)) {
            std::fprintf(stderr, "--stage: numbers 0-15\n");
            return 1;
        }
    }
    ki::install_fight_choice(&fight);
#ifdef KI_R36S
    // Exact 320x240 -> 640x480 scaling, without changing the guest frame rate.
    scale = 2;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    // The DarkOSre log reported flags=0xe: OpenGL ES 2 plus driver VSync.
    // Disable it to avoid expensive blocking at SDL_RenderPresent, but keep
    // the existing guest 58.98 Hz frame pacer, so game speed does not increase.
    SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, "0", SDL_HINT_OVERRIDE);
    // Both modes preserve the exact same audio and accelerated blue boot.
    const bool r36s_audio_smooth = std::getenv("KI_R36S_AUDIO_SMOOTH") != nullptr;
    const char* boot_blue_setting = std::getenv("KI_R36S_BOOT_BLUE_FAST");
    const bool r36s_boot_blue_enabled = boot_blue_setting && boot_blue_setting[0] != '0';
#ifdef KI_R36S_RELEASE_CLEAN
    // No diagnostics or A/B benchmark paths in the final player.
    constexpr bool r36s_bench_no_dcs = false;
    constexpr bool r36s_audio_trace = false;
    constexpr bool r36s_dsp_hotspots = false;
    constexpr bool r36s_time_dsp = false;
    constexpr bool r36s_host_profile = false;
    constexpr bool r36s_gpu_profile = false;
    constexpr bool r36s_no_clear = false;
    bool r36s_fast_rgba = true;
    bool r36s_direct_texture = false;
    constexpr bool r36s_rgba_scalar = false;
    constexpr int r36s_bench_render_every = 1;
#else
    const bool r36s_bench_no_dcs = std::getenv("KI_R36S_BENCH_NO_DCS") != nullptr;
    const bool r36s_audio_trace = std::getenv("KI_R36S_AUDIO_TRACE") != nullptr;
    std::fprintf(stderr, "[r36s] pitch-preserving adaptive audio: %s\n",
                 r36s_audio_smooth ? "ON (experimental WSOLA)" : "OFF (original DSP output)");
    const bool r36s_dsp_hotspots = std::getenv("KI_R36S_DSP_HOTSPOTS") != nullptr;
    const bool r36s_time_dsp = std::getenv("KI_R36S_TIME_DSP") != nullptr;
    const bool r36s_host_profile = std::getenv("KI_R36S_HOST_PROFILE") != nullptr;
    // Only measure SDL texture upload, without timing every MIPS blitter.
    const bool r36s_gpu_profile = std::getenv("KI_R36S_GPU_PROFILE") != nullptr;
    // Test19: the 320x240 opaque game texture covers the entire 4:3 panel.
    // Opt-in: skip redundant clear of the same framebuffer each frame.
    const bool r36s_no_clear = std::getenv("KI_R36S_NO_CLEAR") != nullptr;
    // Test17: native SDL/GLES2 ABGR8888 upload with EXACT RGB555 colours.
    // RGB24 is still the normal release path and available as an A/B control.
    bool r36s_fast_rgba = std::getenv("KI_R36S_GLES2_RGBA") != nullptr;
    // Test19 A/B: direct SDL/GLES2 texture lock avoids intermediate RGBA
    // framebuffer and its extra 307 KB/frame CPU copy, where supported.
    bool r36s_direct_texture = std::getenv("KI_R36S_DIRECT_TEXTURE") != nullptr;
    // Compare NEON conversion against the exact scalar reference on RK3326.
    const bool r36s_rgba_scalar = std::getenv("KI_R36S_RGBA_SCALAR") != nullptr;
    const int r36s_bench_render_every = std::getenv("KI_R36S_BENCH_RENDER_QUARTER") ? 4 : 1;
#endif
#ifndef KI_R36S_RELEASE_CLEAN
    std::fprintf(stderr, "[r36s] fullscreen 4:3 / nearest / original 58.98 Hz timing; VSync requested OFF\n");
    if (r36s_bench_no_dcs)
        std::fprintf(stderr, "[r36s] BENCHMARK: audio DSP disabled (diagnosis only; NOT normal gameplay)\n");
    if (r36s_bench_render_every != 1)
        std::fprintf(stderr, "[r36s] BENCHMARK: draw 1 in 4 guest frames (diagnosis only; not default)\n");
#endif
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    Uint32 window_flags = SDL_WINDOW_RESIZABLE;
#ifdef KI_R36S
    window_flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    SDL_Window* window = SDL_CreateWindow(kTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320 * scale,
                                          240 * scale, window_flags);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#ifdef KI_R36S
    if (!renderer) {
        std::fprintf(stderr, "[r36s] accelerated renderer failed: %s; trying software\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
#endif
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
#ifdef KI_R36S
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (SDL_RenderSetVSync(renderer, 0) != 0)
        std::fprintf(stderr, "[r36s] WARNING: SDL_RenderSetVSync(0): %s\n", SDL_GetError());
#endif
    SDL_RendererInfo ri{};
    if (SDL_GetRendererInfo(renderer, &ri) == 0)
        std::fprintf(stderr, "[r36s] SDL renderer: %s, flags=0x%x; accelerated=%d, vsync=%d\n",
                     ri.name, ri.flags, !!(ri.flags & SDL_RENDERER_ACCELERATED),
                     !!(ri.flags & SDL_RENDERER_PRESENTVSYNC));
#endif
    // 16:9: the 4:3 picture in the middle of a 427x240 view; the sides show a
    // blurred, darkened copy of the picture ("ambient"). V switches, remembered.
    bool wide = read_text(pref_path() + "aspect.txt") == "16:9";
    if (!aspect_arg.empty()) wide = aspect_arg == "16:9";
    auto set_aspect = [&](bool w) {
#ifdef KI_R36S
        w = false; // physical R36S panel is 4:3
#endif
        wide = w;
        SDL_RenderSetLogicalSize(renderer, wide ? kWideW : 320, 240);
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        if (!(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)))
            SDL_SetWindowSize(window, wh * (wide ? kWideW : 320) / 240, wh);
        write_text(pref_path() + "aspect.txt", wide ? "16:9" : "4:3");
    };
    set_aspect(wide);
    SDL_Texture* ambient = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 40, 30);
    SDL_SetTextureScaleMode(ambient, SDL_ScaleModeLinear);
#ifdef KI_R36S
    const Uint32 frame_texture_format = r36s_fast_rgba ?
        SDL_PIXELFORMAT_ABGR8888 : SDL_PIXELFORMAT_RGB24;
#else
    const Uint32 frame_texture_format = SDL_PIXELFORMAT_RGB24;
#endif
    SDL_Texture* texture =
        SDL_CreateTexture(renderer, frame_texture_format, SDL_TEXTUREACCESS_STREAMING, 320, 240);
#ifdef KI_R36S
    if (!texture && r36s_fast_rgba) {
        std::fprintf(stderr, "[r36s] GLES2 RGBA texture unavailable: %s; falling back to RGB24\n",
                     SDL_GetError());
        r36s_fast_rgba = false;
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 320, 240);
    }
    std::fprintf(stderr, "[r36s] GLES2 framebuffer upload: %s, texture=%s, bytes_per_frame=%u\n",
        r36s_fast_rgba ? "ABGR8888 (native RGBA)" : "RGB24 (baseline)",
        SDL_GetPixelFormatName(r36s_fast_rgba ? SDL_PIXELFORMAT_ABGR8888 : SDL_PIXELFORMAT_RGB24),
        r36s_fast_rgba ? 320u * 240u * 4u : 320u * 240u * 3u);
#endif
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
#ifdef KI_R36S
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    if (r36s_direct_texture && !r36s_fast_rgba) {
        std::fprintf(stderr,
            "[r36s] TEST19: locked texture requires RGBA; using baseline SDL_UpdateTexture\n");
        r36s_direct_texture = false;
    }
#ifndef KI_R36S_RELEASE_CLEAN
    std::fprintf(stderr, "[r36s] TEST19 optional opaque fullscreen no-clear=%d\n",
                 r36s_no_clear ? 1 : 0);
    std::fprintf(stderr, "[r36s] TEST19 convert=%s upload=%s (direct lock auto-fallback)\n",
        r36s_rgba_scalar ? "scalar reference" :
#if defined(__aarch64__) && defined(__ARM_NEON)
        "AArch64 NEON SIMD",
#else
        "scalar (NEON unavailable)",
#endif
        r36s_direct_texture ? "SDL_LockTexture direct" : "SDL_UpdateTexture");
#endif
#endif
    SDL_Texture* hd_texture = nullptr;  // 4x picture when an HD pack is loaded (H toggles)

    // ---- R36S independent-port attribution and legal notice ----
    // Displayed before opening any commercial game data. The renderer
    // continues at 320x240/nearest afterwards. The entire intro module
    // is dormant during gameplay, with no persistent textures/timers.
#ifdef KI_R36S
    if (!ki_r36s::intro::show(renderer)) {
        SDL_DestroyTexture(texture);
        SDL_DestroyTexture(ambient);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
#endif

    // ---- the game's files ----
#ifdef KI_R36S
    const auto r36s_load_start = std::chrono::steady_clock::now();
#endif
    std::vector<uint8_t> disk_image, boot_rom;
    std::vector<std::vector<uint8_t>> sound;
    const bool dev = !boot.empty();
    std::string saves;  // file with the sectors the game wrote (player mode only)
    std::vector<uint32_t> earlier_saves;
    if (dev) {
        if (disk.empty() || rom.empty()) {
            std::fprintf(stderr, "usage: %s --boot <dir|rom> --disk <img> --rom <u98> [--sound-roms <dir>]\n", argv[0]);
            return 2;
        }
        disk_image = ki::read_file_or_die(disk);
        boot_rom = ki::read_file_or_die(rom);
        if (sound_roms.empty()) {
            size_t slash = rom.find_last_of('/');
            sound_roms = slash == std::string::npos ? "." : rom.substr(0, slash);
        }
        if (FILE* f = std::fopen((sound_roms + "/u10-l1").c_str(), "rb")) {
            std::fclose(f);
            sound = ki::read_sound_roms_or_die(sound_roms);
        }
    } else {
#ifdef KI_HAVE_FILES
        const std::string prefs = pref_path();
        if (data.empty()) data = read_text(prefs + "data_dir.txt");
        ki::GameFiles files;
        std::string error;
        bool ok = !data.empty() && ki::load_game_files(data, files, error, prefs);
        if (!ok) {
            std::string msg =
                (data.empty() ? std::string("Drag the folder with your MAME 'kinst' set onto this window.")
                              : "The game files in " + data + " cannot be used:\n" + error +
                                    "\n\nDrag another folder onto this window.") +
                "\n\nThe folder must contain kinst.zip (BIOS v1.5d) and kinst.chd (or a .zip with it).";
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, kTitle, msg.c_str(), window);
            SDL_SetWindowTitle(window, "Killer Instinct (recompiled): drop the kinst folder here");
            SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
            while (!ok) {
                SDL_RenderClear(renderer);
                SDL_RenderPresent(renderer);
                SDL_Event ev;
                if (!SDL_WaitEvent(&ev)) continue;
                if (ev.type == SDL_QUIT) return 0;
                if (ev.type != SDL_DROPFILE) continue;
                std::string dropped = ev.drop.file;
                SDL_free(ev.drop.file);
                // A dropped file (kinst.zip, kinst.chd) stands for its folder.
                if (!ki::load_game_files(dropped, files, error, prefs)) {
                    size_t slash = dropped.find_last_of('/');
                    std::string parent = slash == std::string::npos ? dropped : dropped.substr(0, slash);
                    std::string e2;
                    if (ki::load_game_files(parent, files, e2, prefs)) dropped = parent;
                    else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, kTitle, error.c_str(), window);
                        continue;
                    }
                }
                data = dropped;
                ok = true;
            }
            SDL_SetWindowTitle(window, kTitle);
        }
        std::error_code ec;
        std::string absolute = std::filesystem::absolute(data, ec).lexically_normal().string();
        write_text(prefs + "data_dir.txt", ec ? data : absolute);
        disk_image = std::move(files.disk);
        boot_rom = std::move(files.boot_rom);
        sound = std::move(files.sound_roms);
        saves = prefs + "disk_writes.bin";
        earlier_saves = saved_lbas(saves);
        apply_saved_sectors(saves, disk_image);
#else
        std::fprintf(stderr, "built without KI_WITH_CHD: use --boot/--disk/--rom\n");
        return 2;
#endif
    }
#ifdef KI_R36S
    std::fprintf(stderr, "[r36s] game data loaded: %.2f s; expanded disk %.1f MiB\n",
        std::chrono::duration<double>(std::chrono::steady_clock::now() - r36s_load_start).count(),
        static_cast<double>(disk_image.size()) / (1024 * 1024));
#endif

    static ki::Context c;
    // --boot rom (and player mode): start at the reset vector, like the real board.
    const bool cold = !dev || boot == "rom";
    uint32_t entry = cold ? ki::cold_reset(c) : ki::load_boot_state(c, boot);
#ifdef KI_R36S
    c.fast_cop0 = std::getenv("KI_R36S_COP0_BASELINE") == nullptr;
    std::fprintf(stderr, "[r36s] COP0 MFC0 inlining: %s\n",
                 c.fast_cop0 ? "ON" : "OFF (baseline)");
    // Test14 showed a repeatable, large speedup on the actual RK3326:
    // make its verified IRQ-idle path the default. The baseline launcher
    // can still disable it for A/B tests or any unexpected gameplay issue.
    c.fast_mips_idle = std::getenv("KI_R36S_MIPS_IDLE_BASELINE") == nullptr;
    std::fprintf(stderr, "[r36s] MIPS event-driven idle accelerator: %s\n",
                 c.fast_mips_idle ? "ON (default)" : "OFF (baseline)");
    c.fast_mips_memory = std::getenv("KI_R36S_MEM_BASELINE") == nullptr;
    std::fprintf(stderr, "[r36s] MIPS physical RAM fastpath: %s\n",
                 c.fast_mips_memory ? "ON" : "OFF (baseline)");
#endif
    static ki::KinstBoard board(std::move(disk_image), std::move(boot_rom));
    c.io = &board;
    if (cold) board.set_cycles_to_vblank(0);
    static std::unique_ptr<ki::Dcs> dcs;
    SDL_AudioDeviceID audio = 0;
#ifdef KI_R36S
    ki_r36s::AudioSmooth audio_smoother;
    ki_r36s::BlueBootWipe r36s_boot_blue_detector;
    bool r36s_boot_blue_fast = false;
    if (r36s_boot_blue_enabled)
        std::fprintf(stderr,
            "[r36s] BOOT_BLUE enabled: fast-forward confirmed early blue wipe only; original gameplay speed preserved\n");
    bool r36s_audio_primed = false;
    bool r36s_audio_clock_valid = false;
    std::chrono::steady_clock::time_point r36s_audio_last_vblank;
#endif
    if (!sound.empty()
#ifdef KI_R36S
        && !r36s_bench_no_dcs
#endif
        ) {
        dcs = std::make_unique<ki::Dcs>(sound);
#ifdef KI_R36S
        if (r36s_dsp_hotspots) dcs->enable_pc_profiling(true);
#endif
        board.dcs = dcs.get();
        if (!mute) {
            // KI's DCS program drives the DAC at 31250 Hz; SDL converts to the device rate.
            SDL_AudioSpec want{};
            want.freq = 31250;
            want.format = AUDIO_S16SYS;
            want.channels = 1;
            want.samples = 1024;
            audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
            if (!audio) std::fprintf(stderr, "no audio: %s\n", SDL_GetError());
            else {
#ifdef KI_R36S
                if (r36s_audio_smooth) {
                    // Keep device paused until ~90ms of ORIGINAL game audio
                    // (after pitch-preserving time stretching) has accumulated.
                    // Startup padding with silence is avoided.
                    SDL_PauseAudioDevice(audio, 1);
                    std::fprintf(stderr,
                        "[r36s] AUDIO_SMOOTH sink=%dHz S16 mono, priming=%ums target=%ums\n",
                        want.freq, ki_r36s::AudioSmooth::kStartQueueMs,
                        ki_r36s::AudioSmooth::kTargetQueueMs);
                } else
#endif
                SDL_PauseAudioDevice(audio, 0);
            }
        }
    }

#ifdef KI_HAVE_HD
    static ki::HdRenderer hd;
    bool hd_on = false;
    if (!hd_pack.empty()) {
        if (hd.load_pack(hd_pack)) {
            hd.attach();
            hd_on = true;
            hd_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                           ki::HdRenderer::kW, ki::HdRenderer::kH);
            std::fprintf(stderr, "HD pack: %zu sprites, %zu shapes (H switches HD on and off)\n", hd.pack_size(), hd.shape_count());
        } else {
            std::fprintf(stderr, "no HD pack in %s\n", hd_pack.c_str());
        }
    }
#endif
    static ki::SaveState state;  // F5 / F9
    state.bind(board, dcs.get(), sound);

    bool turbo = false;
    std::vector<SDL_GameController*> pads;  // opened as SDL reports them (SDL_CONTROLLERDEVICEADDED)
#ifdef KI_R36S
    // Some DarkOSre builds expose the internal pad as a raw joystick rather
    // than a mapped SDL_GameController. Read Start=13 and Select=12 directly.
    std::vector<SDL_Joystick*> raw_joysticks;
    auto open_raw = [&](int device_index) {
        if (device_index < 0 || device_index >= SDL_NumJoysticks()) return;
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device_index);
        for (SDL_Joystick* j : raw_joysticks)
            if (j && SDL_JoystickInstanceID(j) == id) return;
        SDL_Joystick* joy = SDL_JoystickOpen(device_index);
        if (joy) {
            raw_joysticks.push_back(joy);
            std::fprintf(stderr, "[r36s] joystick '%s', buttons=%d, instance=%d\n",
                         SDL_JoystickName(joy), SDL_JoystickNumButtons(joy), static_cast<int>(id));
        }
    };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        open_raw(i);
        if (SDL_IsGameController(i)) {
            if (SDL_GameController* pad = SDL_GameControllerOpen(i)) pads.push_back(pad);
        }
    }
    std::fprintf(stderr, "[r36s] detected %d joystick device(s), %zu game controller(s)\n",
                 SDL_NumJoysticks(), pads.size());
    bool r36s_menu_open = false;
    int r36s_menu_choice = 0;
    Uint32 r36s_menu_action_ms = 0;
#ifndef KI_R36S_RELEASE_CLEAN
    auto r36s_sample_start = std::chrono::steady_clock::now();
    unsigned r36s_sample_frames = 0;
    // Optional deterministic PCM trace for interpreter vs future KI HLE tests.
    // Only enabled in diagnostic builds.
    uint64_t r36s_audio_samples = 0;
    uint64_t r36s_audio_hash = 14695981039346656037ull; // FNV-1a 64-bit offset basis
    unsigned r36s_present_frames = 0;
    uint64_t r36s_waitloop_last = 0;
    uint64_t r36s_native_opcode_last = 0;
    uint64_t r36s_mips_idle_last = 0;
    double r36s_render_ms = 0, r36s_rgb_ms = 0, r36s_swap_ms = 0;
    // Diagnostic-only split of SDL/ES2 upload vs. draw submission.
    double r36s_tex_upload_ms = 0, r36s_gpu_copy_ms = 0;
    double r36s_direct_convert_ms = 0;
    auto r36s_cpu_begin = std::clock();
    auto r36s_proc_read_bytes = []() -> unsigned long long {
        std::ifstream file("/proc/self/io");
        std::string key;
        unsigned long long value = 0;
        while (file >> key >> value)
            if (key == "read_bytes:") return value;
        return 0;
    };
    auto r36s_rss_kib = []() -> unsigned long {
        std::ifstream file("/proc/self/status");
        std::string key;
        unsigned long value = 0;
        while (file >> key) {
            if (key == "VmRSS:") { file >> value; return value; }
            std::string remainder;
            std::getline(file, remainder);
        }
        return 0;
    };
    auto r36s_cpu_khz = []() -> unsigned long {
        std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        unsigned long val = 0;
        f >> val;
        return val;
    };
    unsigned long long r36s_read_begin = r36s_proc_read_bytes();
#endif // !KI_R36S_RELEASE_CLEAN
    std::vector<uint8_t> r36s_frame_rgb;
    r36s_frame_rgb.reserve(320 * 240 * 3);
    // Preallocated (resized once) and reused: no per-frame GPU staging alloc.
    std::vector<uint32_t> r36s_frame_rgba;
    if (r36s_fast_rgba) r36s_frame_rgba.reserve(320 * 240);
#endif
    uint64_t saved_writes = 0;
    uint64_t title_reset_frame = 0;
    auto save = [&]() {
        if (!saves.empty() && board.disk_writes() != saved_writes) {
            save_sectors(saves, board, earlier_saves);
            saved_writes = board.disk_writes();
        }
    };
    auto next_frame = std::chrono::steady_clock::now();
    bool paused = false;
    // Controller bindings: G opens the setup screen (game paused meanwhile).
    static std::vector<ki::pad::Action> pad_actions = ki::pad::load_bindings(pref_path() + "pad_map.txt");
    static ki::pad::SetupScreen pad_setup(pad_actions, pref_path() + "pad_map.txt");
    auto present = [&](ki::Context& ctx) {
#ifdef KI_R36S
#ifndef KI_R36S_RELEASE_CLEAN
        const auto render_begin = std::chrono::steady_clock::now();
#endif
#endif
#ifdef KI_R36S
        if (!r36s_no_clear || wide) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
        }
#else
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
#endif
#ifdef KI_R36S
#ifndef KI_R36S_RELEASE_CLEAN
        const auto rgb_begin = std::chrono::steady_clock::now();
#endif
        if (r36s_fast_rgba) {
            if (!r36s_direct_texture) {
                if (r36s_rgba_scalar) {
                    r36s_frame_rgba.resize(320u * 240u);
                    ki::rgb555_to_abgr8888_image_bytes(
                        ctx.sram.data() + board.displayed_bank(),
                        reinterpret_cast<uint8_t*>(r36s_frame_rgba.data()),
                        320u * 240u, false);
                } else {
                    board.framebuffer_rgba_into(ctx, r36s_frame_rgba);
                }
            }
            // R36S always uses 4:3, but retain the ambient RGB24 fallback
            // should a future test switch the physical aspect mode.
            if (wide) board.framebuffer_rgb_into(ctx, r36s_frame_rgb);
        } else {
            board.framebuffer_rgb_into(ctx, r36s_frame_rgb);
        }
        const std::vector<uint8_t>& rgb = r36s_frame_rgb;
#ifndef KI_R36S_RELEASE_CLEAN
        r36s_rgb_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - rgb_begin).count();
#endif
#else
        auto rgb = board.framebuffer_rgb(ctx);
#endif
        const SDL_FRect game{wide ? (kWideW - 320) / 2.0f : 0.0f, 0.0f, 320.0f, 240.0f};
        if (wide) {
            // Ambient sides: the picture averaged down to 40x30, stretched over the
            // whole view (the linear filter blurs it), then darkened.
            std::vector<uint8_t> small(40 * 30 * 3);
            for (int y = 0; y < 30; ++y)
                for (int x = 0; x < 40; ++x)
                    for (int ch = 0; ch < 3; ++ch) {
                        int sum = 0;
                        for (int yy = 0; yy < 8; ++yy)
                            for (int xx = 0; xx < 8; ++xx) sum += rgb[((y * 8 + yy) * 320 + x * 8 + xx) * 3 + ch];
                        small[(y * 40 + x) * 3 + ch] = static_cast<uint8_t>(sum / 64);
                    }
            SDL_UpdateTexture(ambient, nullptr, small.data(), 40 * 3);
            // Each side continues the picture's outer columns (blurred by the
            // linear filter), fading to dark towards the window's edge.
            const SDL_Rect left_src{0, 0, 2, 30}, right_src{38, 0, 2, 30};
            const SDL_FRect left_dst{0.0f, 0.0f, game.x, 240.0f}, right_dst{game.x + 320.0f, 0.0f, game.x, 240.0f};
            SDL_RenderCopyF(renderer, ambient, &left_src, &left_dst);
            SDL_RenderCopyF(renderer, ambient, &right_src, &right_dst);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            const int side = static_cast<int>(game.x);
            for (int x = 0; x < side; ++x) {
                Uint8 a = static_cast<Uint8>(60 + 170 * (side - x) / side);  // darker away from the picture
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
                SDL_RenderDrawLine(renderer, x, 0, x, 239);
                SDL_RenderDrawLine(renderer, kWideW - 1 - x, 0, kWideW - 1 - x, 239);
            }
        }
#ifdef KI_HAVE_HD
        // The HD copy is kept up to date even when not shown, so H switches instantly.
        if (hd_texture) {
            const std::vector<uint32_t>& px = hd.frame(ctx, board.displayed_bank());
            if (hd_on) {
                // 16:9 on stages drawn from a wide panorama: the real sky and
                // buildings beyond the picture's edges, over the ambient fill.
                static std::vector<uint32_t> left, right;
                static SDL_Texture* side_tex[2] = {};
                constexpr int kSide = 54;
                if (wide && hd.sides(ctx, board.displayed_bank(), kSide, left, right)) {
                    for (int i = 0; i < 2; ++i) {
                        if (!side_tex[i]) {
                            side_tex[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                                            kSide * ki::HdRenderer::kScale, ki::HdRenderer::kH);
                            SDL_SetTextureBlendMode(side_tex[i], SDL_BLENDMODE_BLEND);
                        }
                        SDL_UpdateTexture(side_tex[i], nullptr, (i ? right : left).data(), kSide * ki::HdRenderer::kScale * 4);
                    }
                    const SDL_FRect l{game.x - kSide, 0.0f, static_cast<float>(kSide), 240.0f}, r{game.x + 320.0f, 0.0f, static_cast<float>(kSide), 240.0f};
                    SDL_RenderCopyF(renderer, side_tex[0], nullptr, &l);
                    SDL_RenderCopyF(renderer, side_tex[1], nullptr, &r);
                }
                SDL_UpdateTexture(hd_texture, nullptr, px.data(), ki::HdRenderer::kW * 4);
                SDL_RenderCopyF(renderer, hd_texture, nullptr, &game);
            }
        }
        if (!hd_on)
#endif
        {
#ifdef KI_R36S
#ifndef KI_R36S_RELEASE_CLEAN
            const bool timing_gpu = r36s_gpu_profile || ki_r36s::gfx_profiler::enabled();
            const auto gpu_begin = timing_gpu ? std::chrono::steady_clock::now()
                                             : std::chrono::steady_clock::time_point{};
#endif
#endif
#ifdef KI_R36S
            if (r36s_fast_rgba && r36s_direct_texture) {
                void* locked_pixels = nullptr;
                int locked_pitch = 0;
                const int lock_result = SDL_LockTexture(
                    texture, nullptr, &locked_pixels, &locked_pitch);
                if (lock_result == 0 && locked_pixels && locked_pitch >= 320*4) {
#ifndef KI_R36S_RELEASE_CLEAN
                    const auto convert_begin = timing_gpu ? std::chrono::steady_clock::now()
                        : std::chrono::steady_clock::time_point{};
#endif
                    ki::rgb555_to_abgr8888_pitched(
                        ctx.sram.data() + board.displayed_bank(),
                        static_cast<uint8_t*>(locked_pixels),
                        320u, 240u, static_cast<size_t>(locked_pitch),
                        !r36s_rgba_scalar);
#ifndef KI_R36S_RELEASE_CLEAN
                    if (timing_gpu)
                        r36s_direct_convert_ms += std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - convert_begin).count();
#endif
                    SDL_UnlockTexture(texture);
                } else {
                    // Firmware SDL2 drivers may reject streaming texture locks:
                    // log once then permanently use the tested RGBA Update path.
                    if (lock_result == 0) SDL_UnlockTexture(texture);
                    std::fprintf(stderr,
                        "[r36s] TEST19 direct texture unsupported (pitch=%d): %s; fallback=RGBA Update\n",
                        locked_pitch, SDL_GetError());
                    r36s_direct_texture = false;
                    board.framebuffer_rgba_into(ctx, r36s_frame_rgba);
                    SDL_UpdateTexture(texture, nullptr, r36s_frame_rgba.data(), 320 * 4);
                }
            } else if (r36s_fast_rgba) {
                SDL_UpdateTexture(texture, nullptr, r36s_frame_rgba.data(), 320 * 4);
            } else {
                SDL_UpdateTexture(texture, nullptr, rgb.data(), 320 * 3);
            }
#else
            SDL_UpdateTexture(texture, nullptr, rgb.data(), 320 * 3);
#endif
#ifdef KI_R36S
#ifndef KI_R36S_RELEASE_CLEAN
            const auto gpu_uploaded = timing_gpu ? std::chrono::steady_clock::now()
                                                : std::chrono::steady_clock::time_point{};
#endif
#endif
            SDL_RenderCopyF(renderer, texture, nullptr, &game);
#ifdef KI_R36S
#ifndef KI_R36S_RELEASE_CLEAN
            if (timing_gpu) {
                const auto gpu_copied = std::chrono::steady_clock::now();
                r36s_tex_upload_ms += std::chrono::duration<double, std::milli>(
                    gpu_uploaded - gpu_begin).count();
                r36s_gpu_copy_ms += std::chrono::duration<double, std::milli>(
                    gpu_copied - gpu_uploaded).count();
            }
#endif
#endif
        }
        // The setup screen is laid out for the 4:3 area.
        SDL_Rect area{static_cast<int>(game.x), 0, 320, 240};
        SDL_RenderSetViewport(renderer, &area);
        pad_setup.draw(renderer, pads);
        SDL_RenderSetViewport(renderer, nullptr);
#ifdef KI_R36S
        if (r36s_menu_open) ki_r36s::draw(renderer, r36s_menu_choice);
#ifndef KI_R36S_RELEASE_CLEAN
        const auto swap_begin = std::chrono::steady_clock::now();
#endif
#endif
        SDL_RenderPresent(renderer);
#ifdef KI_R36S
#ifndef KI_R36S_RELEASE_CLEAN
        r36s_swap_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - swap_begin).count();
        r36s_render_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - render_begin).count();
        ++r36s_present_frames;
#endif
#endif
    };
    board.on_vblank = [&](ki::Context& ctx, uint64_t frame) {
#ifdef KI_R36S
        if (r36s_boot_blue_enabled && !turbo && !paused && !r36s_menu_open) {
            const bool detected_fast = r36s_boot_blue_detector.update(
                ctx.sram.data() + board.displayed_bank(), frame);
            if (detected_fast != r36s_boot_blue_fast) {
                r36s_boot_blue_fast = detected_fast;
#ifndef KI_R36S_RELEASE_CLEAN
                const auto& p = r36s_boot_blue_detector.last_probe();
                std::fprintf(stderr,
                    "[r36s] BOOT_BLUE transition=%s guest_frame=%llu top_blue_rows=%u/30 sampled_blue=%u/30\n",
                    detected_fast ? "FAST" : "NORMAL",
                    static_cast<unsigned long long>(frame),
                    p.top_rows, p.blue_rows);
#endif
                // Startup turbo drops *playback* (not emulated DSP work) and
                // resets the SDL queue, so boot audio cannot play back late
                // when the original 58.98 Hz gameplay pacing resumes.
                if (audio && r36s_audio_smooth) {
                    SDL_PauseAudioDevice(audio, 1);
                    SDL_ClearQueuedAudio(audio);
                    audio_smoother.reset();
                    r36s_audio_primed = false;
                    r36s_audio_clock_valid = false;
                }
                next_frame = std::chrono::steady_clock::now();
            }
#ifndef KI_R36S_RELEASE_CLEAN
            if (frame <= ki_r36s::BlueBootWipe::kMaxBootFrames && frame % 90 == 0) {
                const auto& p = r36s_boot_blue_detector.last_probe();
                std::fprintf(stderr,
                    "[r36s] BOOT_BLUE probe guest_frame=%llu blue_rows=%u/30 top_rows=%u bottom_rows=%u accelerated=%d\n",
                    static_cast<unsigned long long>(frame),
                    p.blue_rows, p.top_rows, p.bottom_rows,
                    r36s_boot_blue_fast ? 1 : 0);
            }
#endif // !KI_R36S_RELEASE_CLEAN
        }
#endif
        SDL_Event ev;
        for (bool first = true; first || paused || pad_setup.open()
#ifdef KI_R36S
             || r36s_menu_open
#endif
             ; first = false) {
            if (!first) {
                // Paused: the frame stays on screen (H still switches HD, to compare), the game does not run.
                present(ctx);
                SDL_Delay(16);
                next_frame = std::chrono::steady_clock::now();
            }
            while (SDL_PollEvent(&ev)) {
#ifdef KI_R36S
                if (ev.type == SDL_JOYDEVICEADDED) open_raw(ev.jdevice.which);
                if (ev.type == SDL_JOYDEVICEREMOVED) {
                    for (SDL_Joystick*& joy : raw_joysticks)
                        if (joy && SDL_JoystickInstanceID(joy) == ev.jdevice.which) {
                            SDL_JoystickClose(joy);
                            joy = nullptr;
                        }
                    raw_joysticks.erase(std::remove(raw_joysticks.begin(), raw_joysticks.end(), nullptr),
                                        raw_joysticks.end());
                }
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat &&
                    ev.key.keysym.scancode == SDL_SCANCODE_F10 && !r36s_menu_open) {
                    r36s_menu_open = true;
                    r36s_menu_choice = 0;
                    if (audio) SDL_PauseAudioDevice(audio, 1);
                    std::fprintf(stderr, "[r36s] exit menu opened with F10\n");
                    continue;
                }
                if (r36s_menu_open) {
                    if (ev.type == SDL_QUIT) {
                        save();
                        if (audio) SDL_CloseAudioDevice(audio);
                        SDL_Quit();
                        std::exit(0);
                    }
                    int action = -1; // 0 up, 1 down, 2 accept, 3 cancel
                    if (ev.type == SDL_JOYBUTTONDOWN) {
                        if (ev.jbutton.button == 8) action = 0;
                        if (ev.jbutton.button == 9) action = 1;
                        if (ev.jbutton.button == 1) action = 2;
                        if (ev.jbutton.button == 0) action = 3;
                    } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                        if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) action = 0;
                        if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) action = 1;
                        if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) action = 2;
                        if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) action = 3;
                    } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                        SDL_Scancode sc = ev.key.keysym.scancode;
                        if (sc == SDL_SCANCODE_UP) action = 0;
                        if (sc == SDL_SCANCODE_DOWN) action = 1;
                        if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE) action = 2;
                        if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_F10) action = 3;
                    }
                    if (action >= 0 && SDL_GetTicks() - r36s_menu_action_ms > 150) {
                        r36s_menu_action_ms = SDL_GetTicks();
                        if (action == 0) r36s_menu_choice = 0;
                        if (action == 1) r36s_menu_choice = 1;
                        if (action == 3 || (action == 2 && r36s_menu_choice == 0)) {
                            r36s_menu_open = false;
                            if (audio && !paused) {
                                if (r36s_audio_smooth) {
                                    // Never play stale pre-menu audio at resume.
                                    SDL_ClearQueuedAudio(audio);
                                    audio_smoother.reset();
                                    r36s_audio_primed = false;
                                    r36s_audio_clock_valid = false;
                                } else {
                                    SDL_PauseAudioDevice(audio, 0);
                                }
                            }
                            std::fprintf(stderr, "[r36s] resume game\n");
                        } else if (action == 2 && r36s_menu_choice == 1) {
                            std::fprintf(stderr, "[r36s] exit selected; flushing save\n");
                            save();
                            if (audio) SDL_CloseAudioDevice(audio);
                            SDL_Quit();
                            std::exit(0);
                        }
                    }
                    continue; // do not inject menu button presses into arcade controls
                }
#endif
                if (ev.type == SDL_CONTROLLERDEVICEADDED || ev.type == SDL_CONTROLLERDEVICEREMOVED || ev.type == SDL_QUIT) {
                    // handled below even while the setup screen is open
                } else if (pad_setup.handle(ev)) {
                    continue;
                } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_G) {
                    pad_setup.toggle();
                    continue;
                }
                if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
                    save();
                    SDL_Quit();
                    std::exit(0);
                }
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_TAB && !ev.key.repeat) turbo = !turbo;
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat &&
                    (ev.key.keysym.scancode == SDL_SCANCODE_F6 || ev.key.keysym.scancode == SDL_SCANCODE_F7)) {
                    // Next fight: off -> 0 -> 1 ... -> off. Replaces any --opponent / --stage list.
                    bool opp = ev.key.keysym.scancode == SDL_SCANCODE_F6;
                    std::vector<int>& list = opp ? fight.opponents : fight.stages;
                    int limit = opp ? 11 : ki::kStageCount;
                    int next = list.size() == 1 ? list[0] + 1 : 0;
                    list.clear();
                    if (next < limit) list.push_back(next);
                    fight.fights = 0;
                    std::string t = std::string(kTitle) + ": next fight: opponent " +
                                    (fight.opponents.empty() ? "(game)" : ki::kCharacterNames[fight.opponents[0]]) +
                                    ", stage " + (fight.stages.empty() ? "(game)" : std::to_string(fight.stages[0]));
                    SDL_SetWindowTitle(window, t.c_str());
                }
#ifdef KI_HAVE_HD
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_M && hd_texture) {
                    // Diagnostics: the sprites recently drawn without HD, and the two pictures.
                    std::string base = pref_path() + "hd_report";
                    if (FILE* f = std::fopen((base + ".txt").c_str(), "w")) {
                        hd.dump_recent_misses(f);
                        std::fclose(f);
                    }
                    write_ppm_file(base + "_original.ppm", board.framebuffer_rgb(ctx), 320, 240);
                    const std::vector<uint32_t>& px = hd.frame(ctx, board.displayed_bank());
                    std::vector<uint8_t> rgb(px.size() * 3);
                    for (size_t i = 0; i < px.size(); ++i)
                        rgb[3 * i] = px[i] >> 16, rgb[3 * i + 1] = px[i] >> 8, rgb[3 * i + 2] = px[i];
                    write_ppm_file(base + "_hd.ppm", rgb, ki::HdRenderer::kW, ki::HdRenderer::kH);
                    SDL_SetWindowTitle(window, ("Killer Instinct (recompiled): report in " + base + ".txt").c_str());
                }
#endif
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_R) {
                    // Reset: like switching the board off and on (same arguments).
                    save();
                    if (audio) SDL_CloseAudioDevice(audio);
                    SDL_Quit();
                    execv(g_argv[0], g_argv);
                    std::perror("reset");
                    std::exit(1);
                }
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_V) set_aspect(!wide);
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_P) {
                    paused = !paused;
                    if (audio) {
#ifdef KI_R36S
                        if (r36s_audio_smooth) {
                            if (paused) {
                                SDL_PauseAudioDevice(audio, 1);
                                SDL_ClearQueuedAudio(audio);
                                audio_smoother.reset();
                                r36s_audio_primed = false;
                                r36s_audio_clock_valid = false;
                            } // resume remains paused until the buffer primes
                        } else
#endif
                        SDL_PauseAudioDevice(audio, paused ? 1 : 0);
                    }
                    SDL_SetWindowTitle(window, paused ? "Killer Instinct (recompiled): paused (P resumes)" : kTitle);
                }
#ifdef KI_HAVE_HD
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_H && hd_texture)
                    hd_on = !hd_on;
#endif
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_F5)
                    state.request(ctx);  // taken at the next dispatcher entry
                if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_F9 && state.valid())
                    throw ki::LoadStateRequest{};
                if (ev.type == SDL_CONTROLLERDEVICEADDED) {
                    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(ev.cdevice.which);
                    bool already_open = false;
                    for (SDL_GameController* p : pads)
                        if (p && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p)) == id)
                            already_open = true;
                    if (!already_open)
                        if (SDL_GameController* pad = SDL_GameControllerOpen(ev.cdevice.which)) pads.push_back(pad);
                }
                if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
                    for (SDL_GameController*& pad : pads)
                        if (pad && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == ev.cdevice.which) {
                            SDL_GameControllerClose(pad);
                            pad = nullptr;
                        }
                    pads.erase(std::remove(pads.begin(), pads.end(), nullptr), pads.end());
                }
            }
        }
        if (frame % 600 == 0) save();  // every ~10 s, if the game wrote anything
        if (state.just_saved()) {
            SDL_SetWindowTitle(window, "Killer Instinct (recompiled): state saved (F9 loads it)");
            title_reset_frame = frame + 120;
        }
        if (title_reset_frame && frame >= title_reset_frame) {
            SDL_SetWindowTitle(window, kTitle);
            title_reset_frame = 0;
        }
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        uint32_t p1 = 0xffffffff, p2 = 0xffffffff;
        for (const KeyBit& k : kKeys)
            if (keys[k.key]) (k.player == 1 ? p1 : p2) &= ~k.bit;
        int player = 0;
        for (SDL_GameController* pad : pads)
            if (pad && SDL_GameControllerGetAttached(pad) && player < 2) (player++ == 0 ? p1 : p2) &= ~ki::pad::pressed(pad, pad_actions);
#ifdef KI_R36S
        // Hold Start+Select for 800ms to open an actual on-screen exit menu.
        // Read raw R36S buttons (Select=12, Start=13) even if SDL has no
        // GameController mapping for this firmware.
        static Uint32 quit_combo_since = 0;
        bool quit_combo = false;
        for (SDL_Joystick* joy : raw_joysticks) {
            if (!joy || !SDL_JoystickGetAttached(joy) || SDL_JoystickNumButtons(joy) <= 13) continue;
            if (SDL_JoystickGetButton(joy, 12) && SDL_JoystickGetButton(joy, 13)) {
                quit_combo = true;
                break;
            }
        }
        for (SDL_GameController* pad : pads) {
            if (!pad || !SDL_GameControllerGetAttached(pad)) continue;
            const bool start = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
            const bool select = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) ||
                                SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_GUIDE);
            if (start && select) { quit_combo = true; break; }
        }
        if (!r36s_menu_open && quit_combo) {
            if (!quit_combo_since) quit_combo_since = SDL_GetTicks();
            else if (SDL_TICKS_PASSED(SDL_GetTicks(), quit_combo_since + 800u)) {
                r36s_menu_open = true;
                r36s_menu_choice = 0;
                if (audio) SDL_PauseAudioDevice(audio, 1);
                std::fprintf(stderr, "[r36s] Start+Select: exit menu opened\n");
            }
        } else {
            quit_combo_since = 0;
        }
#ifndef KI_R36S_RELEASE_CLEAN
        ++r36s_sample_frames;
        auto stamp = std::chrono::steady_clock::now();
        double secs = std::chrono::duration<double>(stamp - r36s_sample_start).count();
        if (secs >= 5.0) {
            std::fprintf(stderr, "[r36s] guest vblank rate: %.1f fps (frames=%u, %.1fs); audio queued=%u bytes\n",
                         r36s_sample_frames / secs, r36s_sample_frames, secs,
                         audio ? SDL_GetQueuedAudioSize(audio) : 0u);
            const auto cpu_now = std::clock();
            const double cpu_pct = 100.0 * static_cast<double>(cpu_now - r36s_cpu_begin) /
                (static_cast<double>(CLOCKS_PER_SEC) * secs);
            const unsigned long long reads_now = r36s_proc_read_bytes();
            const unsigned long long read_delta = reads_now >= r36s_read_begin ? reads_now - r36s_read_begin : 0;
            const double render_div = r36s_present_frames ? static_cast<double>(r36s_present_frames) : 1.0;
            std::fprintf(stderr,
                "[r36s] PERF fps=%.1f cpu=%.0f%% render=%.2fms rgb=%.2fms swap=%.2fms "
                "drawn=%u sd_read=%.2fMiB rss=%.1fMiB cpu_khz=%lu\n",
                r36s_sample_frames / secs, cpu_pct,
                r36s_render_ms / render_div, r36s_rgb_ms / render_div, r36s_swap_ms / render_div,
                r36s_present_frames, static_cast<double>(read_delta) / (1024.0 * 1024.0),
                r36s_rss_kib() / 1024.0, r36s_cpu_khz());
            if (std::getenv("KI_R36S_DISPATCH_PROFILE")) {
                const auto counts = ki::dispatch_cache_take_stats();
                const uint64_t total_calls = counts.hits + counts.misses;
                std::fprintf(stderr,
                    "[r36s] MIPS_DISPATCH lookups=%llu cache_hits=%llu misses=%llu hit_rate=%.1f%% baseline=%d\n",
                    static_cast<unsigned long long>(total_calls),
                    static_cast<unsigned long long>(counts.hits),
                    static_cast<unsigned long long>(counts.misses),
                    total_calls ? 100.0 * counts.hits / total_calls : 0.0,
                    std::getenv("KI_R36S_DISPATCH_BASELINE") ? 1 : 0);
            }
            if (r36s_host_profile)
                ki_r36s::host_profiler::report(r36s_sample_frames);
            ki_r36s::gfx_profiler::report(r36s_sample_frames);
            if (r36s_audio_smooth && audio) {
                const auto stats = audio_smoother.take_stats();
                std::fprintf(stderr,
                    "[r36s] AUDIO_SMOOTH frames=%llu in=%llu out=%llu "
                    "stretch=%.3f queue_min_ms=%u queue_max_ms=%u under20=%llu "
                    "primed=%d sdl_queue_ms=%u\n",
                    static_cast<unsigned long long>(stats.frames),
                    static_cast<unsigned long long>(stats.input_samples),
                    static_cast<unsigned long long>(stats.output_samples),
                    stats.stretch,
                    stats.min_queue_ms == 0xffffffffu ? 0u : stats.min_queue_ms,
                    stats.max_queue_ms,
                    static_cast<unsigned long long>(stats.low_queue_frames),
                    r36s_audio_primed ? 1 : 0,
                    SDL_GetQueuedAudioSize(audio) / (ki_r36s::AudioSmooth::kRate * 2u / 1000u));
            }
            if (r36s_gpu_profile || ki_r36s::gfx_profiler::enabled()) {
                std::fprintf(stderr,
                    "[r36s] GFX_SDL_ES2 upload_ms_per_frame=%.3f copy_submit_ms_per_frame=%.3f "
                    "total_render_ms_per_frame=%.3f swap_ms_per_frame=%.3f "
                    "cpu_rgb_ms_per_frame=%.3f samples=%u\n",
                    r36s_tex_upload_ms / render_div,
                    r36s_gpu_copy_ms / render_div,
                    r36s_render_ms / render_div,
                    r36s_swap_ms / render_div,
                    r36s_rgb_ms / render_div,
                    r36s_present_frames);
                std::fprintf(stderr,
                    "[r36s] TEST19 direct_texture_active=%d scalar=%d locked_convert_ms_per_frame=%.3f\n",
                    r36s_direct_texture ? 1 : 0, r36s_rgba_scalar ? 1 : 0,
                    r36s_direct_convert_ms / render_div);
            }
            if (c.fast_mips_idle) {
                const uint64_t saved = c.mips_idle_skipped_cycles;
                std::fprintf(stderr, "[r36s] MIPS_IDLE saved=%llu cycles_this_window total=%llu iterations=%llu\n",
                    static_cast<unsigned long long>(saved >= r36s_mips_idle_last
                        ? saved - r36s_mips_idle_last : 0u),
                    static_cast<unsigned long long>(saved),
                    static_cast<unsigned long long>(c.mips_idle_skipped_iterations));
                r36s_mips_idle_last = saved;
            }
            if (dcs && r36s_time_dsp) {
                auto time = dcs->take_timing_sample();
                const double dsp_wall_ms = static_cast<double>(time.host_ns) * 1.0e-6;
                const double per_guest_frame = r36s_sample_frames ?
                    dsp_wall_ms / static_cast<double>(r36s_sample_frames) : 0.0;
                const double dsp_wall_pct = secs > 0 ?
                    (dsp_wall_ms / (secs * 1000.0)) * 100.0 : 0.0;
                std::fprintf(stderr,
                    "[r36s] PHASE dsp_wall=%.1fms dsp_per_frame=%.2fms "
                    "dsp_wall_pct=%.1f%% run_calls=%llu guest_cycles=%llu\n",
                    dsp_wall_ms, per_guest_frame, dsp_wall_pct,
                    static_cast<unsigned long long>(time.calls),
                    static_cast<unsigned long long>(time.guest_cycles));
            }
            if (dcs) {
                const uint64_t accelerated = dcs->waitloop_skipped_cycles();
                std::fprintf(stderr,
                    "[r36s] DSP_FAST skipped=%llu guest_instructions_this_period total=%llu\n",
                    static_cast<unsigned long long>(
                        accelerated >= r36s_waitloop_last ?
                        accelerated - r36s_waitloop_last : 0ull),
                    static_cast<unsigned long long>(accelerated));
                r36s_waitloop_last = accelerated;
                if (std::getenv("KI_R36S_DSP_COMPILED")) {
                    const uint64_t native = dcs->native_opcode_count();
                    std::fprintf(stderr,
                        "[r36s] ADSP_NATIVE ops=%llu total=%llu (current 5s window)\n",
                        static_cast<unsigned long long>(
                            native >= r36s_native_opcode_last ?
                            native - r36s_native_opcode_last : 0ull),
                        static_cast<unsigned long long>(native));
                    r36s_native_opcode_last = native;
                }
            }
            r36s_cpu_begin = cpu_now;
            r36s_read_begin = reads_now;
            r36s_present_frames = 0;
            r36s_render_ms = r36s_rgb_ms = r36s_swap_ms = 0.0;
            r36s_tex_upload_ms = r36s_gpu_copy_ms = 0.0;
            r36s_direct_convert_ms = 0.0;
            r36s_sample_frames = 0;
            r36s_sample_start = stamp;
        }
#endif // !KI_R36S_RELEASE_CLEAN
#endif // KI_R36S
        board.p1 = p1;
        board.p2 = p2;

#ifdef KI_R36S
        const bool frame_turbo = turbo || r36s_boot_blue_fast;
#endif
        if (dcs) {
            std::vector<int16_t>& pcm = dcs->samples();
#ifdef KI_R36S
            if (r36s_dsp_hotspots && frame % 300 == 0)
                dcs->log_pc_hotspots(frame);
#ifndef KI_R36S_RELEASE_CLEAN
            if (r36s_audio_trace) {
                // Hash PCM bytes in little-endian sample order, so the log can
                // be compared at equal guest frame numbers across engines.
                for (int16_t sample : pcm) {
                    const uint16_t bits = static_cast<uint16_t>(sample);
                    r36s_audio_hash = (r36s_audio_hash ^ (bits & 0xffu)) * 1099511628211ull;
                    r36s_audio_hash = (r36s_audio_hash ^ (bits >> 8)) * 1099511628211ull;
                }
                r36s_audio_samples += pcm.size();
                if (frame % 300 == 0) {
                    std::fprintf(stderr,
                        "[r36s] DCS_PCM frame=%llu samples=%llu fnv64=%016llx sample_rate=%.1f\n",
                        static_cast<unsigned long long>(frame),
                        static_cast<unsigned long long>(r36s_audio_samples),
                        static_cast<unsigned long long>(r36s_audio_hash),
                        dcs->sample_rate());
                }
            }
#endif // !KI_R36S_RELEASE_CLEAN
#endif
#ifdef KI_R36S
            if (audio && !frame_turbo && r36s_audio_smooth) {
                const auto now_audio = std::chrono::steady_clock::now();
                const double dt = r36s_audio_clock_valid
                    ? std::chrono::duration<double>(now_audio - r36s_audio_last_vblank).count()
                    : (1.0 / 58.98);
                r36s_audio_last_vblank = now_audio;
                r36s_audio_clock_valid = true;
                const Uint32 before = SDL_GetQueuedAudioSize(audio);
                const auto& stretched = audio_smoother.process(pcm, dt, before);
                constexpr Uint32 max_bytes = ki_r36s::AudioSmooth::kRate * 2u *
                    ki_r36s::AudioSmooth::kMaxQueueMs / 1000u;
                if (!paused && before < max_bytes && !stretched.empty()) {
                    if (SDL_QueueAudio(audio, stretched.data(),
                                       static_cast<Uint32>(stretched.size() * sizeof(int16_t))) != 0)
                        std::fprintf(stderr, "[r36s] AUDIO_SMOOTH SDL_QueueAudio error: %s\n",
                                     SDL_GetError());
                }
                if (!r36s_audio_primed && !paused &&
                    SDL_GetQueuedAudioSize(audio) >=
                        ki_r36s::AudioSmooth::kRate * 2u *
                        ki_r36s::AudioSmooth::kStartQueueMs / 1000u) {
                    SDL_PauseAudioDevice(audio, 0);
                    r36s_audio_primed = true;
                    std::fprintf(stderr,
                        "[r36s] AUDIO_SMOOTH primed: SDL playback active, delay approximately %u ms\n",
                        ki_r36s::AudioSmooth::kStartQueueMs);
                }
            } else
#endif
            {
                // Reference path: exact DSP PCM, no adaptive host stretching.
                // Drop audio in turbo mode, or if the queue exceeds 200 ms.
                if (audio &&
#ifdef KI_R36S
                    !frame_turbo &&
#else
                    !turbo &&
#endif
                    SDL_GetQueuedAudioSize(audio) < 31250 * 2 / 5)
                    SDL_QueueAudio(audio, pcm.data(),
                                   static_cast<Uint32>(pcm.size() * sizeof(int16_t)));
            }
            pcm.clear();
        }
#ifdef KI_R36S
        // Diagnostic-only A/B switch. Normal path renders EVERY game frame.
        if ((!frame_turbo || frame % (r36s_boot_blue_fast ? 4u : 8u) == 0) &&
            frame % r36s_bench_render_every == 0) present(ctx);
#else
        if (!turbo || frame % 8 == 0) present(ctx);
#endif
        if (
#ifdef KI_R36S
            !frame_turbo
#else
            !turbo
#endif
        ) {
            next_frame += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(kFrameSeconds));
            // For the original source PCM, slightly slow when >60ms queued.
            // Adaptive WSOLA intentionally holds ~85ms latency. Its feedback
            // controller handles queue drift; avoid adding 0.5ms/frame
            // slowdown merely because the queue is safely primed.
            if (audio && SDL_GetQueuedAudioSize(audio) >
#ifdef KI_R36S
                (r36s_audio_smooth ? 31250 * 2 * 16 / 100 : 31250 * 2 * 6 / 100)
#else
                31250 * 2 * 6 / 100
#endif
                )
                next_frame += std::chrono::microseconds(500);
            auto now = std::chrono::steady_clock::now();
            if (next_frame > now) std::this_thread::sleep_until(next_frame);
            else if (now - next_frame > std::chrono::milliseconds(100)) next_frame = now;  // fell behind: resync
        }
#ifdef KI_R36S
        else if (r36s_boot_blue_fast) {
            // Do not accumulate a stale pacing deadline while speeding only
            // the early boot curtain; immediately restore normal cadence.
            next_frame = std::chrono::steady_clock::now();
        }
#endif
    };
#ifdef KI_R36S
    if (r36s_host_profile)
        ki_r36s::host_profiler::start();
#endif
    ki::schedule(c);
    bool ok = false;
    for (;;) {
        try {
            ok = ki::run_entry(c, entry);
            break;
        } catch (const ki::LoadStateRequest&) {
            entry = state.restore(c);
            if (audio) SDL_ClearQueuedAudio(audio);
#ifdef KI_R36S
            if (audio && r36s_audio_smooth) {
                SDL_PauseAudioDevice(audio, 1);
                audio_smoother.reset();
                r36s_audio_primed = false;
                r36s_audio_clock_valid = false;
            }
#endif
            next_frame = std::chrono::steady_clock::now();
            SDL_SetWindowTitle(window, "Killer Instinct (recompiled): state loaded");
        }
    }
#ifdef KI_R36S
    if (r36s_host_profile) ki_r36s::host_profiler::stop();
#endif
    save();
    if (!ok)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle,
                                 "The recompiled program stopped on an unexpected guest fault "
                                 "(details on the terminal).",
                                 window);
    SDL_Quit();
    return ok ? 0 : 1;
}
