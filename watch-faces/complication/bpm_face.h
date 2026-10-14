/*
 * MIT License
 *
 * Copyright (c) 2025
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

/*
 * BPM Tap
 *
 * A simple tap-to-measure-BPM face. The wearer taps the watch (single-tap via
 * the accelerometer) and the face calculates an average BPM over a moving window.
 */

#include "movement.h"
#include "watch.h"

// If no tap occurs for this many seconds, the running average is cleared
// so the user can start a fresh measurement without a manual reset.
#define BPM_INACTIVITY_TIMEOUT_SECONDS 5

// Number of recent taps to keep for the running average. 32 slots is plenty
// for normal tapping speeds (even very fast tempos).
#define BPM_BUFFER_SIZE 32

// Minimum time between accepted taps. The accelerometer's hardware single-tap
// event can fire more than once for one physical tap, which makes the BPM jump
// around. Your trace showed a second bounce just 250 ms after the first, so we
// use a longer lockout. 300 ms blocks those double triggers while still allowing
// very fast real tempos (300 ms -> up to 200 BPM).
#define BPM_MIN_TAP_INTERVAL_MS 300

// Outlier-rejection window, relative to the median interval. Tap intervals
// outside this band are considered noise and are excluded from the average.
// Using 0.5x to 1.5x of the median is a robust starting point.
#define BPM_OUTLIER_LOW_NUMERATOR   1
#define BPM_OUTLIER_LOW_DENOMINATOR 2
#define BPM_OUTLIER_HIGH_NUMERATOR   3
#define BPM_OUTLIER_HIGH_DENOMINATOR 2

// Set to 1 to print per-tap accelerometer readings and filter decisions over
// the USB serial console. Set to 0 when you are done tuning.
#define BPM_DEBUG_SERIAL 0

typedef struct {
    uint32_t tap_times[BPM_BUFFER_SIZE];
    uint8_t head;
    uint8_t count;
    bool tap_available;
    uint32_t last_accepted_tap;
} bpm_state_t;

void bpm_face_setup(uint8_t watch_face_index, void **context_ptr);
void bpm_face_activate(void *context);
bool bpm_face_loop(movement_event_t event, void *context);
void bpm_face_resign(void *context);

#define bpm_face ((const watch_face_t){ \
    bpm_face_setup, \
    bpm_face_activate, \
    bpm_face_loop, \
    bpm_face_resign, \
    NULL, \
})
