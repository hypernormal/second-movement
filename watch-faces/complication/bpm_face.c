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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bpm_face.h"
#include "watch.h"
#include "watch_rtc.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Return a monotonic high-resolution timestamp. On the emulator we use the
// browser's performance.now() instead of the emulated RTC counter so that
// background tab throttling doesn't skew the interval measurement.
static uint32_t _bpm_face_now(void) {
#ifdef __EMSCRIPTEN__
    return (uint32_t)EM_ASM_DOUBLE({ return performance.now(); });
#else
    return watch_rtc_get_counter();
#endif
}

static uint32_t _bpm_face_frequency(void) {
#ifdef __EMSCRIPTEN__
    return 1000; // milliseconds
#else
    return watch_rtc_get_frequency();
#endif
}

static uint16_t _bpm_face_calculate(bpm_state_t *state, uint32_t now);

static bool _bpm_face_record_tap(bpm_state_t *state) {
    uint32_t now = _bpm_face_now();
    uint32_t frequency = _bpm_face_frequency();

    // Convert the configured minimum tap interval from milliseconds to RTC
    // ticks (or emulated milliseconds). This blocks the false double triggers
    // that make the BPM readout jump around.
    uint32_t min_interval_ticks = ((uint64_t)BPM_MIN_TAP_INTERVAL_MS * frequency) / 1000;

    bool accepted = true;
    uint32_t interval_ticks = 0;
    if (state->count > 0) {
        uint8_t newest_index = (state->head + BPM_BUFFER_SIZE - 1) % BPM_BUFFER_SIZE;
        interval_ticks = now - state->tap_times[newest_index];
        if (interval_ticks < min_interval_ticks) {
            accepted = false;
        }
    }

    if (accepted) {
        if (state->count < BPM_BUFFER_SIZE) {
            state->count++;
        }
        state->tap_times[state->head] = now;
        state->head = (state->head + 1) % BPM_BUFFER_SIZE;
        state->last_accepted_tap = now;
    }

#if BPM_DEBUG_SERIAL
    uint64_t interval_ms = (uint64_t)interval_ticks * 1000 / (frequency ? frequency : 1);
    uint16_t current_bpm = _bpm_face_calculate(state, now);
    printf("bpm tap: %s interval_ms=%lu count=%u bpm=%u\r\n",
           accepted ? "ACCEPTED" : "REJECTED",
           (unsigned long)interval_ms,
           (unsigned)state->count,
           (unsigned)current_bpm);
#endif

    return accepted;
}

// Sorts a small array of interval ticks in place and returns the median.
static uint32_t _bpm_face_interval_median(uint32_t *intervals, uint8_t n) {
    if (n == 0) return 0;

    for (uint8_t i = 1; i < n; i++) {
        uint32_t key = intervals[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && intervals[j] > key) {
            intervals[j + 1] = intervals[j];
            j--;
        }
        intervals[j + 1] = key;
    }

    if (n % 2) {
        return intervals[n / 2];
    }
    return (intervals[n / 2 - 1] + intervals[n / 2]) / 2;
}

static uint16_t _bpm_face_calculate(bpm_state_t *state, uint32_t now) {
    uint32_t frequency = _bpm_face_frequency();

    if (state->count > 0) {
        uint8_t newest_index = (state->head + BPM_BUFFER_SIZE - 1) % BPM_BUFFER_SIZE;
        uint32_t idle_ticks = now - state->tap_times[newest_index];

        // Clear the measurement if the user hasn't tapped for a while.
        if (idle_ticks >= (uint32_t)BPM_INACTIVITY_TIMEOUT_SECONDS * frequency) {
            state->count = 0;
        }
    }

    // Need at least two taps to compute an interval average.
    if (state->count < 2) {
        return 0;
    }

    uint8_t oldest_index = (state->head + BPM_BUFFER_SIZE - state->count) % BPM_BUFFER_SIZE;
    uint8_t newest_index = (state->head + BPM_BUFFER_SIZE - 1) % BPM_BUFFER_SIZE;
    uint32_t elapsed_ticks = state->tap_times[newest_index] - state->tap_times[oldest_index];

    // Avoid division by zero if two taps came in on the same tick.
    if (elapsed_ticks == 0) {
        return 0;
    }

    uint8_t n_intervals = state->count - 1;
    uint32_t intervals[BPM_BUFFER_SIZE];

    // Compute every consecutive interval in the stored window.
    uint8_t idx = oldest_index;
    for (uint8_t i = 0; i < n_intervals; i++) {
        uint8_t next_idx = (idx + 1) % BPM_BUFFER_SIZE;
        intervals[i] = state->tap_times[next_idx] - state->tap_times[idx];
        idx = next_idx;
    }

    // Use the median interval as a robust reference, then drop any intervals
    // that deviate too far from it before averaging. This is far less twitchy
    // than the simple mean when the accelerometer fires an extra tap event.
    uint32_t median_interval = _bpm_face_interval_median(intervals, n_intervals);
    if (median_interval == 0) {
        return 0;
    }

    uint32_t lower = (median_interval * BPM_OUTLIER_LOW_NUMERATOR) / BPM_OUTLIER_LOW_DENOMINATOR;
    uint32_t upper = (median_interval * BPM_OUTLIER_HIGH_NUMERATOR) / BPM_OUTLIER_HIGH_DENOMINATOR;

    uint64_t inlier_sum = 0;
    uint8_t inlier_count = 0;
    for (uint8_t i = 0; i < n_intervals; i++) {
        uint32_t interval = intervals[i];
        if (interval >= lower && interval <= upper) {
            inlier_sum += interval;
            inlier_count++;
        }
    }

    // If every interval was rejected, fall back to the median so we still
    // display a sensible value.
    uint32_t average_interval_ticks = (inlier_count > 0) ? (uint32_t)(inlier_sum / inlier_count) : median_interval;

    float average_interval_s = (float)average_interval_ticks / frequency;
    uint16_t bpm = (uint16_t)((60.0f / average_interval_s) + 0.5f);
    return bpm;
}

static void _bpm_face_update_display(bpm_state_t *state) {
    uint32_t now = _bpm_face_now();
    uint16_t bpm = _bpm_face_calculate(state, now);

    char buf[7];
    if (bpm == 0) {
        snprintf(buf, sizeof(buf), "    --");
    } else if (bpm < 1000) {
        snprintf(buf, sizeof(buf), "%6u", bpm);
    } else {
        snprintf(buf, sizeof(buf), "  Hi  ");
    }

    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "bpm", "BP");
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
}

void bpm_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(bpm_state_t));
        memset(*context_ptr, 0, sizeof(bpm_state_t));
    }
}

void bpm_face_activate(void *context) {
    bpm_state_t *state = (bpm_state_t *)context;

    // Start with a clean window each time the face is opened.
    state->head = 0;
    state->count = 0;
    state->last_accepted_tap = 0;
    state->tap_available = movement_enable_tap_detection_if_available(false);

#if BPM_DEBUG_SERIAL
    printf("bpm: face active, tap detection available=%d\r\n", state->tap_available);
#endif

    movement_request_tick_frequency(1);
    _bpm_face_update_display(state);
}

bool bpm_face_loop(movement_event_t event, void *context) {
    bpm_state_t *state = (bpm_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            _bpm_face_update_display(state);
            break;
        case EVENT_SINGLE_TAP:
            _bpm_face_record_tap(state);
            _bpm_face_update_display(state);
            break;
        case EVENT_LIGHT_BUTTON_UP:
            // If the accelerometer is not present (e.g. emulator or board
            // without LIS2DW), allow a light-button press to act as a tap.
            if (!state->tap_available) {
                _bpm_face_record_tap(state);
                _bpm_face_update_display(state);
            }
            break;
        case EVENT_MODE_BUTTON_UP:
            movement_move_to_next_face();
            break;
        case EVENT_ALARM_LONG_PRESS:
            // Clear the moving window.
            state->head = 0;
            state->count = 0;
            state->last_accepted_tap = 0;
            _bpm_face_update_display(state);
            break;
        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

void bpm_face_resign(void *context) {
    bpm_state_t *state = (bpm_state_t *)context;
    movement_disable_tap_detection_if_available();
    state->tap_available = false;
}
