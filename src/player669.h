// Composer 669 playback. No SDL here: the module mixes into a float buffer
// and main.cpp hands that to the audio stream, which keeps this file testable
// without a sound device.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tsb {

struct Sample {
    std::string name;
    uint32_t length = 0;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;  // 0 means "no loop"; 669 stores >= 0xfffff for that
    bool loops = false;
    std::vector<uint8_t> data;  // 8-bit unsigned, as stored in the file
};

class Module {
public:
    // Reads a .669 from disk. Returns false (and leaves the module empty) if
    // the file is missing or its header does not check out.
    bool load(const std::string& path);
    bool loaded() const { return !patterns_.empty(); }

    const std::string& title() const { return title_; }
    int patternCount() const { return (int)patterns_.size(); }
    int sampleCount() const { return (int)samples_.size(); }
    int songLength() const { return songLength_; }
    const std::string& error() const { return error_; }

    // Mixes `frames` stereo frames at `rate` Hz into `out` (interleaved,
    // -1..1). The song repeats for ever, which is what the original does.
    void render(float* out, int frames, int rate);

    // Playback position, for anything that wants to follow the music.
    int currentOrder() const { return order_; }
    int currentRow() const { return row_; }

private:
    struct Channel {
        const Sample* sample = nullptr;
        double pos = 0.0;
        double freq = 0.0;
        double targetFreq = 0.0;
        float vol = 0.0f;
        bool on = false;
        int effect = -1;
        int param = 0;
        double vibPhase = 0.0;
        int vibDepth = 0;
    };

    void applyRow();
    void applyEvent(Channel& ch, const uint8_t* ev);
    void tickEffects();

    std::string title_;
    std::string error_;
    std::vector<Sample> samples_;
    std::vector<std::vector<uint8_t>> patterns_;  // 1536 bytes each
    std::vector<uint8_t> order_list_;
    std::vector<uint8_t> speeds_;
    std::vector<uint8_t> breaks_;
    int songLength_ = 0;

    Channel chans_[8];
    int order_ = 0;
    int row_ = 0;
    int tick_ = 0;
    int speed_ = 6;
    double frameAcc_ = 0.0;  // output frames still owed to the current tick
    bool started_ = false;
};

}  // namespace tsb
