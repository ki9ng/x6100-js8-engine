// SPDX-License-Identifier: GPL-3.0-or-later
//
// libx6100js8 — public C API.
//
// Phase 3 (JS8Call-interop). Encoders use the lib/js8encode/ frame builders
// and fate's downstream LDPC + recode + fsk pipeline. All output round-trips
// against /usr/bin/js8 + python3-js8py.
//
// Architecture note (2026-05-09): the encoder produces one frame per call,
// with NO inter-frame silence. Silence / slot-boundary timing is the
// scheduler's responsibility (dialog_pota_spot.c). This avoids the class
// of bugs where the caller must guess frame boundaries inside a padded
// buffer, and lets the scheduler use clock_gettime to drive PTT precisely.

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

// JS8 Normal mode constants — 48 kHz output.
// No inter-frame silence is generated here; the scheduler handles gaps.
static constexpr int    kRate             = 48000;
static constexpr int    kSymsamples       = (1920 * kRate) / 12000;  // 7680
static constexpr double kSpacing          = 6.25;
static constexpr int    kSamplesPerFrame  = 79 * kSymsamples;        // 606720 (12.64 sec)

static const char *kVersion = "0.5.0";

extern "C" const char *x6100js8_version(void) {
    return kVersion;
}

// ─── internal helpers ────────────────────────────────────────────────────────

// Encode one 79-symbol JS8 frame and append quantised int16 PCM to *frames*
// as a new entry. i3 selects frame type: 1=first, 0=middle, 2=last.
static bool encode_one_frame(const char m[12], int i3, double hz,
                             std::vector<std::vector<int16_t>> &frames) {
    std::vector<int> syms = x6100js8::encode_frame(m, i3);
    if (syms.empty()) return false;

    std::vector<double> pcm = fsk(syms, hz, kSpacing, kRate, kSymsamples);
    if ((int)pcm.size() != kSamplesPerFrame) return false;

    std::vector<int16_t> frame_pcm(kSamplesPerFrame);
    for (int i = 0; i < kSamplesPerFrame; i++) {
        double s = pcm[i];
        if (s >  1.0) s =  1.0;
        if (s < -1.0) s = -1.0;
        frame_pcm[i] = (int16_t)(s * 32767.0);
    }
    frames.push_back(std::move(frame_pcm));
    return true;
}

// ─── x6100js8_msg_t ──────────────────────────────────────────────────────────

struct x6100js8_msg {
    std::vector<std::vector<int16_t>> frames;
};

extern "C" int x6100js8_msg_frame_count(const x6100js8_msg_t *msg) {
    if (!msg) return 0;
    return (int)msg->frames.size();
}

extern "C" const int16_t *x6100js8_msg_frame(const x6100js8_msg_t *msg,
                                              int i, size_t *n) {
    if (!msg || i < 0 || i >= (int)msg->frames.size()) return nullptr;
    *n = msg->frames[i].size();
    return msg->frames[i].data();
}

extern "C" void x6100js8_msg_free(x6100js8_msg_t *msg) {
    delete msg;
}

// ─── encode_hb ───────────────────────────────────────────────────────────────

extern "C" int x6100js8_encode_hb(const char *callsign,
                                   const char *grid,
                                   double      hz,
                                   int16_t   **out_samples,
                                   size_t     *out_n_samples) {
    if (!callsign || !grid || !out_samples || !out_n_samples) return 1;
    *out_samples   = nullptr;
    *out_n_samples = 0;

    char m[12];
    if (!x6100js8::build_hb_chars(callsign, grid, m)) return 2;

    std::vector<int> syms = x6100js8::encode_frame(m, 0);
    if (syms.empty()) return 3;

    std::vector<double> pcm = fsk(syms, hz, kSpacing, kRate, kSymsamples);
    if ((int)pcm.size() != kSamplesPerFrame) return 4;

    int16_t *buf = (int16_t *)malloc(kSamplesPerFrame * sizeof(int16_t));
    if (!buf) return 5;

    for (int i = 0; i < kSamplesPerFrame; i++) {
        double s = pcm[i];
        if (s >  1.0) s =  1.0;
        if (s < -1.0) s = -1.0;
        buf[i] = (int16_t)(s * 32767.0);
    }

    *out_samples   = buf;
    *out_n_samples = (size_t)kSamplesPerFrame;
    return 0;
}

// ─── encode_pota_spot ────────────────────────────────────────────────────────

extern "C" int x6100js8_encode_pota_spot(const char      *callsign,
                                          const char      *park,
                                          double           freq_mhz,
                                          const char      *mode,
                                          double           hz,
                                          x6100js8_msg_t **msg_out) {
    if (!callsign || !park || !mode || !msg_out) return 1;
    if (freq_mhz <= 0.0 || freq_mhz > 999.0)   return 2;
    *msg_out = nullptr;

    // Build body: "! POTA <park> <freq_mhz> <mode>"
    // APSPOT confirmed wire format from KI9NG live test 2026-05-09.
    char body[80];
    std::snprintf(body, sizeof(body),
                  ":APSPOT   :! POTA %s %.3f %s",
                  park, freq_mhz, mode);

    auto *msg = new x6100js8_msg;

    // Frame 1: directed @APRSIS CMD frame (i3=1, JS8CallFirst)
    char m[12];
    if (!x6100js8::build_directed_chars(callsign, "@APRSIS", 24, m)) {
        delete msg; return 3;
    }
    if (!encode_one_frame(m, 1, hz, msg->frames)) {
        delete msg; return 4;
    }

    // Frames 2..N: data frames carrying body, no silence between them here
    std::string remaining(body);
    int safety = 0;
    while (!remaining.empty()) {
        size_t consumed = 0;
        if (!x6100js8::build_data_compressed_chars(remaining, consumed, m)) {
            delete msg; return 5;
        }
        if (consumed == 0) { delete msg; return 6; }
        remaining.erase(0, consumed);

        int i3 = remaining.empty() ? 2 : 0;  // 2=last, 0=middle
        if (!encode_one_frame(m, i3, hz, msg->frames)) {
            delete msg; return 7;
        }
        if (++safety > 16) { delete msg; return 8; }
    }

    *msg_out = msg;
    return 0;
}

// ─── encode_directed ─────────────────────────────────────────────────────────

extern "C" int x6100js8_encode_directed(const char      *callsign,
                                         const char      *to,
                                         const char      *text,
                                         double           hz,
                                         x6100js8_msg_t **msg_out) {
    if (!callsign || !to || !text || !msg_out) return 1;
    *msg_out = nullptr;

    auto *msg = new x6100js8_msg;

    // Frame 1: directed header frame (i3=1, JS8CallFirst)
    char m[12];
    if (!x6100js8::build_directed_chars(callsign, to, 0, m)) {
        delete msg; return 2;
    }
    if (!encode_one_frame(m, 1, hz, msg->frames)) {
        delete msg; return 3;
    }

    // Frames 2..N: compressed data frames carrying text
    std::string remaining(text);
    int safety = 0;
    while (!remaining.empty()) {
        size_t consumed = 0;
        if (!x6100js8::build_data_compressed_chars(remaining, consumed, m)) {
            delete msg; return 4;
        }
        if (consumed == 0) { delete msg; return 5; }
        remaining.erase(0, consumed);

        int i3 = remaining.empty() ? 2 : 0;
        if (!encode_one_frame(m, i3, hz, msg->frames)) {
            delete msg; return 6;
        }
        if (++safety > 16) { delete msg; return 7; }
    }

    *msg_out = msg;
    return 0;
}
