// SPDX-License-Identifier: GPL-3.0-or-later
//
// libx6100js8 — public C API.
//
// Phase 3 (JS8Call-interop). Encoders use the lib/js8encode/ frame builders
// and fate's downstream LDPC + recode + fsk pipeline. All output round-trips
// against /usr/bin/js8 + python3-js8py.

#include "x6100js8/x6100js8.h"
#include "../js8encode/js8encode.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace x6100js8 {
extern bool build_hb_chars(const std::string &callsign,
                           const std::string &grid,
                           char out12[12]);
extern bool build_directed_chars(const std::string &from,
                                 const std::string &to,
                                 int                cmd,
                                 char               out12[12]);
extern bool build_data_compressed_chars(const std::string &text,
                                        size_t            &consumed,
                                        char               out12[12]);
}

extern std::vector<double> fsk(std::vector<int> symbols,
                                double hz, double spacing,
                                int rate, int symsamples);

// JS8 Normal: 1920 samples per symbol at 12 kHz = 7680 at 48 kHz.
// 79 symbols/frame × 7680 samples = 606720 samples per frame at 48 kHz.
static constexpr int kRate         = 48000;
static constexpr int kSymsamples   = (1920 * kRate) / 12000;  // 7680
static constexpr double kSpacing   = 6.25;
static constexpr int kSamplesPerFrame = 79 * kSymsamples;     // 606720

static const char *kVersion = "0.4.0";

extern "C" const char *x6100js8_version(void) {
    return kVersion;
}

// Quantise [-1,+1] doubles to int16 PCM with peak normalisation to 0.95 FS.
static int16_t *quantise_pcm(const std::vector<double> &samples) {
    int16_t *buf = (int16_t *)malloc(samples.size() * sizeof(int16_t));
    if (!buf) return nullptr;
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

// Encode 12 alphabet64 chars + i3 to 79 tones, then to PCM samples.
// Returns true on success and appends the samples to `out`.
static bool encode_one_frame(const char message12[12],
                             int  i3,
                             double hz,
                             std::vector<double> &out) {
    int tones[79];
    if (!x6100js8::js8_encode_tones(i3, x6100js8::kCostasNormal, message12, tones)) {
        return false;
    }
    std::vector<int> tonevec(tones, tones + 79);
    auto samp = fsk(tonevec, hz, kSpacing, kRate, kSymsamples);
    if (samp.empty()) return false;
    out.insert(out.end(), samp.begin(), samp.end());
    return true;
}

extern "C" int x6100js8_encode_hb(const char *callsign,
                                  const char *grid,
                                  double      hz,
                                  int16_t   **out_samples,
                                  size_t     *out_n_samples) {
    if (!callsign || !grid || !out_samples || !out_n_samples) return 1;
    if (*out_samples != nullptr || *out_n_samples != 0)       return 2;
    if (strlen(callsign) == 0 || strlen(callsign) > 11)       return 3;
    if (strlen(grid) != 4)                                    return 4;

    char m[12];
    if (!x6100js8::build_hb_chars(callsign, grid, m)) return 5;

    // Single-frame: i3 = JS8CallFirst | JS8CallLast = 1 | 2 = 3
    std::vector<double> all;
    all.reserve(kSamplesPerFrame);
    if (!encode_one_frame(m, 3, hz, all)) return 6;

    int16_t *buf = quantise_pcm(all);
    if (!buf) return 7;
    *out_samples   = buf;
    *out_n_samples = all.size();
    return 0;
}

extern "C" int x6100js8_encode_pota_spot(const char *callsign,
                                         const char *park,
                                         int         freq_khz,
                                         const char *mode,
                                         double      hz,
                                         int16_t   **out_samples,
                                         size_t     *out_n_samples) {
    if (!callsign || !park || !mode || !out_samples || !out_n_samples) return 1;
    if (*out_samples != nullptr || *out_n_samples != 0)                return 2;
    if (freq_khz <= 0 || freq_khz > 99999999)                          return 3;

    // Build the body string: ":POTAGW   :CALL PARK FREQ MODE"
    // POTAGW must be padded to exactly 9 chars (POTAGW + 3 spaces).
    char body[128];
    std::snprintf(body, sizeof(body),
                  ":POTAGW   :%s %s %d %s",
                  callsign, park, freq_khz, mode);

    std::vector<double> all;
    all.reserve(kSamplesPerFrame * 6);  // typical 5-frame message + slack

    // Frame 1: directed @APRSIS CMD frame from KI9NG to @APRSIS, cmd=24 (CMD)
    char m[12];
    if (!x6100js8::build_directed_chars(callsign, "@APRSIS", 24, m)) return 4;
    // i3 = JS8CallFirst (=1) on the first frame
    if (!encode_one_frame(m, 1, hz, all)) return 5;

    // Frames 2..N: data frames carrying chunks of `body`
    std::string remaining(body);
    int safety = 0;
    while (!remaining.empty()) {
        size_t consumed = 0;
        if (!x6100js8::build_data_compressed_chars(remaining, consumed, m)) return 6;
        if (consumed == 0) return 7;  // pathological: no progress
        remaining.erase(0, consumed);

        // i3 = JS8Call (=0) for middle frames, JS8CallLast (=2) for the last.
        int i3 = remaining.empty() ? 2 : 0;
        if (!encode_one_frame(m, i3, hz, all)) return 8;

        if (++safety > 16) return 9;  // pathological-length input
    }

    int16_t *buf = quantise_pcm(all);
    if (!buf) return 10;
    *out_samples   = buf;
    *out_n_samples = all.size();
    return 0;
}
