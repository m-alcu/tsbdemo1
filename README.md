# tsbdemo

A C++17 / SDL3 replica of **"The Sound Barrier" demo 1**, programmed by Tran of
Renaissance and dated 31 December 1991. Same conventions as `msmoney`: CMake,
SDL3 located through pkg-config, sources in `src/`, a unit-test target for the
non-graphical layer.

The original was a 320x200 unchained VGA ("Mode X") demo advertising a
SoundBlaster BBS. This replica draws the same screen from the same palette and
plays the same music.

## Building and running

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/tsbdemo          # Esc or Q to quit
ctest --test-dir build   # player unit tests
```

The window is 1280x960, which is 4:3 — the aspect a 320x200 VGA mode actually
had, since its pixels were not square. Scaling is nearest-neighbour, so the
pixels stay chunky.

## Sources

| File | Contents |
|---|---|
| `src/main.cpp` | SDL init, audio stream, frame loop |
| `src/demo.h/.cpp` | 320x200 indexed framebuffer, palette, starfield |
| `src/player669.h/.cpp` | Composer 669 playback — no SDL, unit-testable |
| `tests/player_tests.cpp` | module loading and a second of rendered audio |

## Where the assets came from

Nothing here is a re-drawing by hand. Everything was recovered from the
original executable; the full account is in
`../rea/tsbdemo1-analisis/README.md`.

- **`assets/background.pal`** — the 256 colours the demo programs into the VGA
  DAC, captured from an emulator built for the job.
- **`assets/background.idx`** — the 320x200 screen as palette indices, taken
  from the original's video memory. The moving starfield was removed by taking
  the per-pixel most frequent index across six frames: the nebula and logo are
  static, the stars are not.
- **`assets/music.669`** — one full cycle of the tune (426.7 s, 50 patterns,
  11 samples), reconstructed by observing what the original's mixer did,
  because its own format is proprietary and undocumented.

## Debug hook

Like msmoney's `MSMONEY_SHOT`, the demo can render headless and save a frame,
so the output can be checked without a person watching:

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
TSBDEMO_SHOT=/tmp/shot.bmp TSBDEMO_SHOT_FRAME=90 ./build/tsbdemo
```

`TSBDEMO_ASSETS` overrides the asset directory (useful from a build dir).

## What is not reproduced yet

Stated plainly, because the replica is not finished:

- **The text pages.** The original carries a BBS advert ("Hey...You like this
  cool demo? ... give the Sound Barrier BBS a call!") and a greetings list,
  drawn with a bitmap font whose punctuation is remapped onto letters
  (`x` is an apostrophe, `z` a slash, `j` a colon). That font has not been
  located in the binary yet, so none of the text is drawn.
- **Palette animation.** The original cycles and fades the palette constantly
  (426 index writes and 34,752 colour writes in a 70-second window). Here the
  palette is static.
- **The per-scanline raster effect.** The original reprograms the CRTC start
  address and line-compare registers once per scanline, synchronised to the
  vertical retrace. SDL has no equivalent; reproducing the look would mean
  shearing the framebuffer per row in software.
- **The logo movement.** In the original the title rises into place; here it
  is already in position.

The starfield is regenerated rather than extracted: the stars are drawn by
code, not stored as data, so the look is matched (single pixels, three
parallax speeds) but the positions are not the original ones.
