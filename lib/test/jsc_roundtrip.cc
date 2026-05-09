// Round-trip test: compress a string, decompress, compare. If JSC works
// internally then on-air output will be JS8Call-interoperable since the
// algorithm is byte-for-byte the same as upstream.

#include <cstdio>
#include <string>
#include <vector>
#include "../jsc/jsc.h"

using namespace x6100js8;

int main() {
    const char *cases[] = {
        "POTAGW",
        ":POTAGW   :",
        ":POTAGW   :KI9NG US-0765 14225 SSB",
        "HELLO WORLD",
        "the quick brown fox",
    };
    int fails = 0;
    for (const char *s : cases) {
        std::string in(s);
        auto pairs = JSC::compress(in);

        // Concatenate all codewords for round-trip decompress.
        Codeword all;
        size_t total_chars = 0;
        for (auto &p : pairs) {
            all.insert(all.end(), p.first.begin(), p.first.end());
            total_chars += p.second;
        }

        std::string out = JSC::decompress(all);

        bool ok = (out == in);
        if (!ok) fails++;
        printf("[%s] in=\"%s\" (%zu chars)\n"
               "       %zu pairs, %zu bits, covers %zu chars\n"
               "       out=\"%s\"\n",
               ok ? "OK  " : "FAIL", in.c_str(), in.size(),
               pairs.size(), all.size(), total_chars, out.c_str());
    }
    return fails;
}
