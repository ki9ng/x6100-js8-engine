// SPDX-License-Identifier: GPL-3.0-or-later
//
// js8encode — build a JS8Call-layout 87-bit information vector + CRC, then
// hand off to fate's existing LDPC encoder + tone packer + FSK modulator.
//
// Why this exists: fate's pack_directed() and similar produce 87-bit vectors
// with fields at fate-specific bit positions. JS8Call's decoder expects the
// 87-bit vector laid out as:
//
//     bits   0..71  = 12 × 6-bit alphabet72 words ("the message")
//     bits  72..74  = i3 (frame type, 3 bits)
//     bits  75..86  = CRC-12 over the augmented array
//
// Fate's downstream pipeline (ldpc_encode + recode + fsk) is layout-agnostic
// — it just runs LDPC and emits tones. So the only delta is at the front.

#include "js8encode.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// fate's pipeline functions live in the global namespace (no namespace,
// no extern "C"). Forward-declare them at file scope so the compiler binds
// to the unqualified mangled names fate emits.
extern void ft8_crc(int msg1[], int msglen, int out[12]);
extern void ldpc_encode(int plain[87], int codeword[174]);
extern std::vector<int> recode(int a174[]);

namespace x6100js8 {

// ---------------------------------------------------------------------------
// JS8 Normal Costas array (FT8 "original" set), repeated for blocks A/B/C.
// Identical to fate's recode.cc but exposed for any future submode work.
// ---------------------------------------------------------------------------

const std::array<std::array<int, 7>, 3> kCostasNormal = {{
    {4, 2, 5, 6, 1, 3, 0},
    {4, 2, 5, 6, 1, 3, 0},
    {4, 2, 5, 6, 1, 3, 0},
}};

// ---------------------------------------------------------------------------
// alphabet64 lookup: indices 0..9->'0'..'9', 10..35->'A'..'Z',
// 36..61->'a'..'z', 62->'-', 63->'+'.
// ---------------------------------------------------------------------------

static int alphabet_word(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 36;
    if (c == '-') return 62;
    if (c == '+') return 63;
    return -1;
}

// ---------------------------------------------------------------------------
// js8_encode_tones — main entry point.
// ---------------------------------------------------------------------------

bool js8_encode_tones(int                                      type,
                      const std::array<std::array<int, 7>, 3> &costas,
                      const char                              *message,
                      int                                      tones[79]) {
    int a87[87];
    std::memset(a87, 0, sizeof(a87));

    // bits 0..71: 12 6-bit alphabet64 words, MSB-first per word
    for (int i = 0; i < 12; i++) {
        int w = alphabet_word(message[i]);
        if (w < 0) return false;
        for (int b = 0; b < 6; b++) {
            a87[i * 6 + b] = (w >> (5 - b)) & 1;
        }
    }

    // bits 72..74: i3 (frame type), MSB-first
    a87[72] = (type >> 2) & 1;
    a87[73] = (type >> 1) & 1;
    a87[74] = (type >> 0) & 1;

    // bits 75..86: CRC-12 over the first 75 bits.
    int crc[12];
    ft8_crc(a87, 76, crc);
    for (int i = 0; i < 12; i++) a87[75 + i] = crc[i];

    // LDPC encode -> 174 bits
    int a174[174];
    ldpc_encode(a87, a174);

    // Recode 174 bits -> 79 tones (with Costas blocks inserted)
    auto vec = recode(a174);
    if (vec.size() != 79) return false;
    for (int i = 0; i < 79; i++) tones[i] = vec[i];

    // Override Costas blocks if a non-default array was supplied.
    for (int i = 0; i < 7; i++) {
        tones[i]      = costas[0][i];
        tones[36 + i] = costas[1][i];
        tones[72 + i] = costas[2][i];
    }

    return true;
}

}  // namespace x6100js8
