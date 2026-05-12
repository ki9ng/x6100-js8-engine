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
 * JS8 Normal mode frame constants (48 kHz output).
 *
 * Each frame is exactly X6100JS8_SAMPLES_PER_FRAME samples (12.64 sec).
 * Frames must be transmitted at 15-sec slot boundaries (0, 15, 30, 45 sec
 * past the UTC minute). The 2.36 sec dead-air gap between frames is the
 * scheduler's responsibility — the encoder produces no silence.
 *
 * Slot timing: fire PTT at boundary, wait JS8A_START_DELAY_MS (500 ms),
 * then play one frame. Drop PTT. Wait for next slot boundary. Repeat.
 */
#define X6100JS8_SAMPLE_RATE        48000
#define X6100JS8_SYMBOLS_PER_FRAME  79
#define X6100JS8_SAMPLES_PER_SYMBOL 7680        /* 1920 * (48000/12000) */
#define X6100JS8_SAMPLES_PER_FRAME  606720      /* 79 * 7680 */
#define X6100JS8_FRAME_DURATION_MS  12640       /* 12.64 sec exactly */
#define X6100JS8_SLOT_PERIOD_MS     15000       /* 15.0 sec */
#define X6100JS8_PTT_DELAY_MS       500         /* JS8A_START_DELAY_MS */
#define X6100JS8_LATE_WINDOW_MS     2160        /* fire if within this of boundary */

/*
 * Multi-frame message handle.
 *
 * The encoder breaks a directed JS8 message into one or more frames.
 * Each frame is exactly X6100JS8_SAMPLES_PER_FRAME samples at 48 kHz.
 * No inter-frame silence is included — that is the scheduler's job.
 *
 * Usage:
 *   x6100js8_msg_t *msg = NULL;
 *   x6100js8_encode_pota_spot(..., &msg);
 *
 *   for (int i = 0; i < x6100js8_msg_frame_count(msg); i++) {
 *       size_t   n;
 *       int16_t *samples = x6100js8_msg_frame(msg, i, &n);
 *       // wait for slot boundary, key PTT, usleep(500ms), play samples, drop PTT
 *   }
 *   x6100js8_msg_free(msg);
 */
typedef struct x6100js8_msg x6100js8_msg_t;

/* Return the number of frames in the message. */
int     x6100js8_msg_frame_count(const x6100js8_msg_t *msg);

/*
 * Return a pointer to frame i's samples and write the sample count to *n.
 * Pointer is valid until x6100js8_msg_free() is called.
 * Returns NULL if i is out of range.
 */
const int16_t *x6100js8_msg_frame(const x6100js8_msg_t *msg, int i, size_t *n);

/* Free the message and all its frames. */
void    x6100js8_msg_free(x6100js8_msg_t *msg);

/*
 * Encode a single-frame heartbeat: "MYCALL: HB GRID".
 * Output is exactly X6100JS8_SAMPLES_PER_FRAME samples (12.64 sec) at 48 kHz.
 * No silence appended.
 *
 * Returns 0 on success. *out_samples is malloc()'d; caller must free().
 */
int x6100js8_encode_hb(const char *callsign,
                       const char *grid,
                       double      hz,
                       int16_t   **out_samples,
                       size_t     *out_n_samples);

/*
 * Encode a POTA self-spot via @APRSIS CMD to APSPOT.
 *
 * Builds: "@APRSIS CMD :APSPOT   :! POTA <park> <freq_mhz> <mode>"
 * (e.g.   "@APRSIS CMD :APSPOT   :! POTA US-0765 14.225 SSB")
 *
 * freq_mhz: dial frequency in MHz as a double (e.g. 14.225).
 *           Do NOT add the audio tone offset — report dial freq only.
 * mode:     "SSB" | "CW" | "FM" | "USB" | "LSB" | "DATA"
 * hz:       audio tone offset in Hz (typically 1500.0)
 *
 * On success: *msg points to a new x6100js8_msg_t. Caller must free with
 * x6100js8_msg_free(). Each frame is X6100JS8_SAMPLES_PER_FRAME samples,
 * no inter-frame silence.
 *
 * Returns 0 on success, nonzero on failure.
 */
int x6100js8_encode_pota_spot(const char      *callsign,
                              const char      *park,
                              double           freq_mhz,
                              const char      *mode,
                              double           hz,
                              x6100js8_msg_t **msg);

/*
 * Encode an arbitrary directed JS8 message.
 *
 * Builds: "<callsign>: <to> <text>"
 * e.g.   "KI9NG: W9PRK QRV POTA"
 *
 * On success: *msg points to a new x6100js8_msg_t. Caller must free.
 * Returns 0 on success, nonzero on failure.
 */
int x6100js8_encode_directed(const char      *callsign,
                             const char      *to,
                             const char      *text,
                             double           hz,
                             x6100js8_msg_t **msg);

/*
 * Library version string, e.g. "0.5.0".
 */
const char *x6100js8_version(void);

#ifdef __cplusplus
}
#endif

#endif /* X6100JS8_H */
