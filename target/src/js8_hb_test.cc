// SPDX-License-Identifier: MIT
//
// Phase 2 driver: emit a single JS8 heartbeat frame as a WAV file.
// Cross-compiled for ARMv7 (X6100). No libsndfile, no fftw3 — TX-only path.
//
// Produces "<CALLSIGN>: HB <GRID>" decodable by JS8Call.
//
// JS8 heartbeat frame layout (87 bits) — mirrors fate's pack.cc::pack_directed
// for a heartbeat / hb_type == 0:
//
//   bits  0-2  : 000  (HB/CQ frame type)
//   bits  3-52 : pack_50(callsign)   50 bits
//   bits 53-68 : grid (16 bits; bit 15 = 0 → HB, 1 → CQ)
//   bits 69-71 : hb_type index (0 = "HB")
//   bits 72-74 : itype (3 = first|last, complete single-frame message)
//
// Defaults to KI9NG / EM69 to match the host-side reference WAV bit-for-bit
// when invoked with no arguments — that's our sanity check that the cross-
// compiled engine matches the host build.

#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

#include "fate/pack.h"
#include "fate/util.h"
#include "wav_writer.h"

// pack.cc internals not exposed in pack.h — same extern declarations the
// host driver uses.
extern unsigned long long pack_50(std::string call);
extern void setbits(int a87[87], int off, int n, unsigned long long x);
extern std::vector<double> pack_any(int a87[87], int rate, double hz);

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [output.wav] [callsign] [grid]\n"
        "  output.wav  default test.wav\n"
        "  callsign    default KI9NG\n"
        "  grid        default EM69 (4-char Maidenhead)\n",
        prog);
}

int main(int argc, char *argv[]) {
    const char *outfile  = "test.wav";
    const char *callsign = "KI9NG";
    const char *grid     = "EM69";
    int   rate = 48000;
    double hz  = 1500.0;

    if (argc > 1 && strcmp(argv[1], "-h") == 0) { usage(argv[0]); return 0; }
    if (argc > 1) outfile  = argv[1];
    if (argc > 2) callsign = argv[2];
    if (argc > 3) grid     = argv[3];

    int a87[87];
    memset(a87, 0, sizeof(a87));

    // bits 0-2 = 000 (HB/CQ type indicator)

    unsigned long long call_bits = pack_50(callsign);
    setbits(a87, 3, 50, call_bits);

    unsigned int g = pack_grid(grid);
    setbits(a87, 53, 16, (unsigned long long)g);

    setbits(a87, 69, 3, 0);    // hbs[0] = "HB"
    setbits(a87, 72, 3, 3);    // itype = 3 (first|last) — complete single-frame

    std::vector<double> samples = pack_any(a87, rate, hz);

    if (wav_write_mono16(samples, outfile, rate) != 0) {
        fprintf(stderr, "%s: failed to write %s\n", argv[0], outfile);
        return 1;
    }

    printf("Wrote %s: %zu samples, %.3f sec, callsign=%s grid=%s hz=%.1f\n",
           outfile, samples.size(),
           samples.size() / (double)rate,
           callsign, grid, hz);
    printf("Expected decode: \"%s: HB %s\"\n", callsign, grid);
    return 0;
}
