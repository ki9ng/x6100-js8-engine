// Smoke test for libx6100js8: encode an HB, dump as WAV, compare to the
// reference produced by the standalone target binary.
//
// Build standalone (host):
//   g++ -std=c++17 -I../include smoke.cc -L../build -lx6100js8 \
//       -Wl,-rpath,../build -o smoke
//
// Run:
//   ./smoke smoke.wav && cmp smoke.wav ../../target/build/test.wav

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "x6100js8/x6100js8.h"

static void write_le16(FILE *f, uint16_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }
static void write_le32(FILE *f, uint32_t v) {
    fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f);
    fputc((v >> 16) & 0xff, f); fputc((v >> 24) & 0xff, f);
}

int main(int argc, char *argv[]) {
    const char *outfile = argc > 1 ? argv[1] : "smoke.wav";

    int16_t *samples = nullptr;
    size_t   n       = 0;
    int rc = x6100js8_encode_hb("KI9NG", "EM69", 1500.0, &samples, &n);
    if (rc != 0) {
        fprintf(stderr, "x6100js8_encode_hb failed: rc=%d\n", rc);
        return 1;
    }

    printf("libx6100js8 version=%s, encoded %zu samples\n",
           x6100js8_version(), n);

    FILE *f = fopen(outfile, "wb");
    if (!f) { perror(outfile); free(samples); return 1; }

    const uint32_t rate = 48000;
    const uint16_t ch = 1, bps = 16;
    const uint32_t data_bytes = n * sizeof(int16_t);

    fwrite("RIFF", 1, 4, f);
    write_le32(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    write_le32(f, 16);
    write_le16(f, 1);            // PCM
    write_le16(f, ch);
    write_le32(f, rate);
    write_le32(f, rate * ch * bps / 8);
    write_le16(f, ch * bps / 8);
    write_le16(f, bps);
    fwrite("data", 1, 4, f);
    write_le32(f, data_bytes);
    fwrite(samples, sizeof(int16_t), n, f);
    fclose(f);

    free(samples);
    printf("Wrote %s (%zu samples)\n", outfile, n);
    return 0;
}
