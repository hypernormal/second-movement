/*
 * MIT License
 *
 * Copyright (c) 2025 Mathias Kende
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tide_face.h"
#include "tide_table_generated.h"
#include "watch.h"
#include "watch_utility.h"
#include "zones.h"

#define TIDE_WINDOW_DAYS 7
#define TIDE_SECONDS_PER_DAY (24 * 60 * 60)
#define TIDE_WINDOW_SECONDS (TIDE_WINDOW_DAYS * TIDE_SECONDS_PER_DAY)
#define TIDE_SPLASH_SECONDS 2
#define TIDE_SPLASH_TICK_FREQUENCY 4
#define TIDE_SPLASH_TICKS (TIDE_SPLASH_SECONDS * TIDE_SPLASH_TICK_FREQUENCY)

typedef struct {
    uint16_t start_index;
    uint16_t window_count;
    uint16_t offset;
    bool showing_splash;
    uint8_t splash_tick;
} tide_state_t;

static uint16_t _find_next_event_index(uint32_t now) {
    for (uint16_t i = 0; i < TIDE_TABLE_NUM_EVENTS; i++) {
        if (tide_event_times[i] >= now) {
            return i;
        }
    }
    return TIDE_TABLE_NUM_EVENTS - 1;
}

static uint16_t _compute_window_count(uint16_t start_index, uint32_t now) {
    uint32_t limit = now + TIDE_WINDOW_SECONDS;
    uint16_t count = 0;
    while (start_index + count < TIDE_TABLE_NUM_EVENTS &&
           tide_event_times[start_index + count] <= limit) {
        count++;
    }
    return count;
}

static void _refresh_window(tide_state_t *state, uint32_t now) {
    state->start_index = _find_next_event_index(now);
    state->window_count = _compute_window_count(state->start_index, now);
    if (state->offset >= state->window_count) {
        state->offset = 0;
    }
}

static void _tide_24h_to_12h(watch_date_time_t *date_time) {
    date_time->unit.hour %= 12;
    if (date_time->unit.hour == 0) {
        date_time->unit.hour = 12;
    }
}

static void _draw_tide(tide_state_t *state) {
    watch_clear_display();

    if (TIDE_TABLE_NUM_EVENTS == 0 || state->window_count == 0) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP, "TIDE", "TI");
        watch_display_text(WATCH_POSITION_BOTTOM, "----");
        return;
    }

    uint16_t idx = state->start_index + state->offset;
    if (idx >= TIDE_TABLE_NUM_EVENTS) {
        idx = TIDE_TABLE_NUM_EVENTS - 1;
    }

    uint32_t event_time = tide_event_times[idx];
    int16_t height = tide_event_heights[idx];
    bool is_high = height >= 0;

    watch_date_time_t date_time =
        watch_utility_date_time_from_unix_time(event_time, movement_get_current_timezone_offset());

    // Top center: tide type, in caps.
    watch_display_text(WATCH_POSITION_TOP, is_high ? "HI" : "LO");

    // Top right: day of month.
    char day_str[3];
    sprintf(day_str, "%2u", date_time.unit.day);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, day_str);

    // Bottom part of screen: time in the same layout as the clock face.
    bool pm = false;
    if (movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H) {
        pm = date_time.unit.hour >= 12;
        _tide_24h_to_12h(&date_time);
        if (pm) {
            watch_set_indicator(WATCH_INDICATOR_PM);
        } else {
            watch_clear_indicator(WATCH_INDICATOR_PM);
        }
        watch_clear_indicator(WATCH_INDICATOR_24H);
    } else {
        watch_set_indicator(WATCH_INDICATOR_24H);
        watch_clear_indicator(WATCH_INDICATOR_PM);
    }

    char time_str[7];
    // Formatting matches the clock face: hour/minute sit in the bottom row's
    // big digit positions, with the hardware colon between them.
    sprintf(time_str, "%2u%02u  ", date_time.unit.hour, date_time.unit.minute);
    watch_display_text(WATCH_POSITION_BOTTOM, time_str);
    watch_set_colon();
}

static void _draw_splash(uint8_t tick) {
    watch_clear_display();
    // "TIDES" fits across the bottom row (positions 4-9).
    watch_display_text(WATCH_POSITION_BOTTOM, "TIDES");
    // A small two-frame wave animation across the top row.
    if (tick % 2 == 0) {
        watch_display_text(WATCH_POSITION_TOP_LEFT, "^v");
        watch_display_text(WATCH_POSITION_TOP_RIGHT, "^v");
    } else {
        watch_display_text(WATCH_POSITION_TOP_LEFT, "v^");
        watch_display_text(WATCH_POSITION_TOP_RIGHT, "v^");
    }
}

void tide_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(tide_state_t));
        tide_state_t *state = (tide_state_t *)*context_ptr;
        memset(state, 0, sizeof(tide_state_t));
    }
}

void tide_face_activate(void *context) {
    tide_state_t *state = (tide_state_t *)context;
    state->showing_splash = true;
    state->splash_tick = 0;
    movement_request_tick_frequency(TIDE_SPLASH_TICK_FREQUENCY);
    _draw_splash(0);
}

bool tide_face_loop(movement_event_t event, void *context) {
    tide_state_t *state = (tide_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            tide_face_activate(context);
            break;

        case EVENT_TICK:
            if (state->showing_splash) {
                state->splash_tick++;
                if (state->splash_tick >= TIDE_SPLASH_TICKS) {
                    state->showing_splash = false;
                    movement_request_tick_frequency(1);
                    uint32_t now = movement_get_utc_timestamp();
                    _refresh_window(state, now);
                    state->offset = 0;
                    _draw_tide(state);
                } else {
                    _draw_splash(state->splash_tick);
                }
                break;
            }

            // If the currently-displayed "next tide" has just passed, refresh.
            {
                uint32_t now = movement_get_utc_timestamp();
                if (state->offset == 0 &&
                    state->start_index < TIDE_TABLE_NUM_EVENTS &&
                    now > tide_event_times[state->start_index] + 60) {
                    _refresh_window(state, now);
                    _draw_tide(state);
                }
            }
            break;

        case EVENT_ALARM_BUTTON_UP:
            if (state->showing_splash) {
                state->showing_splash = false;
                movement_request_tick_frequency(1);
                uint32_t now = movement_get_utc_timestamp();
                _refresh_window(state, now);
                state->offset = 0;
                _draw_tide(state);
                break;
            }
            if (state->window_count == 0) {
                break;
            }
            state->offset++;
            if (state->offset >= state->window_count) {
                state->offset = 0;
            }
            _draw_tide(state);
            break;

        case EVENT_LOW_ENERGY_UPDATE: {
            // Always return to the next upcoming tide in low-energy mode.
            state->showing_splash = false;
            uint32_t now = movement_get_utc_timestamp();
            _refresh_window(state, now);
            state->offset = 0;
            _draw_tide(state);
            if (watch_get_lcd_type() == WATCH_LCD_TYPE_CLASSIC) {
                watch_start_sleep_animation(500);
            } else {
                watch_set_indicator(WATCH_INDICATOR_SLEEP);
            }
            break;
        }

        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;

        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void tide_face_resign(void *context) {
    (void) context;
    movement_request_tick_frequency(1);
}
