// ASCII -> glyph table of the text scroller, copied from 1030:07F2 of the
// unpacked TSBDEMO1 image (128 bytes, indexed by character code & 0x7F).
//
// font.bmp is a sheet of 70 glyphs, 15 columns each; glyph g starts at
// column g * 15. The table puts them in this order:
//
//     0        blank (also every code that has no glyph)
//     1..26    A..Z
//     27..52   a..z
//     53..62   0..9
//     63..69   . , ? ! - ( )
//
// The sheet has no apostrophe, slash or colon. Tran drew them over three
// lowercase letters the texts never need, and the texts spell them with those
// letters: in assets/texts.txt "24 Hoursz7 Days" shows as "24 Hours/7 Days".
//
//     'j' -> glyph 36, drawn as ':'
//     'x' -> glyph 50, drawn as an apostrophe
//     'z' -> glyph 52, drawn as '/'
//
// '{', '|' and '}' point past the sheet (72, 101, 121); nothing uses them, and
// Demo::glyph() treats anything past glyph 69 as blank.
#pragma once

#include <cstdint>

namespace tsb {

constexpr int kGlyphCount = 70;

constexpr uint8_t kCharmap[128] = {
    // 0x00..0x1F: control codes, no glyph
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  // 0x20 ' '   never looked up: buildStrip advances 8 pixels for a space
     66,  // 0x21 '!'   draws '!'
      0,  // 0x22 '"'   no glyph
      0,  // 0x23 '#'   no glyph
      0,  // 0x24 '$'   no glyph
      0,  // 0x25 '%'   no glyph
      0,  // 0x26 '&'   no glyph
      0,  // 0x27 '\''  no glyph: write 'x'
     68,  // 0x28 '('   draws '('
     69,  // 0x29 ')'   draws ')'
      0,  // 0x2A '*'   no glyph
      0,  // 0x2B '+'   no glyph
     64,  // 0x2C ','   draws ','
     67,  // 0x2D '-'   draws '-'
     63,  // 0x2E '.'   draws '.'
      0,  // 0x2F '/'   no glyph: write 'z'
     53,  // 0x30 '0'   draws '0'
     54,  // 0x31 '1'   draws '1'
     55,  // 0x32 '2'   draws '2'
     56,  // 0x33 '3'   draws '3'
     57,  // 0x34 '4'   draws '4'
     58,  // 0x35 '5'   draws '5'
     59,  // 0x36 '6'   draws '6'
     60,  // 0x37 '7'   draws '7'
     61,  // 0x38 '8'   draws '8'
     62,  // 0x39 '9'   draws '9'
      0,  // 0x3A ':'   no glyph: write 'j'
      0,  // 0x3B ';'   no glyph
      0,  // 0x3C '<'   no glyph
      0,  // 0x3D '='   no glyph
      0,  // 0x3E '>'   no glyph
     65,  // 0x3F '?'   draws '?'
      0,  // 0x40 '@'   no glyph
      1,  // 0x41 'A'   draws 'A'
      2,  // 0x42 'B'   draws 'B'
      3,  // 0x43 'C'   draws 'C'
      4,  // 0x44 'D'   draws 'D'
      5,  // 0x45 'E'   draws 'E'
      6,  // 0x46 'F'   draws 'F'
      7,  // 0x47 'G'   draws 'G'
      8,  // 0x48 'H'   draws 'H'
      9,  // 0x49 'I'   draws 'I'
     10,  // 0x4A 'J'   draws 'J'
     11,  // 0x4B 'K'   draws 'K'
     12,  // 0x4C 'L'   draws 'L'
     13,  // 0x4D 'M'   draws 'M'
     14,  // 0x4E 'N'   draws 'N'
     15,  // 0x4F 'O'   draws 'O'
     16,  // 0x50 'P'   draws 'P'
     17,  // 0x51 'Q'   draws 'Q'
     18,  // 0x52 'R'   draws 'R'
     19,  // 0x53 'S'   draws 'S'
     20,  // 0x54 'T'   draws 'T'
     21,  // 0x55 'U'   draws 'U'
     22,  // 0x56 'V'   draws 'V'
     23,  // 0x57 'W'   draws 'W'
     24,  // 0x58 'X'   draws 'X'
     25,  // 0x59 'Y'   draws 'Y'
     26,  // 0x5A 'Z'   draws 'Z'
      0,  // 0x5B '['   no glyph
      0,  // 0x5C '\\'  no glyph
      0,  // 0x5D ']'   no glyph
      0,  // 0x5E '^'   no glyph
      0,  // 0x5F '_'   no glyph
      0,  // 0x60 '`'   no glyph
     27,  // 0x61 'a'   draws 'a'
     28,  // 0x62 'b'   draws 'b'
     29,  // 0x63 'c'   draws 'c'
     30,  // 0x64 'd'   draws 'd'
     31,  // 0x65 'e'   draws 'e'
     32,  // 0x66 'f'   draws 'f'
     33,  // 0x67 'g'   draws 'g'
     34,  // 0x68 'h'   draws 'h'
     35,  // 0x69 'i'   draws 'i'
     36,  // 0x6A 'j'   the sheet draws a colon here: write 'j' for ':'
     37,  // 0x6B 'k'   draws 'k'
     38,  // 0x6C 'l'   draws 'l'
     39,  // 0x6D 'm'   draws 'm'
     40,  // 0x6E 'n'   draws 'n'
     41,  // 0x6F 'o'   draws 'o'
     42,  // 0x70 'p'   draws 'p'
     43,  // 0x71 'q'   draws 'q'
     44,  // 0x72 'r'   draws 'r'
     45,  // 0x73 's'   draws 's'
     46,  // 0x74 't'   draws 't'
     47,  // 0x75 'u'   draws 'u'
     48,  // 0x76 'v'   draws 'v'
     49,  // 0x77 'w'   draws 'w'
     50,  // 0x78 'x'   the sheet draws an apostrophe here: write 'x' for '\''
     51,  // 0x79 'y'   draws 'y'
     52,  // 0x7A 'z'   the sheet draws a slash here: write 'z' for '/'
     72,  // 0x7B '{'   past the 70 glyphs of the sheet: not a glyph, unused
    101,  // 0x7C '|'   past the 70 glyphs of the sheet: not a glyph, unused
    121,  // 0x7D '}'   past the 70 glyphs of the sheet: not a glyph, unused
     46,  // 0x7E '~'   same glyph as 't'
     46,  // 0x7F DEL   same glyph as 't'
};

}  // namespace tsb
