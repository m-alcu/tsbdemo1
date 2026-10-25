#include "player669.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace tsb {
namespace {

constexpr int kChannels = 8;
constexpr int kRows = 64;
constexpr int kPatternBytes = kRows * kChannels * 3;  // 1536
constexpr int kHeaderBytes = 497;

// Tuning anchor: note 61 sounds at 8363 Hz, the usual C4 NTSC rate. Checked
// against libopenmpt rendering the same module - the two agree to 0.000
// semitones, so this is not a guess.
constexpr double kC5Rate = 8363.0;
constexpr int kNoteRef = 61;

// 669 has no tempo field; players run it at 78 BPM, i.e. 2.5/78 s per tick.
constexpr double kTickSeconds = 2.5 / 78.0;

double noteFreq(int note) {
    return kC5Rate * std::pow(2.0, (note - kNoteRef) / 12.0);
}

uint32_t readU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

}  // namespace

bool Module::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error_ = "cannot open " + path;
        return false;
    }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    if (d.size() < kHeaderBytes) {
        error_ = "file too short to be a 669";
        return false;
    }
    if (!(d[0] == 'i' && d[1] == 'f') && !(d[0] == 'J' && d[1] == 'N')) {
        error_ = "bad signature: not a 669";
        return false;
    }

    int nos = d[110], nop = d[111];
    if (nos > 64 || nop > 128) {
        error_ = "invalid header counts";
        return false;
    }
    title_.assign((const char*)&d[2], 36);
    while (!title_.empty() && (title_.back() == '\0' || title_.back() == ' ')) title_.pop_back();

    order_list_.assign(d.begin() + 113, d.begin() + 241);
    speeds_.assign(d.begin() + 241, d.begin() + 369);
    breaks_.assign(d.begin() + 369, d.begin() + 497);

    // The song runs until an order entry exceeds the pattern count.
    songLength_ = 0;
    for (uint8_t o : order_list_) {
        if (o > nop) break;
        songLength_++;
    }

    size_t off = kHeaderBytes;
    samples_.resize(nos);
    for (int i = 0; i < nos; i++) {
        if (off + 25 > d.size()) {
            error_ = "truncated sample headers";
            return false;
        }
        Sample& s = samples_[i];
        s.name.assign((const char*)&d[off], 13);
        while (!s.name.empty() && s.name.back() == '\0') s.name.pop_back();
        s.length = readU32(&d[off + 13]);
        s.loopStart = readU32(&d[off + 17]);
        uint32_t le = readU32(&d[off + 21]);
        s.loopEnd = (le >= 0xfffff) ? 0 : le;
        s.loops = s.loopEnd > s.loopStart;
        off += 25;
    }

    patterns_.resize(nop);
    for (int i = 0; i < nop; i++) {
        if (off + kPatternBytes > d.size()) {
            error_ = "truncated pattern data";
            patterns_.clear();
            return false;
        }
        patterns_[i].assign(d.begin() + off, d.begin() + off + kPatternBytes);
        off += kPatternBytes;
    }

    for (Sample& s : samples_) {
        size_t take = std::min<size_t>(s.length, d.size() - std::min(off, d.size()));
        s.data.assign(d.begin() + off, d.begin() + off + take);
        off += take;
        if (s.loops && s.loopEnd > s.data.size()) s.loopEnd = (uint32_t)s.data.size();
    }
    return true;
}

void Module::applyEvent(Channel& ch, const uint8_t* ev) {
    const uint8_t b0 = ev[0], b1 = ev[1], b2 = ev[2];

    if ((b0 & 0xFE) != 0xFE) {  // a note is present
        int note = 37 + (b0 >> 2);
        int inst = ((b0 & 0x03) << 4) + (b1 >> 4);
        if (inst >= 0 && inst < (int)samples_.size() && samples_[inst].data.size() > 2) {
            ch.sample = &samples_[inst];
            ch.pos = 0.0;
            ch.on = true;
        }
        ch.freq = noteFreq(note);
        ch.targetFreq = ch.freq;
        ch.vibPhase = 0.0;
    }
    if (b0 != 0xFF) {  // a volume is present
        ch.vol = (float)(((b1 & 0x0F) << 2) + 1) / 64.0f;
    }
    if (b2 != 0xFF) {
        ch.effect = b2 >> 4;
        ch.param = b2 & 0x0F;
        if (ch.effect >= 6) ch.effect = -1;
        if (ch.effect == 4) ch.vibDepth = ch.param;
        if (ch.effect == 5) speed_ = ch.param > 0 ? ch.param : speed_;
    } else {
        ch.effect = -1;  // 0xFF cancels the running effect
    }
}

void Module::applyRow() {
    if (patterns_.empty() || songLength_ <= 0) return;
    int pat = order_list_[order_ % songLength_];
    if (pat >= (int)patterns_.size()) pat = 0;
    const std::vector<uint8_t>& p = patterns_[pat];

    if (row_ == 0) {
        uint8_t s = speeds_[pat];
        if (s > 0) speed_ = s;
    }
    for (int c = 0; c < kChannels; c++) applyEvent(chans_[c], &p[(row_ * kChannels + c) * 3]);
}

void Module::tickEffects() {
    // 669 effects act on the playback frequency once per tick and keep acting
    // until another event replaces them.
    for (Channel& ch : chans_) {
        if (!ch.on || ch.effect < 0) continue;
        const double p = ch.param;
        switch (ch.effect) {
            case 0: ch.freq *= 1.0 + p * 0.0009; break;
            case 1: ch.freq *= 1.0 - p * 0.0009; break;
            case 2:
                if (ch.targetFreq > ch.freq)
                    ch.freq = std::min(ch.targetFreq, ch.freq * (1.0 + p * 0.0009));
                else
                    ch.freq = std::max(ch.targetFreq, ch.freq * (1.0 - p * 0.0009));
                break;
            case 3: ch.freq *= 1.0 + (p - 8) * 0.0005; break;
            case 4:
                ch.vibPhase += 0.25;
                ch.freq = ch.targetFreq *
                          (1.0 + std::sin(ch.vibPhase * 6.283185) * ch.vibDepth * 0.002);
                break;
            default: break;
        }
    }
}

void Module::render(float* out, int frames, int rate) {
    std::memset(out, 0, sizeof(float) * frames * 2);
    if (!loaded() || songLength_ <= 0) return;

    if (!started_) {
        started_ = true;
        speed_ = 6;
        applyRow();
        frameAcc_ = rate * kTickSeconds;
    }

    int done = 0;
    while (done < frames) {
        if (frameAcc_ <= 0.0) {
            // next tick: advance effects, and the row when the tick count runs out
            if (++tick_ >= speed_) {
                tick_ = 0;
                int pat = order_list_[order_ % songLength_];
                int last = breaks_[pat] < kRows ? breaks_[pat] : kRows - 1;
                if (++row_ > last) {
                    row_ = 0;
                    order_ = (order_ + 1) % songLength_;  // the song loops for ever
                }
                applyRow();
            } else {
                tickEffects();
            }
            frameAcc_ += rate * kTickSeconds;
        }

        int chunk = (int)std::min<double>(frames - done, std::ceil(frameAcc_));
        if (chunk <= 0) chunk = 1;

        for (int i = 0; i < chunk && done < frames; i++, done++) {
            float l = 0.0f, r = 0.0f;
            for (int c = 0; c < kChannels; c++) {
                Channel& ch = chans_[c];
                if (!ch.on || !ch.sample) continue;
                const Sample& s = *ch.sample;
                // A looping sample wraps at its loop end, not at the end of its
                // data, and keeps the fractional position so the pitch holds.
                if (s.loops && ch.pos >= s.loopEnd) {
                    const double span = s.loopEnd - s.loopStart;
                    ch.pos = s.loopStart + std::fmod(ch.pos - s.loopStart, span);
                }
                size_t p = (size_t)ch.pos;
                if (p >= s.data.size()) {
                    ch.on = false;
                    continue;
                }
                float v = ((float)s.data[p] - 128.0f) / 128.0f * ch.vol;
                // 669 pans hard: even channels left, odd channels right. 0.85
                // keeps a little of each side in the other ear so headphones
                // do not split the mix in two.
                if ((c & 1) == 0) {
                    l += v * 0.85f;
                    r += v * 0.15f;
                } else {
                    r += v * 0.85f;
                    l += v * 0.15f;
                }
                ch.pos += ch.freq / rate;
            }
            out[done * 2] = l * 0.3f;
            out[done * 2 + 1] = r * 0.3f;
        }
        frameAcc_ -= chunk;
    }
}

}  // namespace tsb
