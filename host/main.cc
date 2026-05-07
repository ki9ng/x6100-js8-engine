// Phase 1 driver: emit a single JS8 heartbeat frame as a WAV file.
//
// Produces: "KI9NG: HB EM69" decodable by JS8Call.
//
// JS8 heartbeat frame layout (87 bits):
//   bits  0-2  : 000  (HB/CQ frame type)
//   bits  3-52 : pack_50(callsign)   50 bits
//   bits 53-68 : grid (16 bits; bit 15 = 0 → HB, 1 → CQ)
//   bits 69-71 : hb_type index (0 = "HB")
//   bits 72-74 : itype (3 = first+last, i.e. complete single-frame message)

#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

#include "fate/pack.h"
#include "fate/util.h"

// Internal functions from pack.cc not exposed in pack.h.
extern unsigned long long pack_50(std::string call);
extern void setbits(int a87[87], int off, int n, unsigned long long x);
extern std::vector<double> pack_any(int a87[87], int rate, double hz);

int main(int argc, char *argv[])
{
    const char *callsign = "KI9NG";
    const char *grid     = "EM69";
    const char *outfile  = "test.wav";
    int   rate           = 48000;
    double hz            = 1500.0;

    if(argc > 1) outfile = argv[1];

    int a87[87];
    memset(a87, 0, sizeof(a87));

    // bits 0-2 are already 000 (HB/CQ type indicator)

    unsigned long long call_bits = pack_50(callsign);
    setbits(a87, 3, 50, call_bits);

    // pack_grid returns a 15-bit value; store it in bits 53-68 (16 bits).
    // Bit 15 of the 16-bit field = 0 → heartbeat (not CQ).
    unsigned int g = pack_grid(grid);
    setbits(a87, 53, 16, (unsigned long long)g);

    // bits 69-71 = 0 → hbs[0] = "HB"
    setbits(a87, 69, 3, 0);

    // itype = 3 (first | last) — complete single-frame message
    setbits(a87, 72, 3, 3);

    std::vector<double> samples = pack_any(a87, rate, hz);

    writewav(samples, outfile, rate);

    printf("Wrote %s: %zu samples, %.3f sec, callsign=%s grid=%s hz=%.1f\n",
           outfile, samples.size(),
           samples.size() / (double)rate,
           callsign, grid, hz);
    printf("Expected decode: \"%s: HB %s\"\n", callsign, grid);

    return 0;
}
