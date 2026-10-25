#include "demo.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace tsb {
namespace {

// The PIT divisor 9B5Ch gives 30.0006 Hz; the 400-line VGA mode refreshes at
// 25.175 MHz / (800 x 449) = 70.086 Hz.
constexpr double kTick = 39772.0 / 1193182.0;
constexpr double kRetrace = 800.0 * 449.0 / 25175000.0;

constexpr uint16_t kLogoStart = 0xC180;  // where the logo sits in VRAM
constexpr uint16_t kFinalSplit = 200;    // line compare once the logo is up
constexpr int kFontPitch = 1050;         // 70 glyphs x 15 columns
constexpr int kGlyphRows = 14;

// The text sequence of 1030:0B85. Each step dissolves a line of text (or,
// for kBlank, plain nebula) into nebula row 64 or 84 of the lower screen;
// kPause waits 65 ticks. Lines index assets/texts.txt.
constexpr int kBlank = -1;
constexpr int kPause = -2;
struct TextStep {
    int line;
    int row;
};
constexpr TextStep kScript[] = {
    {0, 64},      {kPause, 0},
    {kBlank, 64}, {1, 64},      {kPause, 0},
    {kBlank, 64}, {2, 64},      {3, 84},      {kPause, 0},
    {kBlank, 64}, {kBlank, 84}, {4, 64},      {5, 84},      {kPause, 0},
    {kBlank, 64}, {kBlank, 84}, {6, 64},      {7, 84},      {kPause, 0},
    {kBlank, 64}, {kBlank, 84}, {8, 64},      {kPause, 0},
    {kBlank, 64}, {9, 64},      {kPause, 0},
    {kBlank, 64}, {10, 64},     {kPause, 0},
    {kBlank, 64}, {11, 64},     {12, 84},     {kPause, 0},
    {kBlank, 64}, {kBlank, 84}, {13, 64},     {kPause, 0},
    {kBlank, 64}, {14, 64},     {15, 84},     {kPause, 0},
    {kBlank, 64}, {kBlank, 84}, {16, 64},     {kPause, 0},
    {kBlank, 64},
};
constexpr size_t kScriptLen = sizeof(kScript) / sizeof(kScript[0]);

bool readExact(const std::string& path, void* dst, size_t n) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read((char*)dst, (std::streamsize)n);
    return f.gcount() == (std::streamsize)n;
}

}  // namespace

bool Demo::load(const std::string& assetDir) {
    font_.resize((size_t)kFontPitch * kGlyphRows);
    const struct {
        const char* name;
        void* dst;
        size_t n;
    } files[] = {
        {"logo.idx", logo_, sizeof(logo_)},
        {"nebula.idx", nebula_, sizeof(nebula_)},
        {"palette.dac", dacTable_, sizeof(dacTable_)},
        {"font.idx", font_.data(), font_.size()},
        {"charmap.bin", charmap_, sizeof(charmap_)},
        {"stars.bin", starSeeds_, sizeof(starSeeds_)},
    };
    for (const auto& f : files) {
        if (!readExact(assetDir + "/" + f.name, f.dst, f.n)) {
            error_ = "cannot read " + assetDir + "/" + f.name;
            return false;
        }
    }
    std::ifstream tf(assetDir + "/texts.txt");
    for (std::string line; std::getline(tf, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) texts_.push_back(line);
    }
    if (texts_.size() < 17) {
        error_ = "texts.txt should hold 17 lines";
        return false;
    }

    // 105D: every glyph is 15 columns in the sheet; its width is that minus
    // the empty columns on its right. Glyph 0 is left untrimmed, as there.
    widths_[0] = 15;
    for (int g = 1; g < 70; g++) {
        int empty = 0;
        for (int col = g * 15 + 14; col >= 0; col--, empty++) {
            bool any = false;
            for (int r = 0; r < kGlyphRows; r++) any |= font_[r * kFontPitch + col] != 0;
            if (any) break;
        }
        widths_[g] = (uint8_t)(15 - empty);
    }

    // D30: logo into VRAM at C180h, nebula at 0, planar layout. Only the logo
    // colours are lit; everything below 224 starts black.
    for (int i = 0; i < 320 * 200; i++) {
        const int plane = i & 3;
        vram(plane, (uint16_t)(kLogoStart + (i >> 2))) = logo_[i];
        vram(plane, (uint16_t)(i >> 2)) = nebula_[i];
    }
    std::memcpy(&dac_[224 * 3], &dacTable_[224 * 3], 32 * 3);
    std::memcpy(stripRow64_, &nebula_[64 * 320], sizeof(stripRow64_));
    std::memcpy(stripRow84_, &nebula_[84 * 320], sizeof(stripRow84_));

    initStars();
    setPhase(Phase::Intro, 200);
    nextTick_ = kTick;
    nextRetrace_ = kRetrace;
    return true;
}

void Demo::setPhase(Phase p, int ticks) {
    phase_ = p;
    ticksLeft_ = ticks;
}

void Demo::update(double dt) {
    clock_ += std::min(dt, 0.25);  // don't replay seconds after a stall
    for (;;) {
        if (nextTick_ <= nextRetrace_ && nextTick_ <= clock_) {
            onTick();
            nextTick_ += kTick;
        } else if (nextRetrace_ <= clock_) {
            onRetrace();
            nextRetrace_ += kRetrace;
        } else {
            break;
        }
    }
}

// What the main routine at 13F4 does on each timer tick. The stars move on
// every tick of every phase.
void Demo::onTick() {
    switch (phase_) {
        case Phase::Intro:  // 1194, 200 ticks of stars alone
        case Phase::Settle:
        case Phase::Rest:
            moveStars();
            if (--ticksLeft_ == 0) {
                if (phase_ == Phase::Intro) setPhase(Phase::Scroll);
                else if (phase_ == Phase::Settle) {
                    setPhase(Phase::FadeIn);
                    fadeWaitTick_ = false;
                } else {
                    setPhase(Phase::Text);
                    textStep_ = 0;
                    textTick_ = 0;
                }
            }
            break;
        case Phase::Scroll: {  // 11B2
            const bool moved = scrollStep();
            moveStars();
            if (!moved) setPhase(Phase::Settle, 100);
            break;
        }
        case Phase::FadeIn:  // 11D3: one palette step per tick
            moveStars();
            fadeWaitTick_ = false;
            break;
        case Phase::Text:
            textTick();
            break;
        case Phase::Pulse:  // 135B
            moveStars();
            if (pulseHold_ >= 1) pulseHold_--;
            break;
    }
}

void Demo::onRetrace() {
    if (phase_ == Phase::FadeIn && !fadeWaitTick_) fadeRetrace();
    else if (phase_ == Phase::Pulse && pulseHold_ == 0) pulseRetrace();
}

// 0DDB. Each seed is a word and two bytes. The word picks the plane and one
// of 400 rows; the bytes pick the column. The rows are numbered so that, with
// the registers as they are at start-up, the stars cover the whole screen:
// rows 0..256 fall in the lower part (VRAM from 0) and the rest in the upper
// part, 10 bytes to the right, for reasons only Tran knew.
void Demo::initStars() {
    const uint8_t* p = starSeeds_;
    for (Star& s : stars_) {
        const uint16_t w = (uint16_t)(p[0] | p[1] << 8);
        const uint8_t plane = w & 3;
        s.sub = plane;
        s.drawnPlane = plane;
        s.mask = (uint8_t)(1 << plane);
        uint16_t row = w % 400, bias = 0;
        if (row >= 0x101) {
            bias = 10;
            row += 0x142;
        }
        const uint8_t col = (uint8_t)((p[2] ^ p[3]) % 80);
        s.pos = (uint16_t)(row * 80 + bias + col);
        s.toRowEnd = (uint8_t)(80 - col);
        p += 4;
    }
}

// 0E41. Four groups, each its own colour and speed, all moving right:
//   80 stars, colour FBh, 1 pixel every other tick
//   60 stars, colour FCh, 1 pixel per tick
//   40 stars, colour FDh, 2 pixels per tick
//   20 stars, colour FEh, 4 pixels (one VRAM byte) per tick
// A star is plotted straight into VRAM, and only over pixels below E0h, so it
// passes behind the logo and the text. The pixel it covered is put back
// before it moves, but only if the star is still there.
void Demo::moveStars() {
    auto restore = [this](Star& s, uint8_t colour) {
        if (vram(s.drawnPlane, s.drawnAt) != colour) return;
        for (int p = 0; p < 4; p++)
            if (s.mask & (1 << p)) vram(p, s.drawnAt) = s.saved;
    };
    auto advanceByte = [](Star& s, uint16_t& at) {
        at++;
        if (--s.toRowEnd == 0) {
            at -= 0x50;
            s.toRowEnd = 0x50;
        }
    };
    auto plot = [this](Star& s) {
        s.saved = vram(s.drawnPlane, s.drawnAt);
        return s.saved < 0xE0;
    };

    struct Group {
        int first, count;
        uint8_t colour, step;
    };
    slowStarsTurn_ = !slowStarsTurn_;
    const Group groups[] = {{0, 80, 0xFB, 1}, {80, 60, 0xFC, 1}, {140, 40, 0xFD, 2}};
    for (const Group& g : groups) {
        if (g.colour == 0xFB && !slowStarsTurn_) continue;
        for (int i = g.first; i < g.first + g.count; i++) {
            Star& s = stars_[i];
            restore(s, g.colour);
            uint8_t sub = (uint8_t)(s.sub + g.step);
            uint16_t at = s.pos;
            if (sub >= 4) {  // into the next byte, at plane 0
                advanceByte(s, at);
                sub = 0;
            }
            s.sub = sub;
            s.pos = s.drawnAt = at;
            s.drawnPlane = sub;
            if (plot(s)) {
                s.mask = (uint8_t)(1 << sub);
                vram(sub, at) = g.colour;
            }
        }
    }
    for (int i = 180; i < 200; i++) {  // the fast group keeps its plane
        Star& s = stars_[i];
        restore(s, 0xFE);
        uint16_t at = s.pos;
        advanceByte(s, at);
        s.pos = s.drawnAt = at;
        if (plot(s))
            for (int p = 0; p < 4; p++)
                if (s.mask & (1 << p)) vram(p, at) = 0xFE;
    }
}

// 0FEA, once per tick. The start address walks down one row (B550h to C180h,
// 39 ticks) so the logo rises, and line compare walks down from 142 to 200
// (58 ticks) so the window it rises into opens from the middle. Star
// positions are re-homed so the stars stay put on screen. Returns false once
// both have arrived.
bool Demo::scrollStep() {
    int moved = 0;
    if (start_ != kLogoStart) {
        start_ += 0x50;
        moved++;
        scrollFix_ -= 0x50;
        for (Star& s : stars_)
            if (s.pos >= 0x8000) s.pos += 0x50;
    }
    if (lineCompare_ != kFinalSplit) {
        moved++;
        lineCompare_++;
        scrollFix_ += 0x50;
        for (Star& s : stars_) {
            if (s.pos > 0x8000) continue;
            const uint16_t old = s.pos;
            s.pos -= 0x50;
            if (old < 0x50) s.pos -= scrollFix_;
        }
    }
    return moved != 0;
}

// 11F6. At a retrace, show the buffered colours 16..63, then work out the
// next step: the full nebula palette minus a level that drops from 45 by one
// a tick. When it is done the buffer is primed for the pulse.
void Demo::fadeRetrace() {
    for (int i = 0; i < 144; i++) dac_[16 * 3 + i] = palBuf_[i] & 0x3F;
    const uint8_t level = fadeLevel_;
    for (int i = 0; i < 144; i++) {
        const uint8_t v = dacTable_[16 * 3 + i];
        palBuf_[i] = v >= level ? (uint8_t)(v - level) : 0;
    }
    fadeLevel_--;
    if (fadeLevel_ == 0xFE) {
        std::memcpy(palBuf_, &dacTable_[40 * 3], 24 * 3);
        setPhase(Phase::Rest, 100);
        return;
    }
    fadeWaitTick_ = true;
}

// 1390. At each retrace, show the buffered colours 40..63, then add a level
// to their red component: up from 1 to 62, a hold of 192 ticks, and back
// down. The DAC keeps only 6 bits, so red wraps around rather than
// saturating, and the colours cycle through the nebula.
void Demo::pulseRetrace() {
    for (int i = 0; i < 72; i++) dac_[40 * 3 + i] = palBuf_[i] & 0x3F;
    uint8_t level = pulseLevel_;
    for (int i = 0; i < 24; i++) {
        palBuf_[i * 3] = (uint8_t)(dacTable_[(40 + i) * 3] + level);
        palBuf_[i * 3 + 1] = dacTable_[(40 + i) * 3 + 1];
        palBuf_[i * 3 + 2] = dacTable_[(40 + i) * 3 + 2];
    }
    level = (uint8_t)(level + pulseStep_);
    if (pulseStep_ == 1) {
        if (level >= 0x3F) {
            pulseStep_ = -1;
            pulseHold_ = 0xC0;
        }
    } else if (level == 0xFF) {
        pulseStep_ = 1;
        pulseLevel_ = 1;
        setPhase(Phase::Rest, 100);
        return;
    }
    pulseLevel_ = level;
}

// 125D. Each step takes 15 ticks: on the first the strip is prepared, on
// the other 14 a fourteenth of it is dissolved into VRAM.
void Demo::textTick() {
    const TextStep& step = kScript[textStep_];
    moveStars();
    if (step.line == kPause) {
        if (++textTick_ < 65) return;
    } else if (textTick_ == 0) {
        buildStrip(step.line, step.row);
        textTick_++;
        return;
    } else {
        dissolveStep();
        if (++textTick_ < 15) return;
    }
    textTick_ = 0;
    if (++textStep_ == kScriptLen) setPhase(Phase::Pulse);  // 125D returns into 135B
}

// 1093 and 12B3: a text line is rendered once over the nebula rows it will
// sit on and pasted into a copy of those rows. Every line in the original is
// drawn over the same rows it is shown on, so drawing the glyphs straight
// onto the copy gives identical bytes.
void Demo::buildStrip(int line, int row) {
    std::memcpy(strip_, row == 64 ? stripRow64_ : stripRow84_, sizeof(strip_));
    dissSrc_ = 0;
    dissDst_ = (uint16_t)(row * 80);
    dissMask_ = 1;
    if (line < 0) return;

    const std::string& text = texts_[(size_t)line];
    auto glyph = [this](char c) { return charmap_[(uint8_t)c & 0x7F]; };
    int width = 0;
    for (char c : text) width += c == ' ' ? 8 : widths_[glyph(c)] + 1;
    int x = (320 - width) >> 1;
    for (char c : text) {
        if (c == ' ') {
            x += 8;
            continue;
        }
        const int g = glyph(c), w = widths_[g];
        for (int r = 0; r < kGlyphRows; r++)
            for (int col = 0; col < w; col++) {
                const uint8_t p = font_[r * kFontPitch + g * 15 + col];
                if (p && x + col < 320) strip_[r * 320 + x + col] = p;
            }
        x += w + 1;
    }
}

// 12E1. 320 pixels per tick, visited with a stride of 241: prime to the
// 4480 pixels of the strip, so in 14 ticks every one is written exactly once,
// in an order that looks random.
void Demo::dissolveStep() {
    for (int n = 0; n < 320; n++) {
        const int plane = dissMask_ == 1 ? 0 : dissMask_ == 2 ? 1 : dissMask_ == 4 ? 2 : 3;
        vram(plane, dissDst_) = strip_[dissSrc_];
        dissSrc_++;
        const int carry = dissMask_ >= 8;
        dissMask_ = (uint8_t)(((dissMask_ << 1) | carry) & 0x0F);
        dissDst_ += (uint16_t)(0x3C + carry);
        dissSrc_ += 0xF0;
        if (dissSrc_ >= 0x1180) {
            dissSrc_ -= 0x1180;
            dissDst_ -= 0x460;
        }
    }
}

// The CRT: lines up to line compare read from the start address, the lines
// after it from address 0. 80 bytes per line, pixel x in plane x & 3.
void Demo::renderTo(uint32_t* rgba) const {
    uint32_t rgb[256];
    for (int i = 0; i < 256; i++) {
        auto c = [&](int k) { return (uint32_t)((dac_[i * 3 + k] * 255 + 31) / 63); };
        rgb[i] = 0xFF000000u | c(0) << 16 | c(1) << 8 | c(2);
    }
    for (int y = 0; y < kScreenH; y++) {
        const uint16_t base = y <= lineCompare_ ? (uint16_t)(start_ + y * 80)
                                                : (uint16_t)((y - lineCompare_ - 1) * 80);
        uint32_t* out = &rgba[y * kScreenW];
        for (int x = 0; x < kScreenW; x++)
            out[x] = rgb[vram_[x & 3][(uint16_t)(base + (x >> 2))]];
    }
}

}  // namespace tsb
