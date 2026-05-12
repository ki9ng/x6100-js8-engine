# x6100-js8-engine / libx6100js8

A JS8Call-interoperable JS8 protocol encoder for embedded Linux, targeting the **Xiegu X6100** transceiver. The core deliverable is **libx6100js8** — a shared C library that encodes JS8 directed messages into 48 kHz PCM frames ready for transmission.

Built on [rtmrtmrtmrtm/fate](https://github.com/rtmrtmrtmrtm/fate) — a minimal pure-C++ JS8 implementation by Robert Morris AB1HL. All encoder output is verified decodable by a stock JS8Call installation.

---

## What this is

The X6100 runs a custom Linux firmware (`x6100_gui`) that controls the radio hardware directly. This library gives that firmware the ability to encode and transmit JS8 messages — specifically POTA self-spots via APRS-IS, text messages to other stations, and APRS position beacons — without requiring a running JS8Call application.

**The library does one thing**: take a message and produce PCM frames. Slot timing, PTT control, and audio output are the caller's responsibility.

---

## Architecture

```
x6100_gui (firmware process)
    │
    ├── aether_x6100_control   — PTT, VFO, mode via STM32 co-processor
    ├── PulseAudio             — audio output → TX audio chain
    └── libx6100js8            — JS8 encoder (this library)
            │
            ├── js8encode/     — JS8Call-compatible 87-bit info vector + frame builders
            ├── jsc/           — JSC word-dictionary compression for data frames
            └── fate/          — LDPC encoder, recode, FSK modulator (from rtmrtmrtm/fate)
```

The firmware calls `x6100js8_encode_pota_spot()` (or `x6100js8_encode_directed()`), gets back an `x6100js8_msg_t` handle containing one PCM frame per JS8 slot, then drives the TX sequence:

```
for each frame:
    wait for 15-sec slot boundary (0, 15, 30, 45 sec UTC)
    key PTT
    wait 500 ms  (JS8A_START_DELAY_MS — PA/relay settling)
    play frame   (exactly 606720 samples @ 48 kHz = 12.64 sec)
    drop PTT
    (2.36 sec dead air until next slot boundary)
```

---

## JS8 Normal mode constants

| Constant | Value | Meaning |
|---|---|---|
| `X6100JS8_SAMPLE_RATE` | 48000 | Output sample rate (Hz) |
| `X6100JS8_SYMBOLS_PER_FRAME` | 79 | FSK-8 symbols per frame |
| `X6100JS8_SAMPLES_PER_SYMBOL` | 7680 | Samples per symbol at 48 kHz |
| `X6100JS8_SAMPLES_PER_FRAME` | 606720 | Samples per frame (79 × 7680) |
| `X6100JS8_FRAME_DURATION_MS` | 12640 | Frame duration (12.64 sec) |
| `X6100JS8_SLOT_PERIOD_MS` | 15000 | Slot period (15.0 sec) |
| `X6100JS8_PTT_DELAY_MS` | 500 | PTT-to-audio delay (JS8A_START_DELAY_MS) |
| `X6100JS8_LATE_WINDOW_MS` | 2160 | Max late-start window for immediate TX |

Frame duration is **symbol-count derived**, not time-based: 79 symbols × 1920 samples/symbol (at 12 kHz) × (48000/12000 resample ratio) = 606720 samples. Do not use a time-based sleep to define end-of-frame; drain exactly 606720 samples from the audio buffer.

The **late-start window** (2160 ms) matches JS8Call's `lateThreshold` logic: if the current time is within 2.16 sec of a slot boundary, fire immediately rather than waiting for the next slot. This is derived from `(slot_period - frame_duration - tx_delay) = (15.0 - 12.64 - 0.2) = 2.16 sec`.

---

## Public API

```c
#include <x6100js8/x6100js8.h>
```

### Message handle

```c
// Opaque handle. One entry per JS8 frame, no inter-frame silence.
typedef struct x6100js8_msg x6100js8_msg_t;

int            x6100js8_msg_frame_count(const x6100js8_msg_t *msg);
const int16_t *x6100js8_msg_frame(const x6100js8_msg_t *msg, int i, size_t *n);
void           x6100js8_msg_free(x6100js8_msg_t *msg);
```

Each frame returned by `x6100js8_msg_frame()` is exactly `X6100JS8_SAMPLES_PER_FRAME` (606720) samples of 48 kHz 16-bit mono PCM. **No inter-frame silence is included** — the gap between frames is the scheduler's responsibility.

### POTA self-spot

```c
int x6100js8_encode_pota_spot(
    const char      *callsign,   // e.g. "KI9NG"
    const char      *park,       // e.g. "US-0765"
    double           freq_mhz,   // dial frequency in MHz, e.g. 14.225
    const char      *mode,       // "SSB" | "CW" | "FM" | "USB" | "LSB" | "DATA"
    double           hz,         // audio tone offset in Hz, typically 1500.0
    x6100js8_msg_t **msg         // out: caller must free with x6100js8_msg_free()
);
```

Builds and encodes: `@APRSIS CMD :APSPOT   :! POTA <park> <freq_mhz> <mode>`

Wire format confirmed working by KI9NG live test (2026-05-09). APSPOT identifies the activator from the APRS source callsign in the packet forwarded by the JS8Call gateway — the callsign does not appear in the message body. Frequency is dial frequency in MHz with 3 decimal places (`14.225`, not `14225`).

A typical POTA spot body (`! POTA US-0765 14.225 SSB`) encodes to **4–5 JS8 frames** (60–75 sec on-air).

### Directed message

```c
int x6100js8_encode_directed(
    const char      *callsign,   // sender, e.g. "KI9NG"
    const char      *to,         // destination, e.g. "W9PRK" or "@ALLCALL"
    const char      *text,       // message body
    double           hz,         // audio tone offset, typically 1500.0
    x6100js8_msg_t **msg         // out: caller must free
);
```

Encodes an arbitrary directed JS8 message: `KI9NG: <to> <text>`

Useful for the prestored text messaging feature. The `to` field accepts any JS8 destination including `@ALLCALL`, `@APRSIS`, and individual callsigns.

### Heartbeat (single frame)

```c
int x6100js8_encode_hb(
    const char *callsign,       // e.g. "KI9NG"
    const char *grid,           // Maidenhead grid, 4–10 chars, e.g. "EM69ab"
    double      hz,             // audio tone offset, typically 1500.0
    int16_t   **out_samples,    // out: malloc'd buffer, caller must free()
    size_t     *out_n_samples   // out: always X6100JS8_SAMPLES_PER_FRAME on success
);
```

Encodes a single JS8 heartbeat frame: `KI9NG: HB EM69ab`

Single frame only — returns a flat `int16_t*` rather than an `x6100js8_msg_t`. 6-char grid confirmed working on APRS-IS (KI9NG live test 2026-05-09); use more chars if GPS provides them.

### Return codes

All functions return 0 on success, nonzero on failure. Nonzero values indicate internal encode failures; the specific value identifies the failing stage but is not part of the stable API.

---

## Building

### Host (amd64, for development and testing)

```bash
cd host
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Dependencies: `libsndfile1-dev libfftw3-dev`

### Library only (host)

```bash
cd lib
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
# installs to /usr/local by default:
sudo make install
```

### Cross-compile for X6100 (ARMv7)

The X6100 runs an ARMv7 hard-float Linux system built with Buildroot. A toolchain sysroot is required.

```bash
cd target
./build.sh   # wraps cmake with the ARMv7 toolchain file
```

The resulting `libx6100js8.so` is copied to the radio via the normal firmware deployment process.

---

## Usage example

```c
#include <x6100js8/x6100js8.h>
#include <stdio.h>

int main(void) {
    x6100js8_msg_t *msg = NULL;

    int rc = x6100js8_encode_pota_spot(
        "KI9NG",        // callsign
        "US-0765",      // park reference
        14.225,         // dial frequency MHz
        "SSB",          // mode
        1500.0,         // audio tone offset Hz
        &msg
    );
    if (rc != 0) { fprintf(stderr, "encode failed: %d\n", rc); return 1; }

    int n = x6100js8_msg_frame_count(msg);
    printf("%d frames, %.1f sec on-air\n", n, n * 15.0);

    for (int i = 0; i < n; i++) {
        size_t samples;
        const int16_t *pcm = x6100js8_msg_frame(msg, i, &samples);
        // In firmware:
        //   wait_for_slot_boundary();
        //   radio_set_modem(true);
        //   usleep(X6100JS8_PTT_DELAY_MS * 1000);
        //   audio_play(pcm, samples);
        //   audio_play_wait();
        //   radio_set_modem(false);
        printf("  frame %d: %zu samples (%.3f sec)\n",
               i + 1, samples, (double)samples / X6100JS8_SAMPLE_RATE);
    }

    x6100js8_msg_free(msg);
    return 0;
}
```

---

## Scheduler notes for firmware integrators

The library intentionally produces no inter-frame silence. The correct TX scheduler:

1. **Slot boundary detection**: `fmod(clock_gettime(CLOCK_REALTIME), 15.0)` gives seconds into the current slot. Fire immediately if `into_slot < X6100JS8_LATE_WINDOW_MS/1000.0` (2.16 sec) — this matches JS8Call's `lateThreshold` behaviour and handles the case where `audio_play_wait()` returns slightly after a slot boundary.

2. **Do not use time-based sleeps** to define inter-frame gaps. The gap is whatever wall-clock time remains between `audio_play_wait()` returning and the next slot boundary poll firing. On PulseAudio, `audio_play_wait()` may return up to ~2 sec after the audio actually finishes playing — a 2.16 sec late window absorbs this.

3. **Do not recalculate `into_slot` from `clock_gettime` between frames** if the calculation spans a slot boundary. Track the anchor slot-start time from the first boundary wait and derive all subsequent boundaries from it.

4. **PTT delay**: always `usleep(X6100JS8_PTT_DELAY_MS * 1000)` (500 ms) between PTT assert and audio start. JS8Call's `JS8A_START_DELAY_MS = 500` — receivers expect this silence before the Costas sync arrays.

---

## Repo structure

```
x6100-js8-engine/
├── lib/                    — libx6100js8 (the library)
│   ├── include/x6100js8/   — public C header
│   ├── src/                — public API implementation
│   ├── js8encode/          — JS8Call-compatible frame encoder
│   ├── jsc/                — JSC word-dictionary compression
│   └── test/               — unit tests and WAV smoke tests
├── host/                   — amd64 host driver (Phase 1/2, dev/test)
│   └── fate/               — rtmrtmrtm/fate LDPC + FSK pipeline
└── target/                 — ARMv7 cross-compiled driver for X6100
    └── fate/               — fate sources (shared with host)
```

---

## Status

| Feature | Status |
|---|---|
| JS8 heartbeat (HB) encode | ✅ Complete, on-air verified |
| POTA spot via APSPOT | ✅ Wire format confirmed by live test |
| Directed message encode | ✅ API complete, pending on-air test |
| Slot-boundary TX scheduler | 🔧 In progress (`dialog_pota_spot.c` in x6100_gui) |
| ARMv7 cross-compile | ✅ Complete |
| APRS position beacon | 📋 Planned |
| JS8 text messaging (prestored) | 📋 Planned |
| Emergency beacon | 📋 Planned |

---

## Related

- [ki9ng/x6100_gui](https://github.com/ki9ng/x6100_gui) — X6100 firmware fork; links against this library
- [AetherX6100](https://github.com/AetherRadio/AetherX6100) — X6100 hardware control library
- [rtmrtmrtm/fate](https://github.com/rtmrtmrtmrtm/fate) — JS8 decoder this is based on
- [JS8Call](http://js8call.com) — the original JS8Call application
- [apspot.radio](https://apspot.radio) — APRS-IS POTA spotting service

---

## License

GPL-3.0-or-later. The jsc and js8encode components are GPL-3; fate is MIT; the combination is GPL-3.
