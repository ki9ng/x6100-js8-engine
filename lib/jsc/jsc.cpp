// SPDX-License-Identifier: GPL-3.0-or-later
//
// JSC compression — STL port of upstream JS8Call jsc.cpp.
//
// Same algorithm, same dictionary tables, just translated from Qt
// (QString/QVector/QList/QMap) to std::string / std::vector / std::map.
// The encode/decode logic is byte-identical to JS8Call's so that on-air
// frames produced here are interoperable with every JS8 receiver.
//
// Origin: https://github.com/JS8Call-improved/JS8Call-improved jsc.cpp
// Original (C) 2018 Jordan Sherer <kn4crd@gmail.com>, GPLv3.

#include "jsc.h"

#include <cstring>
#include <map>
#include <sstream>

namespace x6100js8 {

// ---------------------------------------------------------------------------
// Bit packing helpers (ported from Varicode::intToBits / bitsToInt). These
// are also used by the directed-frame builder, so they live in this file
// rather than tucked inside JSC. We re-export them via the header in a
// follow-on commit; for now they're file-local.
// ---------------------------------------------------------------------------

static Codeword intToBits(uint64_t value, int expected) {
    Codeword bits;
    while (value) {
        bits.insert(bits.begin(), (bool)(value & 1));
        value >>= 1;
    }
    if (expected) {
        while ((int)bits.size() < expected) {
            bits.insert(bits.begin(), false);
        }
    }
    return bits;
}

static uint64_t bitsToInt(const Codeword &value) {
    uint64_t v = 0;
    for (bool bit : value) {
        v = (v << 1) + (uint64_t)bit;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Lookup cache. JS8Call uses QMap<QString, quint32>; we use std::map<...>.
// Same semantics: persistent across calls, populated lazily.
// ---------------------------------------------------------------------------

static std::map<std::string, uint32_t> LOOKUP_CACHE;

// ---------------------------------------------------------------------------
// JSC::codeword — exact translation from upstream.
// ---------------------------------------------------------------------------

Codeword JSC::codeword(uint32_t index, bool separate,
                       uint32_t bytesize,
                       uint32_t s, uint32_t c) {
    std::vector<Codeword> out;

    uint32_t v = ((index % s) << 1) + (uint32_t)separate;
    out.insert(out.begin(), intToBits(v, bytesize + 1));

    uint32_t x = index / s;
    while (x > 0) {
        x -= 1;
        out.insert(out.begin(), intToBits((x % c) + s, bytesize));
        x /= c;
    }

    Codeword word;
    for (const auto &w : out) {
        word.insert(word.end(), w.begin(), w.end());
    }
    return word;
}

// ---------------------------------------------------------------------------
// String split helper. Replaces QString::split(" ", Qt::KeepEmptyParts).
// We need *empty* parts preserved so that runs of spaces become explicit
// space tokens in the dictionary lookup loop.
// ---------------------------------------------------------------------------

static std::vector<std::string> split_keep_empty(const std::string &s,
                                                 char delim) {
    std::vector<std::string> out;
    std::string current;
    for (char ch : s) {
        if (ch == delim) {
            out.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    out.push_back(current);
    return out;
}

// ---------------------------------------------------------------------------
// JSC::compress — exact algorithmic translation from upstream.
//
// Splits the input on spaces (keeping empty parts so multi-space runs
// become explicit space tokens), then for each word repeatedly looks up
// the longest prefix in the dictionary and emits a (codeword, char_count)
// pair until the word is consumed. A trailing space is auto-appended
// to the last sub-token of each word unless it's the last word overall
// or the word *is* the space token.
// ---------------------------------------------------------------------------

std::vector<CodewordPair> JSC::compress(const std::string &text) {
    std::vector<CodewordPair> out;

    const uint32_t b = 4;
    const uint32_t s = 7;
    const uint32_t c = 16 - s;  // pow(2, b) - s, b == 4 → 16-7 = 9

    const std::string space = " ";
    std::vector<std::string> words = split_keep_empty(text, ' ');

    for (size_t i = 0, len = words.size(); i < len; i++) {
        std::string w = words[i];

        bool isLastWord = (i == len - 1);
        bool ok = false;
        bool isSpaceCharacter = false;

        if (w.empty() && !isLastWord) {
            w = space;
            isSpaceCharacter = true;
        }

        while (!w.empty()) {
            uint32_t index = lookup(w, &ok);
            if (!ok) break;

            const Tuple &t = JSC::map[index];

            w = w.substr(t.size);

            bool isLast              = w.empty();
            bool shouldAppendSpace   = isLast && !isSpaceCharacter && !isLastWord;

            out.push_back({
                codeword(index, shouldAppendSpace, b, s, c),
                (uint32_t)t.size + (shouldAppendSpace ? 1 : 0)
            });
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// JSC::decompress — used for round-trip testing only on the X6100 (decoder
// path is Phase 4+). Direct translation from upstream.
// ---------------------------------------------------------------------------

std::string JSC::decompress(const Codeword &bitvec) {
    const uint32_t b = 4;
    const uint32_t s = 7;
    const uint32_t c = 16 - s;

    std::vector<std::string> out;
    uint32_t base[8];
    base[0] = 0;
    base[1] = s;
    base[2] = base[1] + s * c;
    base[3] = base[2] + s * c * c;
    base[4] = base[3] + s * c * c * c;
    base[5] = base[4] + s * c * c * c * c;
    base[6] = base[5] + s * c * c * c * c * c;
    base[7] = base[6] + s * c * c * c * c * c * c;

    std::vector<uint64_t> bytes;
    std::vector<uint32_t> separators;

    int i     = 0;
    int count = (int)bitvec.size();
    while (i < count) {
        if (count - i < 4) break;
        Codeword nibble(bitvec.begin() + i, bitvec.begin() + i + 4);
        uint64_t byte = bitsToInt(nibble);
        bytes.push_back(byte);
        i += 4;

        if (byte < s) {
            if (count - i > 0 && bitvec[i]) {
                separators.push_back((uint32_t)bytes.size() - 1);
            }
            i += 1;
        }
    }

    uint32_t start = 0;
    while (start < bytes.size()) {
        uint32_t k = 0;
        uint32_t j = 0;
        while (start + k < bytes.size() && bytes[start + k] >= s) {
            j = j * c + ((uint32_t)bytes[start + k] - s);
            k++;
        }
        if (j >= JSC::size) break;
        if (start + k >= bytes.size()) break;
        j = j * s + (uint32_t)bytes[start + k] + base[k];
        if (j >= JSC::size) break;

        out.emplace_back(JSC::map[j].str);
        if (!separators.empty() && separators.front() == start + k) {
            out.emplace_back(" ");
            separators.erase(separators.begin());
        }
        start = start + (k + 1);
    }

    std::string joined;
    for (const auto &w : out) joined += w;
    return joined;
}

// ---------------------------------------------------------------------------
// JSC::exists — same as upstream.
// ---------------------------------------------------------------------------

bool JSC::exists(const std::string &w, uint32_t *pIndex) {
    bool found     = false;
    uint32_t index = lookup(w, &found);
    if (pIndex) *pIndex = index;
    return found && (size_t)JSC::map[index].size == w.size();
}

// ---------------------------------------------------------------------------
// JSC::lookup(string) — caches positive lookups in LOOKUP_CACHE.
// ---------------------------------------------------------------------------

uint32_t JSC::lookup(const std::string &w, bool *ok) {
    auto it = LOOKUP_CACHE.find(w);
    if (it != LOOKUP_CACHE.end()) {
        if (ok) *ok = true;
        return it->second;
    }

    bool found     = false;
    uint32_t result = lookup(w.c_str(), &found);
    if (found) {
        LOOKUP_CACHE[w] = result;
    }
    if (ok) *ok = found;
    return result;
}

// ---------------------------------------------------------------------------
// JSC::lookup(char const*) — uses the prefix table to jump directly to the
// right slice of list[], then linear-scans for a match. Direct translation
// of upstream; the algorithm is unchanged.
// ---------------------------------------------------------------------------

uint32_t JSC::lookup(const char *b, bool *ok) {
    uint32_t index = 0;
    uint32_t count = 0;
    bool found     = false;

    for (uint32_t i = 0; i < JSC::prefixSize; i++) {
        if (b[0] != JSC::prefix[i].str[0]) continue;

        if (JSC::prefix[i].size == 1) {
            if (ok) *ok = true;
            return JSC::list[JSC::prefix[i].index].index;
        }

        index = JSC::prefix[i].index;
        count = JSC::prefix[i].size;
        found = true;
        break;
    }

    if (!found) {
        if (ok) *ok = false;
        return 0;
    }

    for (uint32_t i = index; i < index + count; i++) {
        uint32_t len = (uint32_t)JSC::list[i].size;
        if (strncmp(b, JSC::list[i].str, len) == 0) {
            if (ok) *ok = true;
            return JSC::list[i].index;
        }
    }

    if (ok) *ok = false;
    return 0;
}

// Re-export the bit packing helpers used by the directed-frame builder.
// They live next to JSC because they're shared with the broader varicode
// port; keeping them in one TU avoids an extra source file just for two
// short functions.
Codeword bits_int_to_bits(uint64_t value, int expected) {
    return intToBits(value, expected);
}
uint64_t bits_to_int(const Codeword &value) {
    return bitsToInt(value);
}

}  // namespace x6100js8
