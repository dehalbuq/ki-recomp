// Killer Instinct R36S - offline SDL2 credits and legal notice.
//
// This runs only once before reading the user's game files. It allocates no
// textures, uses no SDL_ttf/images, and performs no work during emulation.
// Font is a small original 5x7 bitmap; UI is rendered at logical 320x240
// and scaled exactly 2x to the R36S 640x480 panel.
// A/START may advance after the minimum reading time. SDL_QUIT aborts.
#pragma once
#include <SDL.h>
#include <cstddef>
#include <cstring>

namespace ki_r36s::intro {

struct Line { const char* text; int y; int scale; int color; };
inline constexpr Line kCredits[] = {
    {"KILLER INSTINCT",22,2,1},
    {"R36S ARM64 PORT",48,1,0},
    {"A COMMUNITY RECOMPILATION PROJECT",63,1,0},
    {"PORTED AND OPTIMIZED BY",84,1,0},
    {"DEHALBUQ",99,2,1},
    {"BASED ON KI-RECOMP BY KILNAN",129,1,0},
    {"ORIGINAL ARCADE: RARE / MIDWAY",145,1,0},
    {"ADSP CORE: MAME / AARON GILES",161,1,0},
    {"SDL2 / LIBCHDR / MINIZ / RABBITIZER",177,1,0},
    {"CORTEX-A35 + OPENGL ES2",193,1,0},
    {"A / START: CONTINUE",222,1,2},
};
inline constexpr Line kLegal[] = {
    {"LEGAL NOTICE",21,2,1},
    {"UNOFFICIAL AND INDEPENDENT FAN PORT",51,1,0},
    {"NO AFFILIATION OR ENDORSEMENT",67,1,0},
    {"BY THE RIGHTS HOLDERS.",81,1,0},
    {"NO ORIGINAL ROM OR CHD IS INCLUDED.",101,1,1},
    {"STATICALLY RECOMPILED CODE MAY",117,1,0},
    {"CONTAIN COPYRIGHTED GAME-DERIVED",131,1,0},
    {"MATERIAL. REDISTRIBUTION MAY",145,1,0},
    {"REQUIRE AUTHORIZATION.",159,1,0},
    {"USE YOUR OWN LAWFUL GAME FILES.",180,1,1},
    {"SOURCE: GITHUB.COM/DEHALBUQ/KI-RECOMP",199,1,0},
    {"A / START: CONTINUE",222,1,2},
};
constexpr size_t credits_count() { return sizeof(kCredits)/sizeof(kCredits[0]); }
constexpr size_t legal_count() { return sizeof(kLegal)/sizeof(kLegal[0]); }
inline const char* glyph(char c) {
    switch(c) {
        case '0': return "01110100111010110101110011000101110";
        case '1': return "00100011000010000100001000010001110";
        case '2': return "01110100010000100010001000100011111";
        case '3': return "11110000010000101110000010000111110";
        case '4': return "00010001100101010010111110001000010";
        case '5': return "11111100001000011110000010000111110";
        case '6': return "01110100001000011110100011000101110";
        case '7': return "11111000010001000100010000100001000";
        case '8': return "01110100011000101110100011000101110";
        case '9': return "01110100011000101111000010000101110";
        case 'A': return "01110100011000111111100011000110001";
        case 'B': return "11110100011000111110100011000111110";
        case 'C': return "01111100001000010000100001000001111";
        case 'D': return "11110100011000110001100011000111110";
        case 'E': return "11111100001000011110100001000011111";
        case 'F': return "11111100001000011110100001000010000";
        case 'G': return "01111100001000010111100011000101110";
        case 'H': return "10001100011000111111100011000110001";
        case 'I': return "01110001000010000100001000010001110";
        case 'J': return "00111000100001000010100101001001100";
        case 'K': return "10001100101010011000101001001010001";
        case 'L': return "10000100001000010000100001000011111";
        case 'M': return "10001110111010110101100011000110001";
        case 'N': return "10001110011010110011100011000110001";
        case 'O': return "01110100011000110001100011000101110";
        case 'P': return "11110100011000111110100001000010000";
        case 'Q': return "01110100011000110001101011001001101";
        case 'R': return "11110100011000111110101001001010001";
        case 'S': return "01111100001000001110000010000111110";
        case 'T': return "11111001000010000100001000010000100";
        case 'U': return "10001100011000110001100011000101110";
        case 'V': return "10001100011000110001100010101000100";
        case 'W': return "10001100011000110101101011010101010";
        case 'X': return "10001100010101000100010101000110001";
        case 'Y': return "10001100010101000100001000010000100";
        case 'Z': return "11111000010001000100010001000011111";
        case '.': return "00000000000000000000000000010000100";
        case ':': return "00000001000010000000001000010000000";
        case '-': return "00000000000000011111000000000000000";
        case '/': return "00001000010001000100010001000010000";
        case '+': return "00000001000010011111001000010000000";
        case '!': return "00100001000010000100001000000000100";
        case '\'': return "00100001000000000000000000000000000";
        case '(': return "00010001000100001000010000010000010";
        case ')': return "01000001000001000010000100010001000";
        case '&': return "01100100101010001000101011001001101";
        case '=': return "00000111110000011111000000000000000";
        case '?': return "01110100010000100010001000000000100";
        case ' ': return "00000000000000000000000000000000000";
        default: return nullptr;
    }
}
inline int text_width(const char* s, int scale) {
    return static_cast<int>(std::strlen(s)) * 6 * scale;
}
inline bool layout_valid() {
    for (const auto& list : {0,1}) {
        const Line* lines = list == 0 ? kCredits : kLegal;
        const size_t count = list == 0 ? credits_count() : legal_count();
        for (size_t i = 0; i<count; ++i) {
            const Line& l=lines[i];
            if (l.y < 0 || l.y+7*l.scale >= 240 || text_width(l.text,l.scale)>320)
                return false;
            for (const char* p=l.text; *p; ++p) if (!glyph(*p)) return false;
        }
    }
    return true;
}
inline void paint_text(SDL_Renderer* renderer, const char* s, int y, int scale) {
    const int left=(320-text_width(s,scale))/2;
    for (size_t i=0; s[i]; ++i) {
        const char* bitmap=glyph(s[i]);
        if (!bitmap) continue;
        for (int row=0; row<7; ++row)
            for (int col=0; col<5; ++col)
                if (bitmap[row*5+col]=='1') {
                    const SDL_Rect r{left+static_cast<int>(i)*6*scale+col*scale,
                                     y+row*scale,scale,scale};
                    SDL_RenderFillRect(renderer,&r);
                }
    }
}
inline void render_page(SDL_Renderer* renderer, bool legal) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer,10,14,26,255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer,44,144,194,255);
    const SDL_Rect accent_top{12,11,296,3};
    const SDL_Rect accent_bottom{12,212,296,1};
    SDL_RenderFillRect(renderer,&accent_top);
    SDL_RenderFillRect(renderer,&accent_bottom);
    const Line* lines=legal?kLegal:kCredits;
    const size_t count=legal?legal_count():credits_count();
    for (size_t i=0;i<count;++i) {
        const Line& l=lines[i];
        switch(l.color) {
            case 1: SDL_SetRenderDrawColor(renderer,245,195,102,255); break;
            case 2: SDL_SetRenderDrawColor(renderer,124,196,219,255); break;
            default: SDL_SetRenderDrawColor(renderer,230,232,234,255); break;
        }
        paint_text(renderer,l.text,l.y,l.scale);
    }
    SDL_RenderPresent(renderer);
}
inline bool is_skip(const SDL_Event& e) {
    if (e.type==SDL_KEYDOWN && !e.key.repeat) {
        const SDL_Keycode k=e.key.keysym.sym;
        return k==SDLK_RETURN || k==SDLK_SPACE || k==SDLK_ESCAPE ||
               k==SDLK_a || k==SDLK_KP_ENTER;
    }
    if (e.type==SDL_CONTROLLERBUTTONDOWN) {
        return e.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
               e.cbutton.button == SDL_CONTROLLER_BUTTON_START ||
               e.cbutton.button == SDL_CONTROLLER_BUTTON_B;
    }
    if (e.type==SDL_JOYBUTTONDOWN) {
        // PortMaster/dArkOSRE internal raw buttons; A and Start are
        // common choices, but all press-to-continue buttons are accepted.
        return true;
    }
    return false;
}
inline bool show(SDL_Renderer* renderer) {
    constexpr Uint32 kCreditMs=1800u, kLegalMs=3900u, kMinReadMs=700u;
    if (!layout_valid()) return true; // never block original game
    for (int screen=0; screen<2; ++screen) {
        render_page(renderer,screen==1);
        const Uint32 started=SDL_GetTicks();
        const Uint32 duration=screen==0?kCreditMs:kLegalMs;
        while (SDL_GetTicks()-started < duration) {
            SDL_Event e{};
            while (SDL_PollEvent(&e)) {
                if (e.type==SDL_QUIT) return false;
                if (SDL_GetTicks()-started>=kMinReadMs && is_skip(e)) goto next_screen;
            }
            SDL_Delay(16);
        }
    next_screen:;
    }
    return true;
}
} // namespace ki_r36s::intro
