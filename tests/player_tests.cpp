// Tests for the 669 player. They need assets/music.669, so the path can be
// overridden with TSBDEMO_ASSETS when running from a build directory.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "player669.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

int main() {
    const char* env = std::getenv("TSBDEMO_ASSETS");
    const std::string dir = env ? env : "assets";

    tsb::Module m;
    const bool ok = m.load(dir + "/music.669");
    check(ok, "loads assets/music.669");
    if (!ok) {
        std::printf("     %s\n", m.error().c_str());
        return 1;
    }

    check(m.patternCount() == 50, "has the 50 patterns of one full cycle");
    check(m.sampleCount() == 12, "has 12 samples");
    check(m.songLength() == 50, "order list covers 50 entries");

    // A rejected file must leave the module empty rather than half-loaded.
    tsb::Module bad;
    check(!bad.load(dir + "/background.pal"), "rejects a file that is not a 669");
    check(!bad.loaded(), "a rejected module stays empty");

    // Render a second of audio and make sure it is neither silent nor clipped.
    const int rate = 44100;
    std::vector<float> buf((size_t)rate * 2);
    m.render(buf.data(), rate, rate);
    double sum = 0.0;
    float peak = 0.0f;
    for (float v : buf) {
        sum += (double)v * v;
        peak = std::max(peak, std::fabs(v));
    }
    const double rms = std::sqrt(sum / buf.size());
    std::printf("     rms %.4f peak %.4f\n", rms, peak);
    check(rms > 0.001, "a second of audio is not silence");
    check(peak <= 1.0f, "a second of audio does not clip");

    // A looping sample must wrap at its loop end, never play the bytes after
    // it. One note on a 200-byte sample: loud before the loop, silent inside
    // it (100..150) and loud again after it, so once the note is in the loop
    // the output has to stay at zero.
    {
        std::vector<uint8_t> f(497, 0);
        f[0] = 'i';
        f[1] = 'f';
        f[110] = 1;  // samples
        f[111] = 1;  // patterns
        for (int i = 113; i < 241; i++) f[i] = 0xFF;
        f[113] = 0;
        for (int i = 241; i < 369; i++) f[i] = 4;
        for (int i = 369; i < 497; i++) f[i] = 63;
        const uint32_t desc[3] = {200, 100, 150};
        f.resize(f.size() + 13, 0);
        for (uint32_t v : desc)
            for (int b = 0; b < 4; b++) f.push_back((uint8_t)(v >> (8 * b)));
        std::vector<uint8_t> pat(64 * 8 * 3, 0xFF);
        pat[0] = (61 - 37) << 2;  // note 61, sample 0
        pat[1] = 0x0F;            // volume 15
        f.insert(f.end(), pat.begin(), pat.end());
        for (int i = 0; i < 200; i++) f.push_back(i < 100 ? 192 : i < 150 ? 128 : 255);

        const char* path = "player_tests_loop.669";
        if (std::FILE* out = std::fopen(path, "wb")) {
            std::fwrite(f.data(), 1, f.size(), out);
            std::fclose(out);
        }
        tsb::Module lm;
        check(lm.load(path), "loads a synthetic module with a looping sample");
        std::remove(path);
        lm.render(buf.data(), rate, rate);
        float after = 0.0f;
        for (size_t i = (size_t)rate / 20 * 2; i < buf.size(); i++)
            after = std::max(after, std::fabs(buf[i]));
        check(after == 0.0f, "a looping sample never plays past its loop end");
    }

    std::printf(failures ?"\n%d check(s) failed\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
