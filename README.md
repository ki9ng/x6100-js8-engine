# x6100-js8-engine

JS8 heartbeat beacon for the Xiegu X6100. Produces a standards-compliant JS8 heartbeat frame ("KI9NG: HB EM69") as a 48 kHz 16-bit mono WAV, verified decodable by fate's JS8 decoder.

Based on [rtmrtmrtmrtm/fate](https://github.com/rtmrtmrtmrtm/fate) — a minimal pure-C++ JS8 implementation by Robert Morris AB1HL.

---

## Phase 1 — Host build (complete)

### Build

```bash
# On the Optiplex (or any Linux/amd64 host)
cd host
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

Dependencies: `libsndfile1-dev libfftw3-dev`

### Generate heartbeat WAV

```bash
./js8-hb-gen              # writes test.wav at 48 kHz
./js8-hb-gen myfile.wav   # custom output path
```

Output: 606720 samples, 12.640 sec, 48 kHz 16-bit mono PCM.

### Verify decode

```bash
sox test.wav -r 6000 test6k.wav     # resample for fate decoder
./decode-test test6k.wav            # should print: KI9NG: HB EM69
```

Expected output:
```
Loaded 75840 samples at 6000 Hz (12.640 sec)
DECODE hz=1500.0 off=2.502s snr=2.9 dB: KI9NG: HB EM69
```

---

## Phase 3 — On-air test runbook

Play `test.wav` through a Digirig interface into the X6100 to transmit a
single JS8 heartbeat. Verify receipt on [pskreporter.info](https://pskreporter.info).

### Prerequisites

- Xiegu X6100 with Digirig Mobile (or similar USB audio interface)
- `test.wav` from the build above (48 kHz 16-bit mono)
- A JS8 frequency (common: 14.078 MHz USB dial → audio at 1500 Hz = 14.079500 MHz)
- Valid amateur radio licence for your jurisdiction
- Ensure no DX stations are actively using the frequency before transmitting

### X6100 setup

1. Set mode to **USB** (not USB-D — the X6100's DATA mode may apply EQ; plain USB is correct for Digirig audio injection)
2. Tune to **14.078 MHz** (JS8 normal submode calling frequency)
3. Set mic gain to 0 (audio comes entirely from Digirig)
4. Connect Digirig: audio out → X6100 mic/data jack, PTT line wired

### Transmit procedure

```bash
# 1. Identify the Digirig audio device
aplay -l | grep -i digirig     # note card number, e.g. card 2

# 2. Arm PTT via Digirig's RTS/DTR line (hamlib or manual)
#    With rigctl/hamlib:
rigctl -m 1 -r /dev/ttyUSB0 T 1    # PTT on

# 3. Play the WAV into the Digirig audio output
aplay -D hw:2,0 test.wav            # replace 2 with your card number

# 4. Release PTT immediately after aplay finishes
rigctl -m 1 -r /dev/ttyUSB0 T 0    # PTT off
```

Alternatively, if the Digirig is wired for VOX-style keying via audio level,
step 2/4 are not needed.

### Verify receipt

- Open [pskreporter.info](https://pskreporter.info) and search for **KI9NG**
- Allow up to 15 minutes for the report to appear
- A decode by any JS8Call station within propagation range confirms on-air success

### Troubleshooting

| Symptom | Likely cause |
|---------|-------------|
| No decode by self-monitoring JS8Call | Audio level too low/high — adjust aplay volume or Digirig input gain |
| ALC meter pegging | Drive too hot — reduce aplay volume with `--volume` flag or mixer |
| Frame decoded but wrong call | Regenerate WAV with correct callsign/grid in `main.cc` |
| No pskreporter reports after 15 min | Check propagation / band conditions; try 7.078 MHz (40m) |
