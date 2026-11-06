// Synthetic Test21 boot-screen detector checks, no ROM assets required.
#include "ki_r36s_blue_boot_wipe.h"
#include <cstdint>
#include <cstdio>
#include <vector>

static constexpr unsigned W=320u,H=240u;
static void fill(std::vector<uint8_t>& image, uint16_t color, unsigned y0, unsigned y1) {
    for(unsigned y=y0;y<y1;++y) for(unsigned x=0;x<W;++x){
        const size_t o=2u*(y*W+x);
        image[o]=static_cast<uint8_t>(color);
        image[o+1]=static_cast<uint8_t>(color>>8);
    }
}
int main(){
    using ki_r36s::BlueBootWipe;
    std::vector<uint8_t> image(W*H*2u);
    BlueBootWipe wipe;
    if(wipe.update(image.data(),1)) return 1;
    // Gray and bright red must NEVER look like blue.
    fill(image,0x7fff,0,H);
    if(wipe.update(image.data(),2)) return 2;
    fill(image,0x001f,0,H);
    if(wipe.update(image.data(),3)) return 3;
    fill(image,0,0,H);
    // Simulated opaque blue wipe moving from upper rows down.
    fill(image,0x7c00,0,96);
    const auto p=BlueBootWipe::probe(image.data());
    if(p.top_rows!=12 || p.bottom_rows!=0 || p.blue_rows!=12) return 4;
    if(wipe.update(image.data(),4)) return 5;
    if(wipe.update(image.data(),5)) return 6;
    if(!wipe.update(image.data(),6) || !wipe.ever_started()) return 7;
    fill(image,0x7c00,96,208);
    if(!wipe.update(image.data(),7)) return 8;
    fill(image,0x7c00,208,H);
    if(wipe.update(image.data(),8) || !wipe.finished()) return 9;
    // Cannot turn acceleration back on after the intro.
    fill(image,0,0,H);
    fill(image,0x7c00,0,120);
    if(wipe.update(image.data(),9)) return 10;
    // Partial blue marks at the bottom never trigger a downward wipe.
    BlueBootWipe bottom;
    fill(image,0,0,H);
    fill(image,0x7c00,128,H);
    for(unsigned i=0;i<8;++i) if(bottom.update(image.data(),i+1)) return 11;
    // No intro detected by 12 guest seconds must stay inactive forever.
    BlueBootWipe timeout;
    fill(image,0,0,H);
    if(timeout.update(image.data(),721) || !timeout.finished()) return 12;
    fill(image,0x7c00,0,96);
    if(timeout.update(image.data(),722)) return 13;
    std::printf("Blue boot wipe synthetic scene, no false gameplay acceleration: PASS\n");
    return 0;
}
