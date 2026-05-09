// SPDX-License-Identifier: GPL-3.0-or-later
//
// JSC (Jordan Sherer Compression) — word-dictionary codec used by JS8Call
// for compressing free-form text into a 72-bit data frame body.
//
// STL port of upstream JS8Call's jsc.h (which uses Qt). The data tables
// (jsc_list.cpp, jsc_map.cpp) are pure const Tuple arrays with no Qt
// references and are reused verbatim — only this header and jsc.cpp
// needed translating from Qt to STL.
//
// Origin: https://github.com/JS8Call-improved/JS8Call-improved (and earlier
//         js8call/js8call) jsc.h, jsc.cpp, jsc_list.cpp, jsc_map.cpp
// Original (C) 2018 Jordan Sherer <kn4crd@gmail.com>
// Released under GPLv3 by Jordan Sherer; this port is also GPLv3.

#ifndef X6100JS8_JSC_H
#define X6100JS8_JSC_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace x6100js8 {

// A bit-vector representation of a JSC codeword.
typedef std::vector<bool> Codeword;

// A (codeword, character-count) pair returned by compress() so the caller
// can tell how many input chars each emitted codeword covers.
typedef std::pair<Codeword, uint32_t> CodewordPair;

// Dictionary entry. ASCII string, length, and the dictionary index.
struct Tuple {
    char const *str;
    int         size;
    int         index;
};

class JSC {
public:
    static Codeword codeword(uint32_t index, bool separate,
                             uint32_t bytesize,
                             uint32_t s, uint32_t c);

    static std::vector<CodewordPair> compress(const std::string &text);
    static std::string               decompress(const Codeword &bits);

    static bool     exists(const std::string &w, uint32_t *pIndex);
    static uint32_t lookup(const std::string &w, bool *ok);
    static uint32_t lookup(const char *b, bool *ok);

    static const uint32_t size       = 262144;
    static const Tuple    map[262144];   // word -> codeword index (sorted by index)
    static const Tuple    list[262144];  // sorted by string for prefix scan
    static const uint32_t prefixSize = 103;
    static const Tuple    prefix[103];   // first-char prefix index into list[]
};

}  // namespace x6100js8

#endif  // X6100JS8_JSC_H
