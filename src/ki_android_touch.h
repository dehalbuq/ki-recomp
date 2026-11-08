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
    bool overlay_visible_ = false;
    bool portrait_ = false;
    SDL_FPoint stick_center_{};
    float stick_radius_ = 0.0f;
    SDL_FingerID stick_finger_ = 0;
    bool stick_active_ = false;
    SDL_FPoint stick_position_{};
    int width_ = 0, height_ = 0;
    struct Finger { SDL_FingerID id; float x; float y; };
    std::vector<Finger> fingers_;
    std::array<TouchButton, 12> buttons_{};  // 4 directions, 6 attacks, coin, start
    SDL_FRect exit_rect_{};
    bool exit_pressed_ = false;
    // The exit touch control only requests confirmation, never a forced quit.

    static float clampf(float v, float a, float b) {
        return std::max(a, std::min(v, b));
    }
    static bool inside(const SDL_FRect& a, float x, float y) {
        return x >= a.x && y >= a.y && x < a.x+a.w && y < a.y+a.h;
    }
    void reset() {
        fingers_.clear();
        exit_pressed_ = false;
        stick_active_ = false;
        stick_finger_ = 0;
        stick_position_ = stick_center_;
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
            stick_center_ = {w * 0.235f, top + area * 0.59f};
            stick_radius_ = std::min(w * 0.16f, area * 0.255f);
            stick_position_ = stick_center_;
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
            stick_center_ = {rail * 0.51f, h * 0.55f};
            stick_radius_ = std::min(rail * 0.38f, h * 0.24f);
            stick_position_ = stick_center_;
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
    // One-thumb 8-way virtual arcade stick with a small center deadzone.
    // Diagonals work through natural stick displacement, not 2 fingers.
    uint32_t stick_bits() const {
        if (!stick_active_ || !overlay_visible_) return 0;
        const float dx = stick_position_.x - stick_center_.x;
        const float dy = stick_position_.y - stick_center_.y;
        const float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < stick_radius_ * 0.21f) return 0;
        const float threshold = stick_radius_ * 0.22f;
        uint32_t bits = 0;
        if (dx < -threshold) bits |= 0x100;
        if (dx >  threshold) bits |= 0x200;
        if (dy < -threshold) bits |= 0x040;
        if (dy >  threshold) bits |= 0x080;
        return bits;
    }
    uint32_t attack_bits(float x, float y) const {
        uint32_t bits = 0;
        for (size_t i=4;i<buttons_.size();++i) {
            const auto& b=buttons_[i];
            const float dx=x-b.cx,dy=y-b.cy;
            if(dx*dx+dy*dy <= b.r*b.r*1.35f) bits |= b.bits;
        }
        return bits;
    }
    static void disc(SDL_Renderer* r, float x, float y, float radius,
                     Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
        // One GLES2-compatible draw call per circle, no textures/assets.
        constexpr int segments = 40;
        SDL_Vertex vertices[segments+1]{};
        int triangles[segments*3]{};
        const SDL_Color color{red,green,blue,alpha};
        vertices[0].position = {x,y};
        vertices[0].color = color;
        for(int i=0;i<segments;++i) {
            const float angle = static_cast<float>(i) * 6.28318530718f / segments;
            vertices[i+1].position = {x + radius*std::cos(angle),
                                       y + radius*std::sin(angle)};
            vertices[i+1].color = color;
            triangles[i*3]=0;
            triangles[i*3+1]=i+1;
            triangles[i*3+2]=((i+1)%segments)+1;
        }
        SDL_RenderGeometry(r, nullptr, vertices, segments+1, triangles, segments*3);
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
    explicit TouchControls(bool enabled=false)
        :enabled_(enabled),overlay_visible_(enabled){}
    bool enabled() const {return enabled_;}
    bool overlay_visible() const {return enabled_ && overlay_visible_;}
    void set_gamepad_present(bool present) {
        const bool visible = enabled_ && !present;
        if (visible == overlay_visible_) return;
        overlay_visible_ = visible;
        reset(); // No stuck touch attacks when a gamepad appears.
        SDL_Log("[ki-android-touch] overlay=%s (physical gamepad=%d)",
                visible ? "visible" : "hidden", present ? 1 : 0);
    }
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
        const bool show=overlay_visible();
        const float available_w=(portrait || !show)?static_cast<float>(w):w*0.606f;
        const float available_h=(portrait && show)?h*0.595f:static_cast<float>(h);
        int integer=static_cast<int>(std::floor(std::min(available_w/320.0f,available_h/240.0f)));
        const float sc=integer>=1?static_cast<float>(integer):
            std::min(available_w/320.0f,available_h/240.0f);
        const float gw=320.0f*sc,gh=240.0f*sc;
        const float ycenter=(portrait && show)?available_h*0.5f:h*0.5f;
        return {(w-gw)*0.5f,ycenter-gh*0.5f,gw,gh};
    }
    void process(const SDL_Event& e, SDL_Renderer* renderer) {
        if(!enabled_)return;
        if(e.type==SDL_APP_DIDENTERBACKGROUND||e.type==SDL_APP_WILLENTERBACKGROUND) {
            reset();
            return;
        }
        if (!overlay_visible_) return;
        if(e.type!=SDL_FINGERDOWN&&e.type!=SDL_FINGERMOTION&&e.type!=SDL_FINGERUP) return;
        update(renderer);
        if(width_<=0||height_<=0)return;
        const float x=clampf(e.tfinger.x,0,1)*width_;
        const float y=clampf(e.tfinger.y,0,1)*height_;
        auto it=std::find_if(fingers_.begin(),fingers_.end(),[&](const Finger& f){return f.id==e.tfinger.fingerId;});
        if(e.type==SDL_FINGERUP) {
            if(stick_active_ && e.tfinger.fingerId==stick_finger_) {
                stick_active_=false;
                stick_finger_=0;
                stick_position_=stick_center_;
            }
            if(it!=fingers_.end())fingers_.erase(it);
            return;
        }
        if(it!=fingers_.end()) {it->x=x;it->y=y;}
        else if(fingers_.size()<12) fingers_.push_back({e.tfinger.fingerId,x,y});
        const float sx=x-stick_center_.x,sy=y-stick_center_.y;
        if(e.type==SDL_FINGERDOWN && !stick_active_ &&
           sx*sx+sy*sy <= stick_radius_*stick_radius_*2.25f) {
            stick_active_=true;
            stick_finger_=e.tfinger.fingerId;
        }
        if(stick_active_ && e.tfinger.fingerId==stick_finger_) {
            const float len=std::sqrt(sx*sx+sy*sy);
            const float maximum=stick_radius_*0.80f;
            const float ratio=(len>maximum && len>0.001f)?maximum/len:1.0f;
            stick_position_={stick_center_.x+sx*ratio,stick_center_.y+sy*ratio};
        }
        if(e.type==SDL_FINGERDOWN&&inside(exit_rect_,x,y))exit_pressed_=true;
    }
    uint32_t pressed() const {
        if(!overlay_visible())return 0;
        uint32_t bits=stick_bits();
        for(const auto& f:fingers_)
            if(!stick_active_ || f.id!=stick_finger_)
                bits|=attack_bits(f.x,f.y);
        return bits;
    }
    bool take_exit_request() {
        const bool hit=exit_pressed_;
        exit_pressed_=false;
        return hit;
    }
    void draw(SDL_Renderer* r,bool confirm_exit=false) {
        if(!overlay_visible())return;
        update(r);
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        // Arcade stick inspired by a real cabinet: dark base, red ball-top,
        // small offset knob that follows a single thumb.
        disc(r,stick_center_.x,stick_center_.y,stick_radius_,38,40,48,190);
        disc(r,stick_center_.x,stick_center_.y,stick_radius_*0.91f,176,179,186,185);
        disc(r,stick_center_.x,stick_center_.y,stick_radius_*0.84f,19,21,29,210);
        const float knob_r=stick_radius_*0.48f;
        disc(r,stick_position_.x,stick_position_.y,knob_r*1.14f,24,20,24,235);
        disc(r,stick_position_.x,stick_position_.y,knob_r,202,31,32,252);
        disc(r,stick_position_.x-knob_r*0.22f,stick_position_.y-knob_r*0.26f,
             knob_r*0.28f,255,177,169,100);
        // Six cabinet attacks in classic two rows of three blue buttons.
        const uint32_t down=pressed();
        for(size_t i=4;i<buttons_.size();++i) {
            const auto& b=buttons_[i];
            const bool active=(down&b.bits)!=0;
            disc(r,b.cx,b.cy,b.r*1.05f,238,233,227,240);
            disc(r,b.cx,b.cy,b.r*0.83f,active?58:26,active?128:69,
                 active?255:205,245);
            disc(r,b.cx-b.r*0.20f,b.cy-b.r*0.22f,b.r*0.19f,
                 177,203,255,active?75:115);
            SDL_SetRenderDrawColor(r,252,247,244,242);
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
