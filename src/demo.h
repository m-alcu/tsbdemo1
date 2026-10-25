// The visual side of the demo. Rather than layering bitmaps, this keeps the
// same video state the 1991 original kept, and runs its routines on it:
//
//   - 256 KB of planar VGA memory (4 planes x 64 KB, "Mode X" layout)
//   - the CRTC start address and line compare registers
//   - the 256-entry DAC with 6-bit components
//
// The original sets maximum scan line to 0, so the screen is 320x400, and
// uses line compare to split it in two: lines 0..LC show memory from the
// start address (the logo, loaded at C180h), lines LC+1..399 show memory
// from address 0 (a second nebula, where the text appears). The logo rises
// out of the middle by moving both registers at once.
//
// Each routine below names the address it was translated from, in segment
// 1030h of the unpacked executable.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tsb {

constexpr int kScreenW = 320;
constexpr int kScreenH = 400;

class Demo {
public:
    bool load(const std::string& assetDir);
    const std::string& error() const { return error_; }

    // Advances the demo clock. Work happens on the same two events the
    // original waited for: the 30 Hz timer tick it programmed into the PIT,
    // and the 70 Hz vertical retrace of the 400-line mode.
    void update(double dt);
    // What the CRT would show now, as 320x400 RGBA8888.
    void renderTo(uint32_t* rgba) const;

private:
    // The star records of 1030:000E, nine bytes each in the original.
    struct Star {
        uint16_t drawnAt = 0;  // where the star was last plotted
        uint8_t drawnPlane = 0;
        uint8_t mask = 0;      // map mask it was plotted with
        uint8_t saved = 0;     // the pixel it covered
        uint16_t pos = 0;      // where it goes next
        uint8_t sub = 0;       // pixel within the 4-pixel byte (= plane)
        uint8_t toRowEnd = 0;  // bytes left before it wraps to the row start
    };
    // The main routine at 13F4 is a straight sequence of waits and effects;
    // each of those becomes a phase here.
    enum class Phase { Intro, Scroll, Settle, FadeIn, Rest, Text, Pulse };

    void onTick();
    void onRetrace();

    void initStars();                  // 0DDB
    void moveStars();                  // 0E41
    bool scrollStep();                 // 0FEA
    void fadeRetrace();                // 11F6
    void pulseRetrace();               // 1390
    void textTick();                   // 125D
    void buildStrip(int line, int row);
    void dissolveStep();               // 12E1, one tick's worth
    void setPhase(Phase p, int ticks = 0);

    uint8_t& vram(int plane, uint16_t addr) { return vram_[plane][addr]; }

    // assets
    uint8_t logo_[320 * 200] = {};
    uint8_t nebula_[320 * 200] = {};
    uint8_t dacTable_[768] = {};
    std::vector<uint8_t> font_;
    uint8_t charmap_[128] = {};
    uint8_t widths_[70] = {};
    uint8_t starSeeds_[800] = {};
    std::vector<std::string> texts_;
    std::string error_;

    // video hardware
    uint8_t vram_[4][65536] = {};
    uint8_t dac_[768] = {};
    uint16_t start_ = 0xB550;
    uint16_t lineCompare_ = 0x8E;
    uint16_t scrollFix_ = 0x0C30;  // 1030:000C, used to re-home stars

    // demo state
    Star stars_[200];
    bool slowStarsTurn_ = false;   // 1030:0716
    Phase phase_ = Phase::Intro;
    int ticksLeft_ = 0;
    bool fadeWaitTick_ = false;
    uint8_t fadeLevel_ = 0x2D;     // 1030:0717
    uint8_t palBuf_[144] = {};     // 1030:0718
    int8_t pulseStep_ = 1;         // 1030:07A8
    uint8_t pulseLevel_ = 1;       // 1030:07A9
    uint16_t pulseHold_ = 0;       // 1030:07AA
    size_t textStep_ = 0;
    int textTick_ = 0;
    uint8_t strip_[320 * 14] = {};  // the scratch strip at 3A6B:0000
    uint8_t stripRow64_[320 * 14] = {};  // 3A6B:D700, nebula rows 64..77
    uint8_t stripRow84_[320 * 14] = {};  // 3A6B:E880, nebula rows 84..97
    uint16_t dissSrc_ = 0, dissDst_ = 0;
    uint8_t dissMask_ = 1;

    double clock_ = 0.0;
    double nextTick_ = 0.0;
    double nextRetrace_ = 0.0;
};

}  // namespace tsb
