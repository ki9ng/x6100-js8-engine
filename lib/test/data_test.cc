// Build a single data frame carrying ":POTAGW   :KI9NG US-0765 14225 SSB"
// (the body of the POTAGW spot, after "@APRSIS CMD "), encode to PCM, decode
// via js8 + js8py, verify it round-trips.

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include "js8encode.h"

namespace x6100js8 {
extern bool build_data_compressed_chars(const std::string &text, size_t &consumed,
                                        char out12[12]);
}
extern std::vector<double> fsk(std::vector<int> symbols, double hz, double spacing,
                                int rate, int symsamples);

static void le16(FILE*f,uint16_t v){fputc(v&0xff,f);fputc((v>>8)&0xff,f);}
static void le32(FILE*f,uint32_t v){fputc(v&0xff,f);fputc((v>>8)&0xff,f);fputc((v>>16)&0xff,f);fputc((v>>24)&0xff,f);}

int main() {
    std::string body = ":POTAGW   :KI9NG US-0765 14225 SSB";
    char m[12];
    size_t consumed = 0;
    if (!x6100js8::build_data_compressed_chars(body, consumed, m)) {
        fprintf(stderr,"build failed\n"); return 1;
    }
    printf("input = \"%s\" (%zu chars)\n", body.c_str(), body.size());
    printf("frame consumed: %zu chars\n", consumed);
    printf("12-char body: \"%.12s\"\n", m);

    int tones[79];
    if (!x6100js8::js8_encode_tones(0 /* i3=0, JS8Call */, x6100js8::kCostasNormal, m, tones)) {
        fprintf(stderr,"encode failed\n"); return 1;
    }
    const int rate=48000, syms=(1920*rate)/12000;
    std::vector<int> tv(tones, tones+79);
    auto samp = fsk(tv, 1500.0, 6.25, rate, syms);
    double mx=0; for(double s:samp) if(std::fabs(s)>mx) mx=std::fabs(s);
    std::vector<int16_t> buf(samp.size());
    for(size_t i=0;i<samp.size();i++) buf[i]=(int16_t)(samp[i]/mx*0.95*32767.0);

    FILE *f=fopen("/tmp/data1.wav","wb");
    uint32_t db=buf.size()*2;
    fwrite("RIFF",1,4,f);le32(f,36+db);fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f);le32(f,16);le16(f,1);le16(f,1);le32(f,rate);
    le32(f,rate*2);le16(f,2);le16(f,16);
    fwrite("data",1,4,f);le32(f,db);fwrite(buf.data(),2,buf.size(),f);fclose(f);
    printf("wrote /tmp/data1.wav\n");
    return 0;
}
