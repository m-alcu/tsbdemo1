// The visual side of the demo: a 320x200 indexed framebuffer with the
// original VGA palette, exactly like the 1991 original, converted to RGBA once
// per frame for SDL to upload as a texture.
//
// The original composes three layers, and so does this:
//   1. the nebula, scrolled up from below
//   2. the drifting stars, in front of the nebula
//   3. the logo, in front of the stars, scrolled in step with the nebula
//   4. the text lines, under the logo
// Background and logo are separate bitmaps in the original too: its video
// memory holds a copy of the nebula with no logo on it. They move together,
// but the stars pass between them.
// In the original the scroll is done by reprogramming the VGA start address
// rather than by moving pixels, but the result on screen is the same.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tsb {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;

struct Star {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;  // pixels per second of horizontal drift
    uint8_t colour = 0;
};

class Demo {
public:
    // Loads the bitmap (320x200 palette indices) and its 256-colour palette.
    // Both come straight out of the original's video memory.
    bool load(const std::string& assetDir);
    const std::string& error() const { return error_; }

    void update(double dt);
    // Renders the current frame as RGBA8888, ready for SDL_UpdateTexture.
    void renderTo(uint32_t* rgba) const;

    // Rows the bitmap still has to climb; 0 once it has settled.
    int scrollOffset() const { return (int)scrollY_; }

private:
    void drawStars(uint8_t* buf) const;
    void drawLayer(uint8_t* buf, const uint8_t* src) const;

    uint8_t background_[kScreenW * kScreenH] = {};
    uint8_t logo_[kScreenW * kScreenH] = {};
    uint32_t palette_[256] = {};
    std::vector<Star> stars_;
    std::vector<std::string> texts_;
    std::string error_;
    double time_ = 0.0;
    double scrollY_ = (double)kScreenH;  // starts fully below the screen
};

}  // namespace tsb
