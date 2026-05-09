// SPDX-License-Identifier: GPL-3.0-or-later
//
// Data-frame builder for JS8Call interoperability.
//
// Frame body layout (72 bits) per upstream packCompressedMessage:
//   bit 0    = 1  (data-frame flag)
//   bit 1    = 1  (compressed) or 0 (huffman)
//   bits 2.. = JSC-compressed payload (whole codeword pairs only;
//              we never split a pair across frames)
//   bit n    = 0  (end-of-payload marker)
//   bits n+1..71 = 1  (padding)
//
// The receiver reverses this by scanning backwards from bit 71 for the
// last 0 bit; everything before that 0 is content.
//
// Source: js8call/js8call varicode.cpp packCompressedMessage, GPL-3.

#include "js8encode.h"
#include "../jsc/jsc.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace x6100js8 {

extern void pack72bits_to_chars(uint64_t value, uint8_t rem, char out[12]);

// Pack a 72-bit Codeword (vector<bool>) into (uint64_t value, uint8_t rem)
// using upstream's bit-order convention: bits[0..63] -> value (MSB first),
// bits[64..71] -> rem (MSB first).
static void codeword_to_value_rem(const Codeword &b, uint64_t &value, uint8_t &rem) {
    value = 0;
    for (int i = 0; i < 64; i++) {
        value = (value << 1) | (b[i] ? 1 : 0);
    }
    rem = 0;
    for (int i = 64; i < 72; i++) {
        rem = (uint8_t)((rem << 1) | (b[i] ? 1 : 0));
    }
}

// ---------------------------------------------------------------------------
// build_data_compressed_chars — emit one 12-char data frame carrying as
// much of `text` as fits, return the number of input chars consumed.
// `consumed` is set to the count.
// Returns true on success, false on JSC failure (e.g. unencodable char).
// ---------------------------------------------------------------------------

bool build_data_compressed_chars(const std::string &text,
                                 size_t            &consumed,
                                 char               out12[12]) {
    consumed = 0;
    if (text.empty()) return false;

    // Compress the entire input. We'll then greedily append codeword pairs
    // until the next pair won't fit (mirrors upstream packCompressedMessage).
    auto pairs = JSC::compress(text);
    if (pairs.empty()) return false;

    Codeword body;
    body.reserve(72);
    body.push_back(true);   // bit 0: data
    body.push_back(true);   // bit 1: compressed

    size_t total_chars = 0;
    for (const auto &p : pairs) {
        const Codeword &bits = p.first;
        if (body.size() + bits.size() < 72) {
            body.insert(body.end(), bits.begin(), bits.end());
            total_chars += p.second;
        } else {
            break;
        }
    }

    if (total_chars == 0) return false;  // not even one pair fit

    // Pad: first pad bit = 0, rest = 1, until 72 total.
    bool first_pad = true;
    while (body.size() < 72) {
        body.push_back(first_pad ? false : true);
        first_pad = false;
    }

    uint64_t value;
    uint8_t  rem;
    codeword_to_value_rem(body, value, rem);
    pack72bits_to_chars(value, rem, out12);
    consumed = total_chars;
    return true;
}

}  // namespace x6100js8
