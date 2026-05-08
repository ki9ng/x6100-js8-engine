// Multi-frame smoke test: encode a POTA spot body via x6100js8_encode_text,
// dump as WAV, decode via host/build/decode-test (each 12.64-sec frame
// should round-trip independently, since decode-test scans the whole audio).
//
// Build: g++ -std=c++17 -I../include smoke_text.cc -L../build -lx6100js8 \
//          -Wl,-rpath,../build -o smoke_text
// Run:   ./smoke_text "@APRSIS CMD :POTAGW   :KI9NG US-0765 14225 SSB"

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
    const char *text = argc > 1 ? argv[1]
                                : "@APRSIS CMD :POTAGW   :KI9NG US-0765 14225 SSB";
    const char *outfile = argc > 2 ? argv[2] : "smoke_text.wav";

    int16_t *samples = nullptr;
    size_t   n       = 0;
    int rc = x6100js8_encode_text(text, 1500.0, &samples, &n);
    if (rc != 0) {
        fprintf(stderr, "x6100js8_encode_text failed: rc=%d\n", rc);
        return 1;
    }

    const uint32_t rate = 48000;
    const double seconds = (double)n / (double)rate;
    const double frame_sec = 606720.0 / 48000.0;
    const double frame_estimate = seconds / frame_sec;
    printf("libx6100js8 version=%s\n", x6100js8_version());
    printf("text=\"%s\" (%zu bytes)\n", text, strlen(text));
    printf("encoded %zu samples = %.3f sec (~%.1f frames @ JS8 Normal)\n",
           n, seconds, frame_estimate);

    FILE *f = fopen(outfile, "wb");
    if (!f) { perror(outfile); free(samples); return 1; }

    const uint16_t ch = 1, bps = 16;
    const uint32_t data_bytes = n * sizeof(int16_t);
    fwrite("RIFF", 1, 4, f);  write_le32(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);  write_le32(f, 16);
    write_le16(f, 1); write_le16(f, ch);
    write_le32(f, rate); write_le32(f, rate * ch * bps / 8);
    write_le16(f, ch * bps / 8); write_le16(f, bps);
    fwrite("data", 1, 4, f);  write_le32(f, data_bytes);
    fwrite(samples, sizeof(int16_t), n, f);
    fclose(f);

    free(samples);
    printf("Wrote %s\n", outfile);
    return 0;
}
