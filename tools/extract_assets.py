"""Extract the demo's graphics data from the unpacked TSBDEMO1 image.

Usage: python3 -I tools/extract_assets.py <TSBDEMO1.unpacked.bin> <assets dir>

The unpacked image is ../rea/tsbdemo1-analisis/TSBDEMO1.unpacked.bin. Every
offset below comes from the original's own code (segment 1030h, the video
routines at 0C97h..13F3h), not from screenshots:

  logo.bmp     320x200, segment 2ACBh  -> copied to VRAM C180h (upper screen)
  nebula.bmp   320x200, segment 3A6Bh  -> copied to VRAM 0000h (lower screen)
  palette.dac  256 x RGB, 6-bit DAC values: colours 16..63 are the nebula
               fade target (3A6B:FA30), 224..255 the logo/font/stars
               (3A6B:FCA0). Everything else is black in the original.
  font.bmp     1050x14 glyph sheet, segment 4A3Bh, 15 columns per glyph
  charmap.bin  ASCII -> glyph index (1030:07F2)
  stars.bin    200 x {word, byte, byte} star seeds (1030:0C7E)

The pictures are written as 8-bit indexed BMPs so they open in any viewer.
The pixels are the original palette indices, untouched, and every BMP carries
palette.dac (scaled from 6 to 8 bits) as its colour table, so a picture shows
as it looks once the nebula has faded in. The demo reads only the indices;
its palette still comes from palette.dac.
"""

import os
import struct
import sys

LOAD_SEG = 0x1000


def at(seg, off=0):
    return (seg - LOAD_SEG) * 16 + off


def bmp(pixels, width, height, dac):
    """An uncompressed 8-bit BMP: 14-byte file header, 40-byte info header,
    256 BGRx palette entries, then the rows bottom-up, each padded to 4 bytes."""
    stride = (width + 3) & ~3
    palette = bytearray()
    for i in range(256):
        r, g, b = (v << 2 | v >> 4 for v in dac[i * 3:i * 3 + 3])  # 0..63 -> 0..255
        palette += bytes((b, g, r, 0))
    rows = bytearray()
    for y in reversed(range(height)):
        rows += pixels[y * width:(y + 1) * width] + bytes(stride - width)
    offset = 14 + 40 + len(palette)
    return (b"BM" + struct.pack("<IHHI", offset + len(rows), 0, 0, offset)
            + struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(rows),
                          2835, 2835, 256, 0)
            + palette + rows)


def main():
    image = open(sys.argv[1], "rb").read()
    out = sys.argv[2]

    def put(name, data):
        with open(os.path.join(out, name), "wb") as f:
            f.write(data)
        print(f"{name:12s} {len(data):6d} bytes")

    dac = bytearray(768)
    dac[16 * 3:64 * 3] = image[at(0x3A6B, 0xFA30):at(0x3A6B, 0xFA30) + 144]
    dac[224 * 3:256 * 3] = image[at(0x3A6B, 0xFCA0):at(0x3A6B, 0xFCA0) + 96]
    put("palette.dac", bytes(dac))

    put("logo.bmp", bmp(image[at(0x2ACB):at(0x2ACB) + 64000], 320, 200, dac))
    put("nebula.bmp", bmp(image[at(0x3A6B):at(0x3A6B) + 64000], 320, 200, dac))
    put("font.bmp", bmp(image[at(0x4A3B):at(0x4A3B) + 1050 * 14], 1050, 14, dac))
    put("charmap.bin", image[at(0x1030, 0x07F2):at(0x1030, 0x07F2) + 128])
    put("stars.bin", image[at(0x1030, 0x0C7E):at(0x1030, 0x0C7E) + 800])


if __name__ == "__main__":
    main()
