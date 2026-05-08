// SPDX-License-Identifier: MIT
//
// libx6100js8 — public C API implementation.
//
// Wraps fate's encoder primitives (pack_50, setbits, pack_any, pack_grid)
// in a malloc-out-buffer C interface suitable for x6100_gui to call.
//
// JS8 heartbeat frame layout (87 bits) mirrors fate's pack.cc::pack_directed
// for a heartbeat / hb_type == 0:
//
//   bits  0-2  : 000  (HB/CQ frame type)
//   bits  3-52 : pack_50(callsign)   50 bits
//   bits 53-68 : grid (16 bits; bit 15 = 0 → HB, 1 → CQ)
//   bits 69-71 : hb_type index (0 = "HB")
//   bits 72-74 : itype (3 = first|last, complete single-frame message)
//
// Output is 48 kHz int16 mono PCM, peak-normalised to 0.95 FS — matches
// wav_writer.cc's quantisation byte-for-byte (verified by lib/test/smoke).

#include "x6100js8/x6100js8.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "fate/pack.h"
#include "fate/util.h"

// pack.cc internals — same extern declarations the host driver uses.
extern unsigned long long pack_50(std::string call);
extern void setbits(int a87[87], int off, int n, unsigned long long x);
extern std::vector<double> pack_any(int a87[87], int rate, double hz);

static const char *kVersion = "0.1.0";

extern "C" const char *x6100js8_version(void) {
    return kVersion;
}

extern "C" int x6100js8_encode_hb(const char *callsign,
                                  const char *grid,
                                  double      hz,
                                  int16_t   **out_samples,
                                  size_t     *out_n_samples) {
    if (!callsign || !grid || !out_samples || !out_n_samples) return 1;
    if (*out_samples != NULL || *out_n_samples != 0) return 2;

    // Sanity-check inputs before going through fate's bit packers, which
    // assert rather than report errors on malformed input.
    if (strlen(callsign) == 0 || strlen(callsign) > 11) return 3;
    if (strlen(grid) != 4) return 4;

    const int rate = 48000;

    int a87[87];
    memset(a87, 0, sizeof(a87));

    // bits 0-2 = 000 (HB/CQ type indicator)
    unsigned long long call_bits = pack_50(std::string(callsign));
    setbits(a87, 3, 50, call_bits);

    unsigned int g = pack_grid(std::string(grid));
    setbits(a87, 53, 16, (unsigned long long)g);

    setbits(a87, 69, 3, 0);   // hbs[0] = "HB"
    setbits(a87, 72, 3, 3);   // itype = 3 (first|last) — complete single-frame

    std::vector<double> samples = pack_any(a87, rate, hz);
    if (samples.empty()) return 5;

    int16_t *buf = (int16_t *)malloc(samples.size() * sizeof(int16_t));
    if (!buf) return 6;

    // Match wav_writer.cc's peak-normalise-to-0.95-FS quantisation exactly
    // so the library's PCM is bit-for-bit identical to the standalone
    // target binary's WAV output (verified by smoke test under lib/test/).
    double mx = 0.0;
    for (size_t i = 0; i < samples.size(); i++) {
        double a = std::fabs(samples[i]);
        if (a > mx) mx = a;
    }
    if (mx <= 0.0) mx = 1.0;
    for (size_t i = 0; i < samples.size(); i++) {
        double v = (samples[i] / mx) * 0.95;
        if (v >  1.0) v =  1.0;
        if (v < -1.0) v = -1.0;
        buf[i] = (int16_t)lround(v * 32767.0);
    }

    *out_samples   = buf;
    *out_n_samples = samples.size();
    return 0;
}
