// SPDX-License-Identifier: GPL-3.0-or-later
//
// js8encode — port of upstream JS8Call's JS8::encode() to STL-only C++
// without the boost CRC dependency.
//
// Input:  type (3-bit i3 frame-type), 12 chars from alphabet72.
// Output: array of 79 ints in [0..7], the JS8 tone sequence
//         (Costas A | parity 29 | Costas B | data 29 | Costas C).
//
// This replaces fate's pack_any() which uses an incompatible 87-bit
// information-bit layout — verified by feeding fate's output through
// JS8Call's standalone /usr/bin/js8 decoder (gibberish output).
//
// Source: js8call/js8call JS8.cpp::encode (lines ~2645-2800), GPL-3.

#ifndef X6100JS8_JS8ENCODE_H
#define X6100JS8_JS8ENCODE_H

#include <array>
#include <cstdint>

namespace x6100js8 {

// JS8 Normal submode Costas array (FT8 "original"). Same for all 3 blocks.
extern const std::array<std::array<int, 7>, 3> kCostasNormal;

// Pack a 12-char alphabet72 message + 3-bit i3 type into 79 tones.
// `message` must be exactly 12 chars from alphabet72 = "0..9A..Za..z-+".
// Returns true on success, false if `message` contains an invalid char.
bool js8_encode_tones(int                                 type,
                      const std::array<std::array<int, 7>, 3> &costas,
                      const char                          *message,
                      int                                  tones[79]);

}  // namespace x6100js8

#endif
