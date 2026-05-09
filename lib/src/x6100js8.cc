// SPDX-License-Identifier: GPL-3.0-or-later
//
// libx6100js8 — public C API implementation.
//
// Phase 3 (JS8Call-interoperable): builds 87-bit info vectors in JS8Call's
// layout, hands them to fate's LDPC/recode/fsk pipeline, produces audio
// that real JS8Call decoders can read.

#include "x6100js8/x6100js8.h"
#include "../js8encode/js8encode.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace x6100js8 {
extern bool build_hb_chars(const std::string &callsign,
                           const std::string &grid,
                           char out12[12]);
}

// fate's fsk modulator
extern std::vector<double> fsk(std::vector<int> symbols,
                                double hz, double spacing,
                                int rate, int symsamples);

static const char *kVersion = "0.3.0";

extern "C" const char *x6100js8_version(void) {
    return kVersion;
}

// Convert vector of [-1,+1] doubles to int16 PCM, peak-normalised to 0.95 FS.
// Same quantisation as the standalone target binary (verified byte-exact in
// Phase 2). Matters for downstream decoders that don't AGC.
static int16_t *quantise_pcm(const std::vector<double> &samples) {
    int16_t *buf = (int16_t *)malloc(samples.size() * sizeof(int16_t));
    if (!buf) return NULL;
    double mx = 0.0;
    for (double s : samples) {
        double a = std::fabs(s);
        if (a > mx) mx = a;
    }
    if (mx <= 0.0) mx = 1.0;
    for (size_t i = 0; i < samples.size(); i++) {
        double v = (samples[i] / mx) * 0.95;
        if (v >  1.0) v =  1.0;
        if (v < -1.0) v = -1.0;
        buf[i] = (int16_t)lround(v * 32767.0);
    }
    return buf;
}

extern "C" int x6100js8_encode_hb(const char *callsign,
                                  const char *grid,
                                  double      hz,
                                  int16_t   **out_samples,
                                  size_t     *out_n_samples) {
    if (!callsign || !grid || !out_samples || !out_n_samples) return 1;
    if (*out_samples != NULL || *out_n_samples != 0) return 2;
    if (strlen(callsign) == 0 || strlen(callsign) > 11) return 3;
    if (strlen(grid) != 4) return 4;

    char message[12];
    if (!x6100js8::build_hb_chars(callsign, grid, message)) return 5;

    int tones[79];
    if (!x6100js8::js8_encode_tones(0 /*FrameHeartbeat=0*/,
                                    x6100js8::kCostasNormal,
                                    message,
                                    tones)) return 6;

    // Tones -> 48 kHz PCM. JS8 Normal: 1920 samples per symbol at 12 kHz =
    // 7680 samples per symbol at 48 kHz. 6.25 Hz tone spacing.
    const int rate = 48000;
    const int symsamples = (1920 * rate) / 12000;  // = 7680
    std::vector<int> tonevec(tones, tones + 79);
    std::vector<double> samples = fsk(tonevec, hz, 6.25, rate, symsamples);
    if (samples.empty()) return 7;

    int16_t *buf = quantise_pcm(samples);
    if (!buf) return 8;

    *out_samples   = buf;
    *out_n_samples = samples.size();
    return 0;
}

extern "C" int x6100js8_encode_text(const char *text,
                                    double      hz,
                                    int16_t   **out_samples,
                                    size_t     *out_n_samples) {
    // Phase 3 placeholder — the interoperable text-frame builder lives in
    // a follow-up commit that adds packCompressedMessage / packHuffMessage.
    // For now this returns "not implemented" so callers fall back to
    // x6100js8_encode_hb for the radio-side smoke test.
    (void)text; (void)hz; (void)out_samples; (void)out_n_samples;
    return 99;  // ENOSYS-ish
}
