// Android 4.4+ SDL2 physical gamepad selection and raw HID fallback.
// Chanuo A1 (Mediatek) exposes gpio_ir_recv and virtual-search BEFORE
// the Bluetooth iPega PG-9068 in SDL's joystick enumeration.
// This module is Android-only; R36S and desktop input are unchanged.
#pragma once

#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include "ki_pad.h"

namespace ki_android {

inline std::string lower_name(const char* name) {
    std::string out = name ? name : "";
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

inline bool is_virtual_input(const std::string& name) {
    // Known Mediatek IR / Android synthetic inputs confirmed on Chanuo A1.
    static constexpr const char* excluded[] = {
        "gpio_ir_recv", "virtual-search", "android accelerometer",
        "lm_misc", "accdet", "mtk-kpd", "remote control", "ir receiver"
    };
    for (const char* token : excluded)
        if (name.find(token) != std::string::npos) return true;
    return false;
}

inline bool is_ipega(const std::string& name) {
    return name.find("pg-9068") != std::string::npos;
}

inline void add_chanuo_mapping() {
    // Verified Chanuo A1 HID GUID: 6473:1026. D-pad: physical buttons 11-14.
    // Register before opening controllers. Fallback below still reads raw HID.
    const int result = SDL_GameControllerAddMapping(
        "050013054919000002040000ffff3f00,PG-9068,"
        "a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,leftstick:b7,rightstick:b8,"
        "leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,"
        "dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,"
        "lefttrigger:a4,righttrigger:a5,platform:Android,");
    SDL_Log("[ki-android-pad] PG-9068 controller mapping result=%d", result);
}

struct Candidate {
    int index = -1;
    SDL_JoystickID id = -1;
    int score = -1;
    bool ipega = false;
    std::string name;
};

inline std::vector<Candidate> physical_candidates() {
    std::vector<Candidate> out;
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        const std::string name = lower_name(SDL_JoystickNameForIndex(i));
        if (name.empty() || is_virtual_input(name)) continue;
        SDL_Joystick* joy = SDL_JoystickOpen(i);
        if (!joy) continue;
        const int axes = SDL_JoystickNumAxes(joy);
        const int buttons = SDL_JoystickNumButtons(joy);
        const int hats = SDL_JoystickNumHats(joy);
        const SDL_JoystickID id = SDL_JoystickInstanceID(joy);
        SDL_JoystickClose(joy);
        // IR remotes sometimes claim SDL_GameController compatibility;
        // use physical controls instead of SDL_IsGameController alone.
        if (id < 0 || buttons < 4 || (axes < 2 && hats == 0 && buttons < 8))
            continue;
        Candidate c;
        c.index = i;
        c.id = id;
        c.name = name;
        c.ipega = is_ipega(name);
        c.score = (axes >= 2 ? 100 : 0) + (axes >= 4 ? 20 : 0)
                + (buttons >= 4 ? 50 : 0) + (buttons >= 8 ? 10 : 0)
                + (hats > 0 ? 10 : 0) + (SDL_IsGameController(i) ? 20 : 0)
                + (SDL_JoystickGetDeviceVendor(i) && SDL_JoystickGetDeviceProduct(i) ? 10 : 0)
                + (c.ipega ? 100 : 0);
        out.push_back(c);
    }
    std::stable_sort(out.begin(), out.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });
    if (out.size() > 2) out.resize(2);
    return out;
}

inline bool down(SDL_Joystick* joy, int index) {
    return joy && index < SDL_JoystickNumButtons(joy)
        && SDL_JoystickGetButton(joy, index) != 0;
}

inline uint32_t raw_pressed(SDL_Joystick* joy, bool ipega) {
    if (!joy || !SDL_JoystickGetAttached(joy)) return 0;
    uint32_t bits = 0;
    // KI arcade P1 active-low bit layout:
    // high punches 001/002/004, low kicks 008/010/020;
    // D-pad 040/080/100/200, START 400, COIN 800.
    if (down(joy, 2)) bits |= 0x001;
    if (down(joy, 3)) bits |= 0x002;
    if (down(joy, ipega ? 9 : 4)) bits |= 0x004;
    if (down(joy, 0)) bits |= 0x008;
    if (down(joy, 1)) bits |= 0x010;
    if (down(joy, ipega ? 10 : 5)) bits |= 0x020;
    if (down(joy, ipega ? 6 : 7)) bits |= 0x400;
    if (down(joy, ipega ? 4 : 6)) bits |= 0x800;

    if (ipega) {
        if (down(joy, 11)) bits |= 0x040;
        if (down(joy, 12)) bits |= 0x080;
        if (down(joy, 13)) bits |= 0x100;
        if (down(joy, 14)) bits |= 0x200;
    }
    if (SDL_JoystickNumHats(joy) > 0) {
        const Uint8 hat = SDL_JoystickGetHat(joy, 0);
        if (hat & SDL_HAT_UP) bits |= 0x040;
        if (hat & SDL_HAT_DOWN) bits |= 0x080;
        if (hat & SDL_HAT_LEFT) bits |= 0x100;
        if (hat & SDL_HAT_RIGHT) bits |= 0x200;
    }
    if (SDL_JoystickNumAxes(joy) >= 2) {
        constexpr Sint16 deadzone = 14000;
        const Sint16 x = SDL_JoystickGetAxis(joy, 0);
        const Sint16 y = SDL_JoystickGetAxis(joy, 1);
        if (x < -deadzone) bits |= 0x100;
        if (x > deadzone) bits |= 0x200;
        if (y < -deadzone) bits |= 0x040;
        if (y > deadzone) bits |= 0x080;
    }
    return bits;
}

class PhysicalGamepads {
    struct Entry {
        SDL_GameController* controller = nullptr;
        SDL_Joystick* raw = nullptr; // owned only if controller == nullptr
        SDL_JoystickID id = -1;
        bool ipega = false;
    };
    std::vector<Entry> entries_;
    Uint32 last_scan_ = 0;
    bool dirty_ = true;

    void close_all() {
        for (Entry& entry : entries_) {
            if (entry.controller) SDL_GameControllerClose(entry.controller);
            else if (entry.raw) SDL_JoystickClose(entry.raw);
        }
        entries_.clear();
    }
public:
    ~PhysicalGamepads() { close_all(); }
    void on_device_change() { dirty_ = true; }

    void poll(std::vector<SDL_GameController*>& for_setup) {
        bool disconnected = false;
        for (const Entry& e : entries_)
            if (!e.raw || !SDL_JoystickGetAttached(e.raw)) disconnected = true;
        const Uint32 now = SDL_GetTicks();
        if (!dirty_ && !disconnected && !(entries_.empty() && now - last_scan_ > 1700u))
            return;
        dirty_ = false;
        last_scan_ = now;
        const auto selected = physical_candidates();
        bool unchanged = !disconnected && selected.size() == entries_.size();
        if (unchanged)
            for (size_t i = 0; i < selected.size(); ++i)
                if (selected[i].id != entries_[i].id) unchanged = false;
        if (unchanged) return;

        close_all();
        for_setup.clear();
        for (const Candidate& c : selected) {
            Entry e;
            if (SDL_IsGameController(c.index))
                e.controller = SDL_GameControllerOpen(c.index);
            e.raw = e.controller ? SDL_GameControllerGetJoystick(e.controller)
                                 : SDL_JoystickOpen(c.index);
            if (!e.raw) {
                if (e.controller) SDL_GameControllerClose(e.controller);
                continue;
            }
            e.id = SDL_JoystickInstanceID(e.raw);
            e.ipega = c.ipega;
            entries_.push_back(e);
            if (e.controller) for_setup.push_back(e.controller);
            SDL_Log("[ki-android-pad] player=%u name=%s id=%d score=%d axes=%d buttons=%d mapped=%d raw_ipega=%d",
                    static_cast<unsigned>(entries_.size()), c.name.c_str(),
                    static_cast<int>(e.id), c.score,
                    SDL_JoystickNumAxes(e.raw), SDL_JoystickNumButtons(e.raw),
                    e.controller != nullptr, e.ipega);
        }
        if (entries_.empty())
            SDL_Log("[ki-android-pad] no physical pad found; IR/virtual inputs excluded");
    }

    uint32_t pressed(size_t player, const std::vector<ki::pad::Action>& bindings) const {
        if (player >= entries_.size()) return 0;
        const Entry& e = entries_[player];
        if (!e.raw || !SDL_JoystickGetAttached(e.raw)) return 0;
        // Chanuo's PG-9068 presents as a Keyboard/Controller hybrid:
        // raw continuous polling is required even if SDL claims a mapping.
        if (e.ipega || !e.controller) return raw_pressed(e.raw, e.ipega);
        return ki::pad::pressed(e.controller, bindings);
    }
};

} // namespace ki_android
