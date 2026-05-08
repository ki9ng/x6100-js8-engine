// SPDX-License-Identifier: MIT
//
// Tiny PCM/WAV writer — emits a 16-bit signed mono RIFF WAV file.
// Replaces libsndfile for the target build, where we don't want to drag
// libsndfile into the rootfs just to write Phase-2 test files.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

// Write `samples` (each in roughly the range -1.0 .. +1.0) as a 16-bit signed
// PCM mono WAV at the given sample rate. The samples are normalised so the
// peak hits 95% of full scale, matching fate's writewav() so downstream
// decoders see the same level. Returns 0 on success, nonzero on error.
int wav_write_mono16(const std::vector<double> &samples, const char *filename,
                     int rate);
