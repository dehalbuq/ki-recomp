// Tiny SDL2-only exit menu for R36S. No fonts, assets or SDL_ttf required.
#pragma once
#include <SDL.h>
#include <cstring>

namespace ki_r36s {
inline const char* glyph(char c) {
    switch(c) {
      case 'A': return "011101000110001111111000110001";
      case 'B': return "11110100011000111110100011000111110";
      case 'C': return "01111100001000010000100001000001111";
      case 'D': return "11110100011000110001100011000111110";
      case 'E': return "11111100001000011110100001000011111";
      case 'F': return "11111100001000011110100001000010000";
      case 'I': return "11111001000010000100001000010011111";
      case 'K': return "10001100101010011000101001001010001";
      case 'M': return "10001110111010110101100011000110001";
      case 'N': return "10001110011010110011100011000110001";
      case 'O': return "01110100011000110001100011000101110";
      case 'P': return "11110100011000111110100001000010000";
      case 'R': return "11110100011000111110101001001010001";
      case 'S': return "01111100001000001110000010000111110";
      case 'T': return "11111001000010000100001000010000100";
      case 'U': return "10001100011000110001100011000101110";
      case 'V': return "1000110001100011000110001010100100";
      case 'X': return "10001100010101000100010101000110001";
      case ':': return "00000001000010000000001000010000000";
      case '+': return "00000001000010011111001000010000000";
      default:  return nullptr;
    }
}
inline void text(SDL_Renderer* r, int x, int y, const char* str, int scale) {
    for (const char* c=str; *c; ++c,x+=6*scale) {
        const char* bits=glyph(*c);
        if (!bits) continue;
        if (std::strlen(bits) < 35) continue;
        for (int row=0; row<7; ++row)
            for (int col=0; col<5; ++col) {
                if (bits[row*5+col]!='1') continue;
                SDL_Rect pt{x+col*scale,y+row*scale,scale,scale};
                SDL_RenderFillRect(r,&pt);
            }
    }
}
inline void draw(SDL_Renderer* r, int choice) {
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,0,0,0,210);
    SDL_Rect shade{38,52,244,143};
    SDL_RenderFillRect(r,&shade);
    SDL_SetRenderDrawColor(r,220,220,220,255);
    SDL_RenderDrawRect(r,&shade);
    text(r,104,63,"PAUSED",3);
    for(int i=0;i<2;++i) {
        SDL_Rect field{77,104+i*34,166,30};
        if(choice==i) {
            SDL_SetRenderDrawColor(r,245,245,245,255);
            SDL_RenderFillRect(r,&field);
            SDL_SetRenderDrawColor(r,0,0,0,255);
        } else {
            SDL_SetRenderDrawColor(r,220,220,220,255);
        }
        text(r,i==0?91:119,112+i*34,i==0?"RESUME":"EXIT",2);
    }
    SDL_SetRenderDrawColor(r,230,230,230,255);
    text(r,83,176,"A:OK B:BACK",1);
}
} // namespace ki_r36s
