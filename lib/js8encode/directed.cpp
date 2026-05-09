// SPDX-License-Identifier: GPL-3.0-or-later
//
// Directed-frame and data-frame builders for JS8Call interoperability.
//
// Directed frame (i3=011, FrameDirected):
//   72 bits = [flag(3)][from(28)][to(28)][cmd(5)],[portable_from(1)][portable_to(1)][num(6)]
//   then pack72bits → 12 alphabet64 chars → js8_encode_tones (i3=3)
//
// Data frame (i3=100, FrameData):
//   72 bits = [prefix bits][JSC-compressed bits][padding: 0 then 1s]
//   then pack72bits → 12 chars → js8_encode_tones (i3=4)
//
// Multi-frame orchestrator: emit directed frame for "@APRSIS CMD ", then
// data frames for the rest of the message until consumed.
//
// Source: js8call/js8call varicode.cpp, GPL-3.

#include "js8encode.h"
#include "../jsc/jsc.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// fate's pack_call_28 — global namespace.
extern bool pack_call_28(std::string call, unsigned int &x);

namespace x6100js8 {

// ---------------------------------------------------------------------------
// Frame type codes (subset of upstream FrameType enum, lower 3 bits).
// ---------------------------------------------------------------------------

const int kFrameHeartbeat = 0;  // 0b000
const int kFrameCompound  = 1;  // 0b001
const int kFrameDirected  = 3;  // 0b011
const int kFrameData      = 4;  // 0b100

// ---------------------------------------------------------------------------
// nbasecall = 37 * 36 * 10 * 27 * 27 * 27 = 262177560.
// Groupcall codes start at nbasecall + 1 and go up to nbasecall + 54.
// ---------------------------------------------------------------------------

const uint32_t NBASECALL = 262177560u;

static const std::unordered_map<std::string, uint32_t> kBasecalls = {
    { "@ALLCALL",   NBASECALL +  2 },
    { "@JS8NET",    NBASECALL +  3 },
    { "@DX/NA",     NBASECALL +  4 },
    { "@DX/SA",     NBASECALL +  5 },
    { "@DX/EU",     NBASECALL +  6 },
    { "@DX/AS",     NBASECALL +  7 },
    { "@DX/AF",     NBASECALL +  8 },
    { "@DX/OC",     NBASECALL +  9 },
    { "@DX/AN",     NBASECALL + 10 },
    { "@APRSIS",    NBASECALL + 33 },
    { "@RAGCHEW",   NBASECALL + 34 },
    { "@JS8",       NBASECALL + 35 },
    { "@EMCOMM",    NBASECALL + 36 },
    { "@ARES",      NBASECALL + 37 },
    { "@CQ",        NBASECALL + 44 },
    { "@HB",        NBASECALL + 45 },
    { "@SOTA",      NBASECALL + 50 },
    { "@IOTA",      NBASECALL + 51 },
    { "@POTA",      NBASECALL + 52 },
    { "@QRP",       NBASECALL + 53 },
    { "@QRO",       NBASECALL + 54 },
};

// Subset of upstream's directed_cmds. Note the leading-space convention.
// CMD = 24 is what POTAGW spots use. Others kept for future QSO support.
static const std::unordered_map<std::string, int> kDirectedCmds = {
    { " SNR?",     0 },
    { " HW CPY?", 19 },
    { " QSL?",    22 },
    { " QSL",     23 },
    { " CMD",     24 },
    { " 73",      28 },
    { " ACK",     14 },
    { " ",        31 },  // freetext continuation
};

// ---------------------------------------------------------------------------
// pack_callsign — wrap fate's pack_call_28 with groupcall lookup. Returns
// 28-bit packed value, 0 on failure.
// ---------------------------------------------------------------------------

static uint32_t pack_callsign(const std::string &call_in) {
    // Uppercase + trim
    std::string c;
    for (char ch : call_in) {
        if (ch == ' ' || ch == '\t') continue;
        c += (char)std::toupper((unsigned char)ch);
    }
    auto it = kBasecalls.find(c);
    if (it != kBasecalls.end()) return it->second;
    unsigned int x = 0;
    if (!pack_call_28(c, x)) return 0;
    return (uint32_t)x;
}

// ---------------------------------------------------------------------------
// Reuse pack72bits_to_chars from frames.cpp (same TU? no — separate file).
// Re-declare here at file scope; we'll move it to a shared util later.
// ---------------------------------------------------------------------------

extern void pack72bits_to_chars(uint64_t value, uint8_t rem, char out[12]);

// ---------------------------------------------------------------------------
// build_directed_chars — produce the 12-char message body for a directed
// frame (e.g. "KI9NG: @APRSIS CMD ").
// ---------------------------------------------------------------------------

bool build_directed_chars(const std::string &from,
                          const std::string &to,
                          int                cmd,
                          char               out12[12]) {
    uint32_t pfrom = pack_callsign(from);
    uint32_t pto   = pack_callsign(to);
    if (pfrom == 0 || pto == 0) return false;

    // value (64 bits) = [flag(3)=011][from(28)][to(28)][cmd(5)]
    uint64_t value =
          ((uint64_t)kFrameDirected      & 0x7ULL)        << 61
        | ((uint64_t)pfrom              & 0xFFFFFFFULL)  << 33
        | ((uint64_t)pto                & 0xFFFFFFFULL)  << 5
        | ((uint64_t)(cmd % 32)         & 0x1FULL);

    // rem (8 bits) = [portable_from(1)=0][portable_to(1)=0][num(6)=0]
    uint8_t rem = 0;

    pack72bits_to_chars(value, rem, out12);
    return true;
}

}  // namespace x6100js8
