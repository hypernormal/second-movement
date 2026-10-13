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

#pragma once

#include "movement.h"

/*
 * KINGSBOROUGH CC TIDE face
 *
 * Shows the next high or low tide for Kingsborough Community College /
 * Sheepshead Bay (NOAA station 8517756). The face starts with the first tide
 * that occurs on or after the current moment. Pressing the Alarm button (bottom
 * right) cycles forward through alternating high and low tides for the next
 * seven days, wrapping back to the next tide when you reach the end of the
 * window. The date, time, tide type and predicted tide height are shown.
 *
 * The underlying tide table is generated once a year from NOAA harmonic
 * constituents using scripts/generate_tide_table.py. Regenerate it when you
 * replace the battery or when a new year begins.
 */

void tide_face_setup(uint8_t watch_face_index, void ** context_ptr);
void tide_face_activate(void *context);
bool tide_face_loop(movement_event_t event, void *context);
void tide_face_resign(void *context);

#define tide_face ((const watch_face_t){ \
    tide_face_setup, \
    tide_face_activate, \
    tide_face_loop, \
    tide_face_resign, \
    NULL, \
})
