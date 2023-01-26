//------------------------------------------------------------------------|
// Copyright (c) 2020-2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//------------------------------------------------------------------------|

#pragma once

//-----------------------------------------------------------------------------+
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

//-----------------------------------------------------------------------------+
// Some timespec struct conversion and operator functions that chronometer
// depends on, but may be useful for other reasons.
struct timespec seconds_to_timespec(double seconds);
double timespec_to_seconds(struct timespec * ts);
struct timespec timespec_sub(struct timespec * a, struct timespec * b);
struct timespec timespec_add(struct timespec * a, struct timespec * b);

//-----------------------------------------------------------------------------+
// A chronometer object for measuring relative elapsed time.  This has good
// accuracy within the microsecond range but not so much within the nanosecond
// range due to accumulation of truncation errors.  For best results: reset
// often and try to keep the start/stop/resume to a minimum.
typedef struct chronom_t
{
    // Create a chronometer.  Initially stopped at elapsed time 0.0
    struct chronom_t * (*create)();

    // Destroy a chronometer object
    void (*destroy)(void * chronom);

    // Start the chronometer running
    void (*start)(struct chronom_t * chronom);

    // Stop/Pause the chronometer
    void (*stop)(struct chronom_t * chronom);

    // Reset chronom back to initial conditions: elapsed 0.0
    void (*reset)(struct chronom_t * chronom);

    // Resume after a Stop.  This is just a wrapper around Start.
    // Semantically it's normally used after a Stop rather than Reset
    void (*resume)(struct chronom_t * chronom);

    // Get the state of the chronometer (true = running, false = stopped)
    bool (*running)(struct chronom_t * chronom);

    // Get total elapsed time as fractional seconds (while in any state)
    double (*elapsed_seconds)(struct chronom_t * chronom);

    // Get total elapsed time as a struct (while in any state)
    struct timespec (*elapsed)(struct chronom_t * chronom);

    // Dump a DEBUG-level report of the chronometer to blammo
    void (*report)(struct chronom_t * chronom, const char * title);

    // PIMPL private data pointer
    void * data;
}
chronom_t;

extern const chronom_t chronom_pub;
