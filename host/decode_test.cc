//
// Minimal JS8 decode test — no TTY or curses required.
// Loads a WAV file and calls fate's entry() directly, printing decoded messages.
//
#include <stdio.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <complex>
#include "fate/util.h"
#include "fate/fft.h"
#include "fate/js8.h"
#include "fate/defs.h"

extern std::string unpack(const int a87[87], std::string &other_call);

static int decode_cb(int *a87, double hz0, double hz1, double off,
                     const char *comment, double snr, int pass, int correct_bits)
{
    std::string other_call;
    std::string txt = unpack(a87, other_call);
    printf("DECODE hz=%.1f off=%.3fs snr=%.1f dB: %s\n",
           hz0, off, snr, txt.c_str());
    fflush(stdout);
    return 2;
}

int main(int argc, char *argv[])
{
    const char *wavfile = (argc > 1) ? argv[1] : "test6k.wav";

    int rate = 0;
    std::vector<double> samples = readwav(wavfile, rate);
    if (samples.empty()) {
        fprintf(stderr, "Failed to read %s\n", wavfile);
        return 1;
    }
    printf("Loaded %zu samples at %d Hz (%.3f sec)\n",
           samples.size(), rate, samples.size() / (double)rate);

    // Prepend 2.5 seconds of silence so the frame has context before it.
    int pad = (5 * rate) / 2;
    std::vector<double> padded(pad, 0.0);
    padded.insert(padded.end(), samples.begin(), samples.end());

    int hints[2] = { 0, 0 };

    entry(padded.data(), (int)padded.size(), pad, rate,
          200.0, 4000.0,
          hints, hints,
          decode_cb);

    return 0;
}
