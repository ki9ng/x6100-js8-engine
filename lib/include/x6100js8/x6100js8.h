/*
 * libx6100js8 — JS8 protocol engine for the Xiegu X6100
 *
 * Public C API. JS8Call-interoperable: encoder output decodes correctly
 * against /usr/bin/js8 + python3-js8py.
 */

#ifndef X6100JS8_H
#define X6100JS8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Caller-owned output buffer convention used by all encoders:
 *   - On entry, *out_samples must be NULL and *out_n_samples must be 0.
 *   - On success, *out_samples points to a malloc()'d buffer of
 *     *out_n_samples int16 samples (48 kHz, mono, little-endian).
 *     Caller must free() it.
 *   - Returns 0 on success, nonzero on failure.
 */

/*
 * Encode a single-frame heartbeat: "MYCALL: HB GRID".
 * For JS8 Normal the output is 606720 samples (12.64 sec) at the audio
 * offset specified by hz (typically 1500.0).
 */
int x6100js8_encode_hb(const char *callsign,
                       const char *grid,
                       double      hz,
                       int16_t   **out_samples,
                       size_t     *out_n_samples);

/*
 * Encode a POTA self-spot using the @APRSIS CMD path that POTAGW listens on.
 *
 * Builds the message "@APRSIS CMD :POTAGW   :CALLSIGN PARK FREQ_KHZ MODE"
 * (where PARK is e.g. "US-0765", FREQ_KHZ is integer kHz like "14225",
 * MODE is "SSB"|"FM"|"CW"|"USB"|"LSB"|"DATA"). Emits a directed @APRSIS
 * frame followed by enough JSC-compressed data frames to carry the body.
 *
 * The message is encoded as: [directed frame][data frame 1]...[data frame N]
 * concatenated as continuous PCM with no inter-frame silence. Total length
 * varies with body size: a typical 32-char body needs 4 data frames + the
 * 1 directed frame = 5 × 12.64 sec ≈ 63 sec on-air.
 *
 * Returns:
 *   0   on success
 *   1-3 invalid arguments (null pointers, bad lengths)
 *   4   callsign couldn't be packed
 *   5+  internal encode failures
 */
int x6100js8_encode_pota_spot(const char *callsign,
                              const char *park,
                              int         freq_khz,
                              const char *mode,
                              double      hz,
                              int16_t   **out_samples,
                              size_t     *out_n_samples);

/*
 * Library version string, e.g. "0.4.0".
 */
const char *x6100js8_version(void);

#ifdef __cplusplus
}
#endif

#endif /* X6100JS8_H */
