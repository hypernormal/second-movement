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

typedef struct {
    uint32_t tap_times[BPM_BUFFER_SIZE];
    uint8_t head;
    uint8_t count;
    bool tap_available;
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
