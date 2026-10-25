// tsbdemo - a C++17/SDL3 replica of "The Sound Barrier" demo 1,
// programmed by Tran of Renaissance, 31 December 1991.
//
// Layout of the sources:
//   demo.h/.cpp       - the original's VGA state and the routines that drive it
//   player669.h/.cpp  - Composer 669 playback (no SDL, unit-testable)
//   main.cpp          - SDL init, audio stream and the frame loop
//
// The graphics in assets/ were extracted from the original executable by
// tools/extract_assets.py; the music in assets/music.669 was reconstructed
// from the original player. See ../rea/tsbdemo1-analisis/README.md for how.
#include <SDL3/SDL.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "demo.h"
#include "player669.h"

namespace {

constexpr int kAudioRate = 44100;

// SDL asks for more audio whenever the device runs low; the module renders
// straight into the stream so there is no buffer of our own to keep in sync.
void SDLCALL feedAudio(void* userdata, SDL_AudioStream* stream, int additional, int) {
    if (additional <= 0) return;
    auto* mod = (tsb::Module*)userdata;
    const int frames = additional / (int)(sizeof(float) * 2);
    if (frames <= 0) return;
    static std::vector<float> buf;
    buf.resize((size_t)frames * 2);
    mod->render(buf.data(), frames, kAudioRate);
    SDL_PutAudioStreamData(stream, buf.data(), (int)(buf.size() * sizeof(float)));
}

// Finding assets/ must not depend on the working directory: VS Code and the
// CMake Tools extension launch the binary from build/, where "assets" does not
// resolve, and the demo would exit before opening a window.
std::string assetDir() {
    std::vector<std::string> candidates;
    if (const char* d = SDL_getenv("TSBDEMO_ASSETS")) candidates.push_back(d);
    candidates.push_back("assets");
    candidates.push_back("../assets");
    if (const char* base = SDL_GetBasePath()) {
        candidates.push_back(std::string(base) + "assets");
        candidates.push_back(std::string(base) + "../assets");
    }
    for (const std::string& c : candidates) {
        SDL_IOStream* io = SDL_IOFromFile((c + "/logo.bmp").c_str(), "rb");
        if (io) {
            SDL_CloseIO(io);
            return c;
        }
    }
    return "assets";  // let the caller report the failure against the usual name
}

// A GUI launch has nowhere to show stderr, so a fatal error gets a dialog as
// well as the log line.
void fatal(const std::string& msg) {
    SDL_Log("%s", msg.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "tsbdemo", msg.c_str(), nullptr);
}

}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const std::string assets = assetDir();
    tsb::Demo demo;
    if (!demo.load(assets)) {
        fatal("Cannot load the assets: " + demo.error() +
              "\nRun from the project root, or set TSBDEMO_ASSETS to the assets folder.");
        return 1;
    }

    tsb::Module music;
    if (!music.load(assets + "/music.669")) {
        SDL_Log("music: %s (continuing without sound)", music.error().c_str());
    }

    // The original runs a 320x400 mode on a 4:3 monitor, so its pixels were
    // far from square. 1280x960 with STRETCH reproduces that.
    SDL_Window* win = SDL_CreateWindow("The Sound Barrier - Renaissance 1991", 1280, 960,
                                       SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!win || !ren) {
        fatal(std::string("window/renderer failed: ") + SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);
    SDL_SetRenderLogicalPresentation(ren, tsb::kScreenW, tsb::kScreenH,
                                     SDL_LOGICAL_PRESENTATION_STRETCH);

    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, tsb::kScreenW,
                                         tsb::kScreenH);
    // Nearest-neighbour: chunky pixels, as on the original hardware.
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    SDL_AudioStream* audio = nullptr;
    if (music.loaded()) {
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_F32;
        spec.channels = 2;
        spec.freq = kAudioRate;
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feedAudio,
                                          &music);
        if (audio)
            SDL_ResumeAudioStreamDevice(audio);
        else
            SDL_Log("audio device failed: %s", SDL_GetError());
    }

    // Debug hook, same idea as msmoney's MSMONEY_SHOT: render a few frames
    // headless, save a BMP and exit, so the output can be checked without a
    // person watching.
    const char* shotPath = SDL_getenv("TSBDEMO_SHOT");
    const int shotFrame = SDL_getenv("TSBDEMO_SHOT_FRAME")
                              ? SDL_atoi(SDL_getenv("TSBDEMO_SHOT_FRAME"))
                              : 60;

    std::vector<uint32_t> pixels((size_t)tsb::kScreenW * tsb::kScreenH);
    bool running = true;
    Uint64 prev = SDL_GetTicks();
    int frame = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN &&
                (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_Q))
                running = false;
        }

        Uint64 now = SDL_GetTicks();
        double dt = (double)(now - prev) / 1000.0;
        prev = now;
        if (shotPath) dt = 1.0 / 30.0;  // deterministic stepping for screenshots
        demo.update(dt);

        demo.renderTo(pixels.data());
        SDL_UpdateTexture(tex, nullptr, pixels.data(), tsb::kScreenW * (int)sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, nullptr, nullptr);

        if (shotPath && ++frame >= shotFrame) {
            SDL_Surface* s = SDL_RenderReadPixels(ren, nullptr);
            if (s) {
                SDL_SaveBMP(s, shotPath);
                SDL_DestroySurface(s);
            }
            running = false;
        }
        SDL_RenderPresent(ren);
    }

    if (audio) SDL_DestroyAudioStream(audio);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
