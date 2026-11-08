// Killer Instinct Android: adaptive virtual arcade controls (SDL2/GLES2).
// Works on touchscreen Android 4.4+; Bluetooth/USB and TV remote are independent.
// All overlays are drawn by SDL: no external fonts, no proprietary resources.
#pragma once

#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ki_android {

struct TouchButton {
    float cx = 0, cy = 0, r = 0;
    uint32_t bits = 0;         // active-low arcade P1 bit to press
    const char* label = "";
};

class TouchControls {
    bool enabled_ = false;
    bool portrait_ = false;
    int width_ = 0, height_ = 0;
    struct Finger { SDL_FingerID id; float x; float y; };
    std::vector<Finger> fingers_;
    std::array<TouchButton, 12> buttons_{};  // 4 directions, 6 attacks, coin, start
    SDL_FRect exit_rect_{};
    bool exit_pressed_ = false;
    Uint32 exit_request_ms_ = 0;

    static float clampf(float v, float a, float b) {
        return std::max(a, std::min(v, b));
    }
    static bool inside(const SDL_FRect& a, float x, float y) {
        return x >= a.x && y >= a.y && x < a.x+a.w && y < a.y+a.h;
    }
    void reset() {
        fingers_.clear();
        exit_pressed_ = false;
    }
    void button(int i, float cx, float cy, float r, uint32_t bits, const char* label) {
        buttons_[i] = {cx, cy, r, bits, label};
    }
    void place(int w, int h) {
        if (!enabled_ || w <= 0 || h <= 0) return;
        if (w == width_ && h == height_) return;
        width_ = w; height_ = h;
        reset(); // On rotation do not keep finger contacts in old positions.
        portrait_ = h > w;
        if (portrait_) {
            // In portrait, top ~61% holds the centered 4:3 game,
            // bottom holds the virtual arcade D-pad and six attacks.
            const float top = h * 0.615f, area = h - top;
            const float dpad_cx = w * 0.235f, dpad_cy = top + area * 0.60f;
            const float step = std::min(w * 0.112f, area * 0.218f);
            const float dr = step * 0.55f;
            button(0,dpad_cx, dpad_cy-step,dr,0x040,"U");
            button(1,dpad_cx, dpad_cy+step,dr,0x080,"D");
            button(2,dpad_cx-step,dpad_cy,dr,0x100,"L");
            button(3,dpad_cx+step,dpad_cy,dr,0x200,"R");
            const float attack_cx = w * 0.605f, attack_step = w * 0.125f;
            const float attack_r = std::min(attack_step * 0.48f, area * 0.078f);
            const float top_row = top + area * 0.43f;
            const float bottom_row = top + area * 0.72f;
            for(int j=0;j<3;++j) {
                button(4+j,attack_cx + attack_step*j,top_row,attack_r,1u<<j,
                       j==0?"H1":j==1?"H2":"H3");
                button(7+j,attack_cx + attack_step*j,bottom_row,attack_r,1u<<(j+3),
                       j==0?"L1":j==1?"L2":"L3");
            }
            button(10,w*0.40f,top + area*0.16f,std::min(w*0.065f,area*0.072f),0x800,"COIN");
            button(11,w*0.57f,top + area*0.16f,std::min(w*0.065f,area*0.072f),0x400,"START");
            exit_rect_={w*0.79f,top+area*0.04f,w*0.185f,area*0.15f};
        } else {
            // In landscape, reserve physical side rails for controls;
            // game itself uses the 4:3 region between them.
            const float rail = w * 0.197f;
            const float dpad_x = rail * 0.49f, dpad_y = h * 0.56f;
            const float step = std::min(rail * 0.245f, h * 0.17f);
            const float dr = step * 0.56f;
            button(0,dpad_x,dpad_y-step,dr,0x040,"U");
            button(1,dpad_x,dpad_y+step,dr,0x080,"D");
            button(2,dpad_x-step,dpad_y,dr,0x100,"L");
            button(3,dpad_x+step,dpad_y,dr,0x200,"R");
            const float attack_step = rail * 0.285f;
            const float attack_x = w-rail*0.80f;
            const float attack_r = std::min(attack_step * 0.45f,h * 0.092f);
            for(int j=0;j<3;++j) {
                button(4+j,attack_x+j*attack_step,h*0.40f,attack_r,1u<<j,
                       j==0?"H1":j==1?"H2":"H3");
                button(7+j,attack_x+j*attack_step,h*0.61f,attack_r,1u<<(j+3),
                       j==0?"L1":j==1?"L2":"L3");
            }
            button(10,rail*0.34f,h*0.89f,std::min(rail*0.15f,h*0.066f),0x800,"COIN");
            button(11,rail*0.74f,h*0.89f,std::min(rail*0.15f,h*0.066f),0x400,"START");
            exit_rect_={w-rail*0.63f,h*0.81f,rail*0.55f,h*0.10f};
        }
        SDL_Log("[ki-android-touch] orientation=%s output=%dx%d controls=%d",
                portrait_?"portrait":"landscape", w,h,enabled_?1:0);
    }
    uint32_t pressed_for(float x, float y) const {
        uint32_t bits=0;
        // D-pad supports diagonals with ONE thumb (critical for KI specials).
        // The central deadzone is small; sliding between quadrants changes
        // directional bits without releasing the SDL finger contact.
        const float center_x=buttons_[0].cx;
        const float center_y=buttons_[2].cy;
        const float step=std::max(1.0f,(buttons_[1].cy-buttons_[0].cy)*0.5f);
        const float dx=x-center_x,dy=y-center_y;
        if (std::fabs(dx)<=step*1.60f && std::fabs(dy)<=step*1.60f) {
            const float threshold=step*0.25f;
            if(dx<-threshold)bits|=0x100;
            if(dx>threshold)bits|=0x200;
            if(dy<-threshold)bits|=0x040;
            if(dy>threshold)bits|=0x080;
        }
        for (size_t i=4;i<buttons_.size();++i) {
            const auto& b=buttons_[i];
            const float ax=x-b.cx,ay=y-b.cy;
            // Slightly bigger hit areas prevent missed taps.
            if (ax*ax+ay*ay <= b.r*b.r*1.35f) bits |= b.bits;
        }
        return bits;
    }
    static const char* glyph(char c) {
        // Compact original 3x5 pixel font for touch controls and exit prompt.
        switch(c) {
            case 'A':return "010101111101101";
            case 'B':return "110101110101110";
            case 'C':return "011100100100011";
            case 'D':return "110101101101110";
            case 'E':return "111100110100111";
            case 'F':return "111100110100100";
            case 'G':return "011100101101011";
            case 'H':return "101101111101101";
            case 'I':return "111010010010111";
            case 'J':return "001001001101010";
            case 'K':return "101101110101101";
            case 'L':return "100100100100111";
            case 'M':return "101111111101101";
            case 'N':return "101111111111101";
            case 'O':return "010101101101010";
            case 'P':return "110101110100100";
            case 'Q':return "010101101111011";
            case 'R':return "110101110101101";
            case 'S':return "011100010001110";
            case 'T':return "111010010010010";
            case 'U':return "101101101101111";
            case 'V':return "101101101101010";
            case 'X':return "101101010101101";
            case 'Y':return "101101010010010";
            case 'Z':return "111001010100111";
            case '1':return "010110010010111";
            case '2':return "110001010100111";
            case '3':return "110001010001110";
            case '?':return "110001010000010";
            case ' ':return "000000000000000";
            default:return "000000000000000";
        }
    }
    static void label(SDL_Renderer* renderer,const char* txt,float cx,float cy,float scale) {
        int count=0;for(const char* c=txt;*c;++c)++count;
        const float x0=cx-count*2.0f*scale;
        const float y0=cy-2.5f*scale;
        for(int ch=0;ch<count;++ch) {
            const char* g=glyph(txt[ch]);
            for(int y=0;y<5;++y)for(int x=0;x<3;++x)
                if(g[y*3+x]=='1') {
                    const SDL_FRect px{x0+ch*4.0f*scale+x*scale,
                                       y0+y*scale,scale,scale};
                    SDL_RenderFillRectF(renderer,&px);
                }
        }
    }
public:
    explicit TouchControls(bool enabled=false):enabled_(enabled){}
    bool enabled() const {return enabled_;}
    void update(SDL_Renderer* renderer) {
        if (!enabled_) return;
        int w=0,h=0;
        if(SDL_GetRendererOutputSize(renderer,&w,&h)==0)place(w,h);
    }
    SDL_FRect game_rect(SDL_Renderer* renderer) {
        int w=0,h=0;
        if(SDL_GetRendererOutputSize(renderer,&w,&h)!=0||w<=0||h<=0)
            return {0,0,320,240};
        update(renderer);
        const bool portrait=h>w;
        const float available_w=portrait?static_cast<float>(w):w*0.606f;
        const float available_h=portrait?h*0.595f:static_cast<float>(h);
        int integer=static_cast<int>(std::floor(std::min(available_w/320.0f,available_h/240.0f)));
        const float sc=integer>=1?static_cast<float>(integer):
            std::min(available_w/320.0f,available_h/240.0f);
        const float gw=320.0f*sc,gh=240.0f*sc;
        const float ycenter=portrait?available_h*0.5f:h*0.5f;
        return {(w-gw)*0.5f,ycenter-gh*0.5f,gw,gh};
    }
    void process(const SDL_Event& e, SDL_Renderer* renderer) {
        if(!enabled_)return;
        if(e.type==SDL_APP_DIDENTERBACKGROUND||e.type==SDL_APP_WILLENTERBACKGROUND) {
            reset();
            return;
        }
        if(e.type!=SDL_FINGERDOWN&&e.type!=SDL_FINGERMOTION&&e.type!=SDL_FINGERUP) return;
        update(renderer);
        if(width_<=0||height_<=0)return;
        const float x=clampf(e.tfinger.x,0,1)*width_;
        const float y=clampf(e.tfinger.y,0,1)*height_;
        auto it=std::find_if(fingers_.begin(),fingers_.end(),[&](const Finger& f){return f.id==e.tfinger.fingerId;});
        if(e.type==SDL_FINGERUP) {
            if(it!=fingers_.end())fingers_.erase(it);
            return;
        }
        if(it!=fingers_.end()) {it->x=x;it->y=y;}
        else if(fingers_.size()<12) fingers_.push_back({e.tfinger.fingerId,x,y});
        if(e.type==SDL_FINGERDOWN&&inside(exit_rect_,x,y))exit_pressed_=true;
    }
    uint32_t pressed() const {
        if(!enabled_)return 0;
        uint32_t bits=0;
        for(const auto& f:fingers_)bits|=pressed_for(f.x,f.y);
        return bits;
    }
    bool take_exit_request() {
        const bool hit=exit_pressed_;
        exit_pressed_=false;
        return hit;
    }
    void draw(SDL_Renderer* r,bool confirm_exit=false) {
        if(!enabled_)return;
        update(r);
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        const uint32_t down=pressed();
        for(const auto& b:buttons_) {
            const bool active=(down&b.bits)!=0;
            const SDL_FRect outer{b.cx-b.r,b.cy-b.r,2*b.r,2*b.r};
            SDL_SetRenderDrawColor(r,active?228:68,active?81:85,active?46:114,active?210:132);
            SDL_RenderFillRectF(r,&outer);
            SDL_SetRenderDrawColor(r,248,241,230,220);
            SDL_RenderDrawRectF(r,&outer);
            const float sc=clampf(b.r/(b.label[1]?3.5f:2.2f),2,8);
            label(r,b.label,b.cx,b.cy,sc);
        }
        SDL_SetRenderDrawColor(r,120,43,48,164);
        SDL_RenderFillRectF(r,&exit_rect_);
        SDL_SetRenderDrawColor(r,248,241,230,220);
        SDL_RenderDrawRectF(r,&exit_rect_);
        label(r,"EXIT",exit_rect_.x+exit_rect_.w*0.5f,
              exit_rect_.y+exit_rect_.h*0.5f,
              clampf(exit_rect_.h/10.0f,2,7));
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        (void)confirm_exit;
    }
    void draw_exit_confirmation(SDL_Renderer* r) {
        int w=0,h=0;if(SDL_GetRendererOutputSize(r,&w,&h)!=0)return;
        const float pw=std::min(w*0.80f,720.0f);
        const float ph=std::min(h*0.30f,220.0f);
        const SDL_FRect panel{(w-pw)*0.5f,(h-ph)*0.5f,pw,ph};
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r,15,12,18,225);SDL_RenderFillRectF(r,&panel);
        SDL_SetRenderDrawColor(r,252,184,132,255);SDL_RenderDrawRectF(r,&panel);
        SDL_SetRenderDrawColor(r,255,239,213,255);
        label(r,"SAIR?",w*0.5f,panel.y+ph*0.33f,
              clampf(std::min(pw/35.0f,ph/17.0f),3,12));
        label(r,"BACK OU EXIT NOVAMENTE",w*0.5f,panel.y+ph*0.69f,
              clampf(std::min(pw/92.0f,ph/28.0f),1,7));
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    }
};

} // namespace ki_android
