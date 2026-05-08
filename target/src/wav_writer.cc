// SPDX-License-Identifier: MIT
#include "wav_writer.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

namespace {

// Write a little-endian uint32_t to fp. The RIFF spec is little-endian; the
// X6100's A33 (and our build host) are both little-endian too, so this is
// portable enough for our purposes — but explicit byte writes mean nothing
// surprises us if a future build target changes endianness.
void w_u32_le(FILE *fp, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v        & 0xFF),
        (uint8_t)((v >> 8)  & 0xFF),
        (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 24) & 0xFF),
    };
    fwrite(b, 1, 4, fp);
}

void w_u16_le(FILE *fp, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF) };
    fwrite(b, 1, 2, fp);
}

} // anon

int wav_write_mono16(const std::vector<double> &samples, const char *filename,
                     int rate)
{
    // Find peak (matches fate's writewav() — normalise to 0.95 FS so we
    // don't clip and downstream decoders see the same drive level).
    double mx = 0.0;
    for (size_t i = 0; i < samples.size(); i++) {
        double a = fabs(samples[i]);
        if (a > mx) mx = a;
    }
    if (mx <= 0.0) mx = 1.0;  // degenerate all-zero input

    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;

    const uint32_t n        = (uint32_t)samples.size();
    const uint32_t data_sz  = n * 2;            // 16-bit mono
    const uint32_t riff_sz  = 36 + data_sz;     // file size minus the 8-byte RIFF header

    // RIFF header
    fwrite("RIFF", 1, 4, fp);
    w_u32_le(fp, riff_sz);
    fwrite("WAVE", 1, 4, fp);

    // fmt sub-chunk
    fwrite("fmt ", 1, 4, fp);
    w_u32_le(fp, 16);                  // sub-chunk size for PCM
    w_u16_le(fp, 1);                   // PCM format
    w_u16_le(fp, 1);                   // mono
    w_u32_le(fp, (uint32_t)rate);      // sample rate
    w_u32_le(fp, (uint32_t)rate * 2);  // byte rate (rate * channels * bytes/sample)
    w_u16_le(fp, 2);                   // block align (channels * bytes/sample)
    w_u16_le(fp, 16);                  // bits per sample

    // data sub-chunk
    fwrite("data", 1, 4, fp);
    w_u32_le(fp, data_sz);

    for (size_t i = 0; i < samples.size(); i++) {
        double v = (samples[i] / mx) * 0.95;     // normalise to 95% FS
        if (v >  1.0) v =  1.0;                  // belt-and-braces clip
        if (v < -1.0) v = -1.0;
        int16_t s = (int16_t)lround(v * 32767.0);
        w_u16_le(fp, (uint16_t)s);
    }

    if (fclose(fp) != 0) return -2;
    return 0;
}
