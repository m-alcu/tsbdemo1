# tsbdemo

A C++17 / SDL3 replica of **"The Sound Barrier" demo 1**, programmed by Tran of
Renaissance and dated 31 December 1991. Same conventions as `msmoney`: CMake,
SDL3 located through pkg-config, sources in `src/`, a unit-test target for the
non-graphical layer.

The original was an unchained VGA ("Mode X") demo advertising a SoundBlaster
BBS. This replica keeps the same video state it kept (planar VRAM, start
address, line compare, 6-bit DAC), runs translations of its own routines on
it, and plays the same music.

## Building and running

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/tsbdemo          # Esc or Q to quit
ctest --test-dir build   # player unit tests
```

The original runs at **320x400**: it takes mode 13h and clears the maximum
scan line register, so rows are no longer doubled. The window is 1280x960,
the 4:3 of the monitor that showed it, so pixels are as wide and flat as they
were. Scaling is nearest-neighbour.

## Sources

| File | Contents |
|---|---|
| `src/main.cpp` | SDL init, audio stream, frame loop |
| `src/demo.h/.cpp` | emulated VGA state and the original's video routines |
| `tools/extract_assets.py` | pulls the graphics out of the unpacked executable |
| `src/player669.h/.cpp` | Composer 669 playback — no SDL, unit-testable |
| `tests/player_tests.cpp` | module loading and a second of rendered audio |

## What the original does, and this replica with it

Read from the code at 1030:0C97..13F3 of the unpacked executable, not from
screenshots. Ticks are the 30 Hz timer the demo programs into the PIT.

1. **Split screen.** Lines 0..LC of the 400 show VRAM from the start address,
   where the logo picture (with its own nebula) sits at C180h. Lines after
   line compare (LC) show VRAM from address 0: a second nebula, where the
   text goes.
2. **200 ticks of stars alone.** Every colour below 224 starts black, so only
   the stars are lit.
3. **The logo rises out of the middle.** Each tick the start address moves one
   row (B550h to C180h, 39 ticks) and LC moves one line down (142 to 200, 58
   ticks). The logo ends in the upper half; the lower half is the text area.
4. **100 ticks, then the nebulae fade in**: colours 16..63, one step a tick
   over 46 ticks.
5. **100 ticks, then the text.** Seventeen lines, one or two at a time, on
   rows 64 and 84 of the lower half. Each line (and each erase) is dissolved
   in over 14 ticks by visiting the 320x14 strip with a stride of 241 pixels.
6. **Colour cycle.** At each 70 Hz retrace, colours 40..63 get a growing red
   offset, held for 192 ticks, then shrinking. The DAC keeps only 6 bits, so
   red wraps instead of saturating. This reveals text that is painted into
   the nebula in colours identical to it: the BBS name, modem, hours and
   phone number. Then back to step 5, forever.

**Stars**: 200, seeded from a table in the executable, plotted into VRAM only
over pixels below 224 (so behind logo and text), all moving **right**: 80 at
half a pixel a tick, 60 at one, 40 at two, 20 at four.

## Where the assets came from

Nothing here is a re-drawing by hand. `tools/extract_assets.py` takes every
graphic straight out of the unpacked executable
(`../rea/tsbdemo1-analisis/TSBDEMO1.unpacked.bin`), at offsets read from the
code that loads them:

```bash
python3 -I tools/extract_assets.py ../rea/tsbdemo1-analisis/TSBDEMO1.unpacked.bin assets
```

- **`logo.bmp`**, **`nebula.bmp`**: the two 320x200 pictures, upper and lower.
  All three pictures are 8-bit indexed BMPs, so any image viewer opens them.
  Their pixels are the original palette indices, and their colour table is
  `palette.dac` scaled to 8 bits, so they show the screen once the nebula has
  faded in. The demo reads only the indices; an editor that reorders the
  palette on save would break the fade and the colour cycle. The hidden text
  is in `nebula.bmp` too, invisible: raise the red of colours 40..54 in an
  editor and it appears, which is what the colour cycle does.
- **`palette.dac`**: the DAC values the demo uses (16..63 nebula, 224..255
  logo, font and stars).
- **`font.bmp`**, **`src/charmap.h`**: the glyph sheet and the ASCII mapping
  (which remaps `x` to an apostrophe, `z` to a slash, `j` to a colon).
- **`src/stars.h`**: the star seeds, with how each one is decoded.
- **`texts.txt`**: the seventeen text lines, in the order the code shows them.
- **`music.669`**: one full cycle of the tune (426.7 s, 50 patterns, 12
  samples, two of them looping), reconstructed by observing what the
  original's mixer did, because its own format is proprietary and
  undocumented. Simulated tick by tick against the mixer trace, it plays the
  same voices except for single ticks at note ends; see section 6 of
  `../rea/tsbdemo1-analisis/README.md`.

## Verification

The replica was compared pixel by pixel against the original running in the
8086 emulator of `../rea/tsbdemo1-analisis`, with the emulator's screen
composed the way the CRT does it (400 lines, split at line compare). Excluding
star pixels, the scroll and both text pages compared were identical; the stars
themselves matched in all four groups.

## Debug hook

Like msmoney's `MSMONEY_SHOT`, the demo can render headless and save a frame,
so the output can be checked without a person watching:

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
TSBDEMO_SHOT=/tmp/shot.bmp TSBDEMO_SHOT_FRAME=600 ./build/tsbdemo
```

`TSBDEMO_ASSETS` overrides the asset directory (useful from a build dir).
