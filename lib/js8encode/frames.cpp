// SPDX-License-Identifier: GPL-3.0-or-later
//
// Frame builders — given a JS8Call-format message-bit layout, hand off to
// js8_encode_tones() to produce 79 tones, then to fate's fsk() to make 48 kHz
// PCM. This is the "info-bits → audio" half of the library; combined with
// the public C API in src/x6100js8.cc, it produces JS8Call-interoperable PCM.

#include "js8encode.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// fate's pack_grid is in the global namespace; bind here.
extern unsigned int pack_grid(std::string grid);

namespace x6100js8 {

// ---------------------------------------------------------------------------
// alphabetnum: the 38-char "alphanumeric" set used by packAlphaNumeric50
// (callsign packing). 0-9, A-Z, space, /, @.
// ---------------------------------------------------------------------------

static const char *kAlphanum = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ /@";  // 39 chars

static int alphanum_index(char c) {
    const char *p = std::strchr(kAlphanum, c);
    return p ? (int)(p - kAlphanum) : -1;
}

// ---------------------------------------------------------------------------
// pack_alpha_numeric_50 — port of upstream Varicode::packAlphaNumeric50.
// Takes an 11-char-or-shorter callsign-with-grid-or-suffix (uppercased), pads
// to 11 chars, and packs into a 50-bit value. Insert spaces at slots 3 and 7
// unless they're '/' (i.e. portable). For the HB use case, callsign without
// suffix is fine.
// ---------------------------------------------------------------------------

static uint64_t pack_alpha_numeric_50(const std::string &call_in) {
    // Filter to [A-Z0-9 /@] and uppercase.
    std::string w;
    for (char c : call_in) {
        char u = (char)std::toupper((unsigned char)c);
        if (alphanum_index(u) >= 0) w += u;
    }
    if (w.size() > 3 && w[3] != '/') w.insert(w.begin() + 3, ' ');
    if (w.size() > 7 && w[7] != '/') w.insert(w.begin() + 7, ' ');
    while (w.size() < 11) w += ' ';
    if (w.size() > 11) w = w.substr(0, 11);

    auto idx = [](char c) -> uint64_t { return (uint64_t)alphanum_index(c); };

    uint64_t a =  (uint64_t)38ULL * 38 * 38 * 2 * 38 * 38 * 38 * 2 * 38 * 38 * idx(w[0]);
    uint64_t b =  (uint64_t)38ULL * 38 * 38 * 2 * 38 * 38 * 38 * 2 * 38      * idx(w[1]);
    uint64_t c =  (uint64_t)38ULL * 38 * 38 * 2 * 38 * 38 * 38 * 2           * idx(w[2]);
    uint64_t d =  (uint64_t)38ULL * 38 * 38 * 2 * 38 * 38 * 38               * (uint64_t)(w[3] == '/');
    uint64_t e =  (uint64_t)38ULL * 38 * 38 * 2 * 38 * 38                    * idx(w[4]);
    uint64_t f =  (uint64_t)38ULL * 38 * 38 * 2 * 38                         * idx(w[5]);
    uint64_t g =  (uint64_t)38ULL * 38 * 38 * 2                              * idx(w[6]);
    uint64_t h =  (uint64_t)38ULL * 38 * 38                                  * (uint64_t)(w[7] == '/');
    uint64_t i =  (uint64_t)38ULL * 38                                       * idx(w[8]);
    uint64_t j =  (uint64_t)38ULL                                            * idx(w[9]);
    uint64_t k =                                                                idx(w[10]);

    return a + b + c + d + e + f + g + h + i + j + k;
}

// ---------------------------------------------------------------------------
// pack_grid is in fate's global namespace; declared at file scope above the
// namespace block to bind the unqualified symbol.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// alphabet64 (the JS8.cpp 64-char alphabet, used by pack72bits-style chunking)
// ---------------------------------------------------------------------------

static const char kAlphabet64[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+";

// ---------------------------------------------------------------------------
// pack72bits_to_chars — port of upstream Varicode::pack72bits.
//
// Given a 64-bit value (high) + 8-bit rem (low) = 72 bits, produce 12 chars
// from kAlphabet64. The mapping mirrors upstream byte for byte.
// ---------------------------------------------------------------------------

void pack72bits_to_chars(uint64_t value, uint8_t rem, char out[12]) {
    const uint8_t mask4 = (1 << 4) - 1;
    const uint8_t mask6 = (1 << 6) - 1;

    uint8_t remHigh = ((value & mask4) << 2) | (rem >> 6);
    uint8_t remLow  =  rem & mask6;
    value >>= 4;

    out[11] = kAlphabet64[remLow];
    out[10] = kAlphabet64[remHigh];

    for (int i = 0; i < 10; i++) {
        out[9 - i] = kAlphabet64[value & mask6];
        value >>= 6;
    }
}

// ---------------------------------------------------------------------------
// build_hb_chars — produce the 12-char message body for a heartbeat frame.
//
// Layout (matches packCompoundFrame for type=FrameHeartbeat=0, hbType=0):
//   value (64 bits) = [flag(3)=000][callsign(50)][packed_11(11)]
//   rem   (8 bits)  = [packed_5(5)][hbType(3)]
// where:
//   packed_11 = (grid >> 5) & 0x7FF
//   packed_5  = grid & 0x1F
//
// hbType: 0 = HB, others reserved for CQ variants. We always send HB.
// ---------------------------------------------------------------------------

bool build_hb_chars(const std::string &callsign,
                    const std::string &grid,
                    char out12[12]) {
    uint64_t cpack = pack_alpha_numeric_50(callsign);
    if (cpack == 0) return false;

    unsigned int g = pack_grid(grid);  // 16-bit grid pack (fate's existing)
    uint16_t packed_11 = (g & ((1 << 11) - 1) << 5) >> 5;  // bits 5..15 of g
    // Wait — upstream does ((num & mask11) >> 5) where mask11 = ((1<<11)-1)<<5,
    // i.e. mask11 = 0xFFE0. So packed_11 = (g & 0xFFE0) >> 5.
    packed_11 = (uint16_t)((g & 0xFFE0) >> 5);
    uint8_t packed_5  = (uint8_t)(g & 0x1F);
    uint8_t hbType    = 0;  // HB
    uint8_t packed_8  = (uint8_t)((packed_5 << 3) | (hbType & 0x7));

    // Concatenate the 64-bit value bits: [flag(3) | callsign(50) | packed_11(11)]
    uint64_t value = ((uint64_t)0 /*FrameHeartbeat=0*/ << 61)
                   | ((uint64_t)cpack & ((1ULL << 50) - 1)) << 11
                   | ((uint64_t)packed_11 & 0x7FF);

    pack72bits_to_chars(value, packed_8, out12);
    return true;
}

}  // namespace x6100js8
