// No original game assets. Validate font coverage, layout and both page renders.
#include "ki_r36s_intro.h"
#include <SDL.h>
#include <cstdint>
#include <cstdio>
#include <string>

int main() {
    using namespace ki_r36s::intro;
    if (!layout_valid()) return 1;
    bool credit=false, legal=false, original=false, derivative=false, optin=false;
    for (const auto& l : kCredits) {
        const std::string s=l.text;
        credit |= s.find("DEHALBUQ") != std::string::npos;
        original |= s.find("KILNAN") != std::string::npos;
    }
    for (const auto& l : kLegal) {
        const std::string s=l.text;
        legal |= s.find("UNOFFICIAL") != std::string::npos;
        derivative |= s.find("COPYRIGHTED") != std::string::npos;
        optin |= s.find("LAWFUL") != std::string::npos;
    }
    if (!(credit && original && legal && derivative && optin)) return 2;
    if (!glyph('0') || !glyph('Z') || glyph('@')) return 3;

    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    if(SDL_Init(SDL_INIT_VIDEO)!=0) {
        std::fprintf(stderr,"SDL headless init: %s\n",SDL_GetError());
        return 4;
    }
    SDL_Surface* canvas=SDL_CreateRGBSurfaceWithFormat(
        0,320,240,32,SDL_PIXELFORMAT_ARGB8888);
    if(!canvas) return 5;
    SDL_Renderer* renderer=SDL_CreateSoftwareRenderer(canvas);
    if(!renderer) return 6;
    for (int page=0;page<2;++page) {
        render_page(renderer,page!=0);
        if(SDL_LockSurface(canvas)!=0) return 7;
        unsigned non_background=0, accent=0;
        for(int y=0;y<240;++y) for(int x=0;x<320;++x){
            uint32_t px=*(reinterpret_cast<uint32_t*>(
                static_cast<uint8_t*>(canvas->pixels)+canvas->pitch*y)+x);
            uint8_t r=0,g=0,b=0;
            SDL_GetRGB(px,canvas->format,&r,&g,&b);
            if(r!=10||g!=14||b!=26) ++non_background;
            if(r==245&&g==195&&b==102) ++accent;
        }
        SDL_UnlockSurface(canvas);
        if(non_background<900 || accent<100) return 8;
    }
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(canvas);
    SDL_Quit();
    std::puts("R36S splash credits/legal pages: glyphs, text and software renders PASS");
    return 0;
}
