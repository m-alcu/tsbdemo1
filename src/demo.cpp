#include "demo.h"

#include <cstring>
#include <fstream>
#include <random>

namespace tsb {
namespace {

// How long the logo takes to climb into place. Measured from the original:
// at 8 s it is still some 40 rows low, and by 60 s it has settled.
constexpr double kScrollSeconds = 28.0;

bool readExact(const std::string& path, void* dst, size_t n) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read((char*)dst, (std::streamsize)n);
    return f.gcount() == (std::streamsize)n;
}

}  // namespace

bool Demo::load(const std::string& assetDir) {
    const std::string idx = assetDir + "/background.idx";
    const std::string pal = assetDir + "/background.pal";
    const std::string lg = assetDir + "/logo.idx";

    if (!readExact(idx, background_, sizeof(background_))) {
        error_ = "cannot read " + idx;
        return false;
    }
    if (!readExact(lg, logo_, sizeof(logo_))) {
        error_ = "cannot read " + lg;
        return false;
    }
    uint8_t rgb[768];
    if (!readExact(pal, rgb, sizeof(rgb))) {
        error_ = "cannot read " + pal;
        return false;
    }
    for (int i = 0; i < 256; i++) {
        palette_[i] = 0xFF000000u | ((uint32_t)rgb[i * 3] << 16) |
                      ((uint32_t)rgb[i * 3 + 1] << 8) | (uint32_t)rgb[i * 3 + 2];
    }

    // The text lines recovered from the original, one per line.
    std::ifstream tf(assetDir + "/texts.txt");
    for (std::string line; std::getline(tf, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) texts_.push_back(line);
    }

    // The stars are drawn by code in the original, not stored as data, so the
    // look is matched rather than the exact positions: single pixels, three
    // parallax speeds, the brightest entries of the palette.
    std::mt19937 rng(1991);
    std::uniform_real_distribution<float> fx(0.0f, (float)kScreenW);
    std::uniform_real_distribution<float> fy(0.0f, (float)kScreenH);
    std::uniform_int_distribution<int> layer(0, 2);
    stars_.resize(220);
    for (Star& s : stars_) {
        s.x = fx(rng);
        s.y = fy(rng);
        const int l = layer(rng);
        s.speed = 1.5f + 2.0f * l;
        s.colour = (uint8_t)(l == 2 ? 255 : (l == 1 ? 254 : 253));
    }
    return true;
}

void Demo::update(double dt) {
    time_ += dt;
    if (scrollY_ > 0.0) {
        scrollY_ -= (double)kScreenH * dt / kScrollSeconds;
        if (scrollY_ < 0.0) scrollY_ = 0.0;
    }
    for (Star& s : stars_) {
        s.x -= s.speed * (float)dt * 10.0f;
        if (s.x < 0.0f) s.x += (float)kScreenW;
    }
}

void Demo::drawStars(uint8_t* buf) const {
    for (const Star& s : stars_) {
        const int x = (int)s.x, y = (int)s.y;
        if (x >= 0 && x < kScreenW && y >= 0 && y < kScreenH) buf[y * kScreenW + x] = s.colour;
    }
}

// Blits one scrolled layer, treating colour 0 as transparent. Both layers
// share the same offset, so they climb into place together.
void Demo::drawLayer(uint8_t* buf, const uint8_t* src) const {
    const int off = (int)scrollY_;
    for (int y = 0; y < kScreenH; y++) {
        const int sy = y - off;
        if (sy < 0 || sy >= kScreenH) continue;
        const uint8_t* s = &src[sy * kScreenW];
        uint8_t* d = &buf[y * kScreenW];
        for (int x = 0; x < kScreenW; x++)
            if (s[x]) d[x] = s[x];
    }
}

void Demo::renderTo(uint32_t* rgba) const {
    uint8_t buf[kScreenW * kScreenH];
    std::memset(buf, 0, sizeof(buf));  // black sky
    drawLayer(buf, background_);       // nebula behind everything
    drawStars(buf);                    // stars in front of the nebula
    drawLayer(buf, logo_);             // logo in front of the stars
    for (int i = 0; i < kScreenW * kScreenH; i++) rgba[i] = palette_[buf[i]];
}

}  // namespace tsb
