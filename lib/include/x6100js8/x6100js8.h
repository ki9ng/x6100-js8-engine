/*
 * libx6100js8 — JS8 protocol engine for the Xiegu X6100
 *
 * Public C API. The implementation underneath is C++ (ported from
 * rtmrtmrtmrtm/fate), but everything callable from outside the library
 * is plain C so x6100_gui (a C codebase with a few .cpp helpers) can
 * link against it without ABI surprises.
 *
 * Phase 2.5 surface: encoder only. Decoder hooks land in Phase 4.
 */

#ifndef X6100JS8_H
#define X6100JS8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Encode a JS8 directed-message frame ("MYCALL: HB GRID", an HB probe
 * frame in fate parlance) into 48 kHz mono 16-bit little-endian PCM.
 *
 * Caller-owned output buffer:
 *   - On entry, *out_samples must be NULL and *out_n_samples must be 0.
 *   - On success, *out_samples points to a malloc()'d buffer holding
 *     *out_n_samples int16 samples. Caller must free() it.
 *
 * Returns 0 on success, nonzero on failure (callsign too long, grid
 * malformed, etc.).
 *
 * For the JS8 Normal submode the output is exactly 606720 samples
 * (12.64 sec) at 1500 Hz audio offset.
 */
int x6100js8_encode_hb(const char *callsign,
                       const char *grid,
                       double      hz,
                       int16_t   **out_samples,
                       size_t     *out_n_samples);

/*
 * Library version string, e.g. "0.1.0".
 */
const char *x6100js8_version(void);

#ifdef __cplusplus
}
#endif

#endif /* X6100JS8_H */
