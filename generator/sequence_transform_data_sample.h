// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

/*******************************************************************************
  88888888888 888      d8b                .d888 d8b 888               d8b
      888     888      Y8P               d88P"  Y8P 888               Y8P
      888     888                        888        888
      888     88888b.  888 .d8888b       888888 888 888  .d88b.       888 .d8888b
      888     888 "88b 888 88K           888    888 888 d8P  Y8b      888 88K
      888     888  888 888 "Y8888b.      888    888 888 88888888      888 "Y8888b.
      888     888  888 888      X88      888    888 888 Y8b.          888      X88
      888     888  888 888  88888P'      888    888 888  "Y8888       888  88888P'
                                                        888                 888
                                                        888                 888
                                                        888                 888
     .d88b.   .d88b.  88888b.   .d88b.  888d888 8888b.  888888 .d88b.   .d88888
    d88P"88b d8P  Y8b 888 "88b d8P  Y8b 888P"      "88b 888   d8P  Y8b d88" 888
    888  888 88888888 888  888 88888888 888    .d888888 888   88888888 888  888
    Y88b 888 Y8b.     888  888 Y8b.     888    888  888 Y88b. Y8b.     Y88b 888
     "Y88888  "Y8888  888  888  "Y8888  888    "Y888888  "Y888 "Y8888   "Y88888
         888
    Y8b d88P
     "Y88P"
*******************************************************************************/

#pragma once

// Autocorrection dictionary with longest match semantics:
// Autocorrection dictionary (98 entries):
//   👍      -> ↻
//   d★     -> develop
//   d★t    -> development
//   d★r    -> developer
//   d★d    -> developed
//   ⎵i👍    -> I
//   ⎵i👍m   -> I'm
//   ⎵i👍d   -> I'd
//   ⎵i👍l   -> I'll
//   ⎵c👍    -> come
//   ⎵s👍    -> some
//   ⎵c👍u   -> could
//   ⎵w👍u   -> would
//   ⎵s👍u   -> should
//   ⎵c👍ut  -> couldn't
//   ⎵w👍ut  -> wouldn't
//   ⎵s👍ut  -> shouldn't
//   ⎵c👍uv  -> could've
//   ⎵w👍uv  -> would've
//   ⎵s👍uv  -> should've
//   ⎵👆     -> the
//   ⎵👆r    -> there
//   ⎵👆rs   -> there's
//   ⎵👆i    -> their
//   ⎵👆yr   -> they're
//   ⎵👆yl   -> they'll
//   ⎵👆d    -> they'd
//   ⎵👆s    -> these
//   ⎵👆t    -> that
//   ⎵👆a    -> than
//   ⎵👆o    -> those
//   ⎵t👍    -> time
//   ⎵t👍g   -> though
//   ⎵t👍t   -> thought
//   ⎵t👍r   -> through
//   ⎵t👍c   -> technology
//   ⎵t👍a   -> take
//   ⎵w👍    -> why
//   ⎵w👍n   -> when
//   ⎵w👍e   -> where
//   ⎵w👍t   -> what
//   ⎵w👍r   -> we're
//   ⎵w👍d   -> we'd
//   ⎵w👍l   -> we'll
//   ⎵w👍v   -> we've
//   a👆     -> abo
//   a👆👍    -> about
//   a👆👆    -> above
//   v👆     -> ver
//   s👆     -> sk
//   x👆     -> es
//   m👆     -> ment
//   t👆     -> tment
//   k👆     -> ks
//   l👆     -> lk
//   r👆     -> rl
//   j👆     -> just
//   j👆👆    -> justment
//   ⎵j👍    -> join
//   ⎵j👍d   -> judge
//   ⎵j👍dt  -> judgment
//   ⎵j👍dta -> judgmental
//   c👆     -> cy
//   p👆     -> py
//   d👆     -> dy
//   y👆     -> yp
//   g👆     -> gy
//   w👆     -> which
//   q👆     -> question
//   b👆     -> before
//   f👆     -> first
//   z👆     -> zone
//   👆👆     -> 👆n
//   n👆     -> nion
//   h👆     -> however
//   u👆     -> ue
//   e👆     -> eu
//   o👆     -> oa
//   ,👆     -> ,⎵but
//   i👆     -> ion
//   .👆     -> .\ [escape]
//   ⎵👍     -> for
//   a👍     -> and
//   x👍     -> xer
//   k👍     -> know
//   i👍     -> ing
//   y👍     -> you
//   q👍     -> quick
//   j👍     -> join
//   c👍     -> ck
//   n👍     -> nf
//   h👍     -> hn
//   ,👍     -> ,⎵and
//   .👍     -> .⎵⇑
//   ?👍     -> ?⎵⇑
//   :👍     -> :⎵⇑
//   ;👍     -> ;⎵⇑
//   !👍     -> !⎵⇑

#define MAGICKEY_MIN_LENGTH 1 // "👍"
#define MAGICKEY_MAX_LENGTH 6 // "⎵j👍dta"
#define DICTIONARY_SIZE 558
#define COMPLETIONS_SIZE 295
#define MAGICKEY_COUNT 4

static const uint16_t sequence_transform_data[DICTIONARY_SIZE] PROGMEM = {
    0x4004, 0x0025, 0x0006, 0x003C, 0x0007, 0x0042, 0x0008, 0x0064, 0x000A, 0x006A, 0x000C, 0x0070, 0x000F, 0x0075, 0x0010, 0x008C,
    0x0011, 0x0092, 0x0012, 0x0098, 0x0015, 0x009D, 0x0016, 0x00C0, 0x0017, 0x00CE, 0x0018, 0x0109, 0x0019, 0x011E, 0x0102, 0x013D,
    0x0100, 0x0141, 0x0101, 0x01C3, 0x0000, 0x4017, 0x002C, 0x0100, 0x0033, 0x0101, 0x0037, 0x0000, 0x0007, 0x0101, 0x000D, 0x002C,
    0x0000, 0x8000, 0x0000, 0x002C, 0x0000, 0x8001, 0x0003, 0x0017, 0x002C, 0x0000, 0x8003, 0x0006, 0x0101, 0x0017, 0x002C, 0x0000,
    0x8003, 0x000A, 0x4102, 0x0049, 0x0100, 0x004D, 0x0101, 0x0051, 0x0000, 0x0007, 0x0000, 0x8000, 0x0014, 0x002C, 0x0000, 0x8000,
    0x0017, 0x400C, 0x0058, 0x000D, 0x005C, 0x001A, 0x0060, 0x0000, 0x002C, 0x0000, 0x8000, 0x001B, 0x002C, 0x0000, 0x8003, 0x001E,
    0x002C, 0x0000, 0x8002, 0x0023, 0x0101, 0x001A, 0x002C, 0x0000, 0x8001, 0x0027, 0x0101, 0x0017, 0x002C, 0x0000, 0x8003, 0x002B,
    0x0100, 0x002C, 0x0000, 0x8000, 0x0031, 0x401C, 0x007A, 0x0101, 0x007F, 0x0000, 0x0100, 0x002C, 0x0000, 0x8003, 0x0034, 0x400C,
    0x0084, 0x001A, 0x0088, 0x0000, 0x002C, 0x0000, 0x8000, 0x0039, 0x002C, 0x0000, 0x8002, 0x003D, 0x0101, 0x000C, 0x002C, 0x0000,
    0x8000, 0x0042, 0x0101, 0x001A, 0x002C, 0x0000, 0x8001, 0x0045, 0x0100, 0x002C, 0x0000, 0x8001, 0x0048, 0x401C, 0x00A6, 0x0102,
    0x00AB, 0x0100, 0x00AF, 0x0101, 0x00B3, 0x0000, 0x0100, 0x002C, 0x0000, 0x8003, 0x004C, 0x0007, 0x0000, 0x8000, 0x0051, 0x002C,
    0x0000, 0x8000, 0x0054, 0x4017, 0x00B8, 0x001A, 0x00BC, 0x0000, 0x002C, 0x0000, 0x8003, 0x0057, 0x002C, 0x0000, 0x8002, 0x005E,
    0x4015, 0x00C5, 0x0100, 0x00CA, 0x0000, 0x0100, 0x002C, 0x0000, 0x8000, 0x0063, 0x002C, 0x0000, 0x8000, 0x0066, 0x4007, 0x00D9,
    0x0018, 0x00DF, 0x0102, 0x00F4, 0x0100, 0x00F8, 0x0101, 0x00FC, 0x0000, 0x0101, 0x000D, 0x002C, 0x0000, 0x8001, 0x0069, 0x0101,
    0x0000, 0x4006, 0x00E8, 0x0016, 0x00EC, 0x001A, 0x00F0, 0x0000, 0x002C, 0x0000, 0x8000, 0x006E, 0x002C, 0x0000, 0x8000, 0x006E,
    0x002C, 0x0000, 0x8000, 0x006E, 0x0007, 0x0000, 0x8000, 0x0069, 0x002C, 0x0000, 0x8001, 0x0072, 0x4017, 0x0101, 0x001A, 0x0105,
    0x0000, 0x002C, 0x0000, 0x8003, 0x0075, 0x002C, 0x0000, 0x8001, 0x0072, 0x0101, 0x0000, 0x4006, 0x0112, 0x0016, 0x0116, 0x001A,
    0x011A, 0x0000, 0x002C, 0x0000, 0x8002, 0x007C, 0x002C, 0x0000, 0x8003, 0x0080, 0x002C, 0x0000, 0x8002, 0x0086, 0x4018, 0x0123,
    0x0101, 0x0138, 0x0000, 0x0101, 0x0000, 0x4006, 0x012C, 0x0016, 0x0130, 0x001A, 0x0134, 0x0000, 0x002C, 0x0000, 0x8000, 0x008B,
    0x002C, 0x0000, 0x8000, 0x008B, 0x002C, 0x0000, 0x8000, 0x008B, 0x001A, 0x002C, 0x0000, 0x8002, 0x008F, 0x0007, 0x0000, 0x8000,
    0x0094, 0x4036, 0x017E, 0x0037, 0x0180, 0x0004, 0x0182, 0x0005, 0x0184, 0x0006, 0x0186, 0x0007, 0x0188, 0x0008, 0x018A, 0x0009,
    0x018C, 0x000A, 0x018E, 0x000B, 0x0190, 0x000C, 0x0192, 0x000D, 0x0194, 0x000E, 0x0196, 0x000F, 0x0198, 0x0010, 0x019A, 0x0011,
    0x019C, 0x0012, 0x019E, 0x0013, 0x01A0, 0x0014, 0x01A2, 0x0015, 0x01A4, 0x0016, 0x01A6, 0x0017, 0x01A8, 0x0018, 0x01AA, 0x0019,
    0x01AC, 0x001A, 0x01AE, 0x001B, 0x01B0, 0x001C, 0x01B2, 0x001D, 0x01B4, 0x002C, 0x01B6, 0x0100, 0x01B8, 0x0000, 0x8000, 0x009B,
    0x8000, 0x00A0, 0x8000, 0x00A2, 0x8000, 0x00A5, 0x8000, 0x00AB, 0x8000, 0x00AB, 0x8000, 0x00AD, 0x8000, 0x00AF, 0x8000, 0x00AB,
    0x8000, 0x00B4, 0x8000, 0x00BB, 0x8000, 0x00BE, 0x8000, 0x00C2, 0x8000, 0x00C4, 0x8000, 0x00C6, 0x8000, 0x00CA, 0x8000, 0x00CE,
    0x8000, 0x00AB, 0x8000, 0x00D0, 0x8000, 0x00D8, 0x8000, 0x00C4, 0x8000, 0x0069, 0x8000, 0x00DA, 0x8000, 0x0051, 0x8000, 0x00DC,
    0x8001, 0x00E1, 0x8000, 0x00E4, 0x8000, 0x00E6, 0x8000, 0x00EA, 0xC000, 0x00EE, 0x4004, 0x01BF, 0x000D, 0x01C1, 0x0000, 0x8000,
    0x00F0, 0x8000, 0x0069, 0xC800, 0x00F3, 0x421E, 0x01F0, 0x0036, 0x01F2, 0x0037, 0x01F4, 0x0233, 0x01F6, 0x0033, 0x01F8, 0x0238,
    0x01FA, 0x0004, 0x01FC, 0x0006, 0x01FE, 0x000B, 0x0204, 0x000C, 0x0206, 0x000D, 0x020C, 0x000E, 0x0212, 0x0011, 0x0214, 0x0014,
    0x0216, 0x0016, 0x0218, 0x0017, 0x021C, 0x001A, 0x0220, 0x001B, 0x0224, 0x001C, 0x0226, 0x002C, 0x0228, 0x0100, 0x022A, 0x0000,
    0x9000, 0x00F4, 0x8000, 0x00F6, 0x9000, 0x00F4, 0x9000, 0x00F4, 0x9000, 0x00F4, 0x9000, 0x00F4, 0x8000, 0x00FB, 0xC000, 0x00C4,
    0x002C, 0x0000, 0x8000, 0x00FE, 0x8000, 0x00EE, 0xC000, 0x0102, 0x002C, 0x0000, 0x8001, 0x0105, 0xC000, 0x0107, 0x002C, 0x0000,
    0x8000, 0x0107, 0x8000, 0x010B, 0x8000, 0x010F, 0x8000, 0x0111, 0x002C, 0x0000, 0x8000, 0x00FE, 0x002C, 0x0000, 0x8000, 0x0116,
    0x002C, 0x0000, 0x8000, 0x011A, 0x8000, 0x0051, 0x8000, 0x011D, 0x8000, 0x0120, 0x0004, 0x0000, 0x8000, 0x0124
};

static const uint8_t sequence_transform_completions_data[COMPLETIONS_SIZE] PROGMEM = {
    0x61, 0x6C, 0x00, 0x61, 0x6E, 0x00, 0x61, 0x6B, 0x65, 0x00, 0x65, 0x63, 0x68, 0x6E, 0x6F, 0x6C,
    0x6F, 0x67, 0x79, 0x00, 0x65, 0x64, 0x00, 0x79, 0x27, 0x64, 0x00, 0x27, 0x64, 0x00, 0x75, 0x64,
    0x67, 0x65, 0x00, 0x65, 0x27, 0x64, 0x00, 0x65, 0x72, 0x65, 0x00, 0x68, 0x6F, 0x75, 0x67, 0x68,
    0x00, 0x69, 0x72, 0x00, 0x79, 0x27, 0x6C, 0x6C, 0x00, 0x27, 0x6C, 0x6C, 0x00, 0x65, 0x27, 0x6C,
    0x6C, 0x00, 0x27, 0x6D, 0x00, 0x65, 0x6E, 0x00, 0x6F, 0x73, 0x65, 0x00, 0x79, 0x27, 0x72, 0x65,
    0x00, 0x65, 0x72, 0x00, 0x72, 0x65, 0x00, 0x68, 0x72, 0x6F, 0x75, 0x67, 0x68, 0x00, 0x65, 0x27,
    0x72, 0x65, 0x00, 0x27, 0x73, 0x00, 0x73, 0x65, 0x00, 0x6D, 0x65, 0x6E, 0x74, 0x00, 0x6E, 0x27,
    0x74, 0x00, 0x61, 0x74, 0x00, 0x68, 0x6F, 0x75, 0x67, 0x68, 0x74, 0x00, 0x75, 0x6C, 0x64, 0x00,
    0x68, 0x6F, 0x75, 0x6C, 0x64, 0x00, 0x6F, 0x75, 0x6C, 0x64, 0x00, 0x27, 0x76, 0x65, 0x00, 0x65,
    0x27, 0x76, 0x65, 0x00, 0x65, 0x76, 0x65, 0x6C, 0x6F, 0x70, 0x00, 0x20, 0x62, 0x75, 0x74, 0x00,
    0x5C, 0x00, 0x62, 0x6F, 0x00, 0x65, 0x66, 0x6F, 0x72, 0x65, 0x00, 0x79, 0x00, 0x75, 0x00, 0x69,
    0x72, 0x73, 0x74, 0x00, 0x6F, 0x77, 0x65, 0x76, 0x65, 0x72, 0x00, 0x6F, 0x6E, 0x00, 0x75, 0x73,
    0x74, 0x00, 0x73, 0x00, 0x6B, 0x00, 0x65, 0x6E, 0x74, 0x00, 0x69, 0x6F, 0x6E, 0x00, 0x61, 0x00,
    0x75, 0x65, 0x73, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x6C, 0x00, 0x65, 0x00, 0x68, 0x69, 0x63, 0x68,
    0x00, 0x65, 0x73, 0x00, 0x70, 0x00, 0x6F, 0x6E, 0x65, 0x00, 0x74, 0x68, 0x65, 0x00, 0x6E, 0x00,
    0x76, 0x65, 0x00, 0x00, 0x20, 0x00, 0x20, 0x61, 0x6E, 0x64, 0x00, 0x6E, 0x64, 0x00, 0x6F, 0x6D,
    0x65, 0x00, 0x6E, 0x67, 0x00, 0x49, 0x00, 0x6F, 0x69, 0x6E, 0x00, 0x6E, 0x6F, 0x77, 0x00, 0x66,
    0x00, 0x75, 0x69, 0x63, 0x6B, 0x00, 0x69, 0x6D, 0x65, 0x00, 0x68, 0x79, 0x00, 0x6F, 0x75, 0x00,
    0x66, 0x6F, 0x72, 0x00, 0x75, 0x74, 0x00
};
