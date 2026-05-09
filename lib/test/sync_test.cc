// Minimal end-to-end test: skip all the HB-specific packing logic, just
// hand js8_encode_tones a 12-char message and see if the resulting audio
// is at least syncable by JS8Call's decoder. Tells us whether the bit
// ordering inside the alphabet64 packing is correct, isolated from the
// HB-frame layout.
//
// Build (from lib/test/):
//   g++ -std=c++17 -I../js8encode -I../include sync_test.cc \
//     ../build/CMakeFiles/x6100js8.dir/js8encode/js8encode.cpp.o \
//     -L../build -lx6100js8 -Wl,-rpath,../build -o sync_test

#include <cstdio>
#include <cstdint>
#include <vector>
#include "js8encode.h"
#include <cmath>

extern std::vector<double> fsk(std::vector<int> symbols,
                                double hz, double spacing,
                                int rate, int symsamples);

static void write_le16(FILE *f, uint16_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }
static void write_le32(FILE *f, uint32_t v) {
    fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f);
    fputc((v >> 16) & 0xff, f); fputc((v >> 24) & 0xff, f);
}

int main() {
    // Pick a 12-char message of valid alphabet64 chars. Use "AAAAAAAAAAAA"
    // for maximum simplicity — we don't expect this to *decode* to anything
    // sensible, but the Costas blocks should still sync since they're set
    // independently of the message.
    const char *msg = "AAAAAAAAAAAA";
    int tones[79];
    if (!x6100js8::js8_encode_tones(0, x6100js8::kCostasNormal, msg, tones)) {
        fprintf(stderr, "encode failed\n");
        return 1;
    }

    printf("first 7 tones (Costas A): ");
    for (int i = 0; i < 7; i++) printf("%d ", tones[i]);
    printf("\n");
    printf("tones 36-42 (Costas B):   ");
    for (int i = 36; i < 43; i++) printf("%d ", tones[i]);
    printf("\n");
    printf("tones 72-78 (Costas C):   ");
    for (int i = 72; i < 79; i++) printf("%d ", tones[i]);
    printf("\n");

    const int rate = 48000;
    const int symsamples = (1920 * rate) / 12000;
    std::vector<int> tonevec(tones, tones + 79);
    auto samples = fsk(tonevec, 1500.0, 6.25, rate, symsamples);

    // Write WAV.
    FILE *f = fopen("/tmp/sync_test.wav", "wb");
    const uint32_t r = 48000;
    const uint16_t ch = 1, bps = 16;
    const size_t n = samples.size();
    std::vector<int16_t> buf(n);
    double mx = 0;
    for (double s : samples) if (std::abs(s) > mx) mx = std::abs(s);
    for (size_t i = 0; i < n; i++)
        buf[i] = (int16_t)(samples[i] / mx * 0.95 * 32767.0);

    const uint32_t db = n * 2;
    fwrite("RIFF", 1, 4, f); write_le32(f, 36 + db);
    fwrite("WAVE", 1, 4, f); fwrite("fmt ", 1, 4, f); write_le32(f, 16);
    write_le16(f, 1); write_le16(f, ch); write_le32(f, r);
    write_le32(f, r * ch * bps / 8); write_le16(f, ch * bps / 8); write_le16(f, bps);
    fwrite("data", 1, 4, f); write_le32(f, db);
    fwrite(buf.data(), 2, n, f);
    fclose(f);
    printf("Wrote /tmp/sync_test.wav (%zu samples)\n", n);
    return 0;
}
