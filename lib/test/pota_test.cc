// Full POTA-spot test. Encode "KI9NG US-0765 14225 SSB" via the public
// API, decode every frame via js8 + js8py, verify the message round-trips.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "x6100js8/x6100js8.h"

static void le16(FILE*f,uint16_t v){fputc(v&0xff,f);fputc((v>>8)&0xff,f);}
static void le32(FILE*f,uint32_t v){fputc(v&0xff,f);fputc((v>>8)&0xff,f);fputc((v>>16)&0xff,f);fputc((v>>24)&0xff,f);}

int main() {
    int16_t *samples = nullptr;
    size_t   n       = 0;
    int rc = x6100js8_encode_pota_spot("KI9NG", "US-0765", 14225, "SSB",
                                       1500.0, &samples, &n);
    if (rc) { fprintf(stderr,"encode failed rc=%d\n",rc); return 1; }
    printf("version=%s\n", x6100js8_version());
    printf("encoded %zu samples = %.2f sec = %.1f frames\n",
           n, (double)n / 48000.0, (double)n / 606720.0);

    FILE *f = fopen("/tmp/pota.wav","wb");
    uint32_t db = n * 2;
    fwrite("RIFF",1,4,f);le32(f,36+db);fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f);le32(f,16);le16(f,1);le16(f,1);le32(f,48000);
    le32(f,48000*2);le16(f,2);le16(f,16);
    fwrite("data",1,4,f);le32(f,db);fwrite(samples,2,n,f);fclose(f);
    free(samples);
    printf("wrote /tmp/pota.wav\n");
    return 0;
}
