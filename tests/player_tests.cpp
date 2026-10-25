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
    check(m.sampleCount() == 11, "has 11 samples");
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

    std::printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
