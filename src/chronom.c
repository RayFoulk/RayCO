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

//-----------------------------------------------------------------------------+
#include <stdio.h>      // snprintf()
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>

#include "chronom.h"
#include "utils.h"              // memzero()
#include "blammo.h"

//-----------------------------------------------------------------------------+
// Reason why 0.5e-9 is added here and subtracted in 'to seconds'
// is to mitigate truncation error as described here:
// https://stackoverflow.com/questions/25328730/how-to-initialize-timespec-from-a-double
struct timespec seconds_to_timespec(double seconds)
{
    struct timespec ts;

    // Add an IOTA so final nanoseconds is correct
    seconds += 0.5e-9;
    ts.tv_sec = (long) seconds;
    ts.tv_nsec = (seconds - ts.tv_sec) * 1000000000L;

    return ts;
}

//-----------------------------------------------------------------------------+
double timespec_to_seconds(struct timespec * ts)
{
    double seconds = (double) ts->tv_sec;
    seconds += ((double) ts->tv_nsec / (double) 1e+9);

    // Subtract the IOTA from total seconds
    if (seconds > 0)
    {
        seconds -= 0.5e-9;
    }

    return seconds;
}

//-----------------------------------------------------------------------------+
// There are many examples for adding and subtracting timespec that can be
// found online.  For example:
// https://codereview.stackexchange.com/questions/40176
struct timespec timespec_sub(struct timespec * a, struct timespec * b)
{
    struct timespec diff;

    diff.tv_sec = a->tv_sec - b->tv_sec;
    diff.tv_nsec = a->tv_nsec - b->tv_nsec;
    if (diff.tv_nsec < 0)
    {
        diff.tv_sec--;
        diff.tv_nsec += 1000000000L;
    }

    return diff;
}

//-----------------------------------------------------------------------------+
struct timespec timespec_add(struct timespec * a, struct timespec * b)
{
    struct timespec sum;

    sum.tv_sec = a->tv_sec + b->tv_sec;
    sum.tv_nsec = a->tv_nsec + b->tv_nsec;
    if (sum.tv_nsec >= 1000000000L)
    {
        sum.tv_sec++;
        sum.tv_nsec -= 1000000000L;
    }

    return sum;
}

//-----------------------------------------------------------------------------+
typedef struct
{
    // Chronometer state: running (true) or stopped (false)
    bool running;

    // Timestamp when the chronometer was started
    struct timespec start;

    // Timestamp when the chronometer was stopped
    struct timespec stop;

    // Sum total elapsed time not including stopped periods
    struct timespec elapsed;
}
chronom_data_t;

//-----------------------------------------------------------------------------+
static chronom_t * chronom_create()
{
    chronom_t * chronom = (chronom_t *) malloc(sizeof(chronom_t));
    if (!chronom)
    {
        BLAMMO(FATAL, "malloc(sizeof(chronom_t) failed");
        return NULL;
    }

    memcpy(chronom, &chronom_pub, sizeof(chronom_t));
    chronom->data = malloc(sizeof(chronom_data_t));
    if (!chronom->data)
    {
        BLAMMO(FATAL, "malloc(sizeof(chronom_data_t) failed");
        chronom->destroy(chronom);
        return NULL;
    }

    chronom->reset(chronom);
    return chronom;
}

static void chronom_destroy(void * chronom)
{
    chronom_t * chronomp = (chronom_t *) chronom;

    if (!chronomp)
    {
        BLAMMO(WARNING, "attempt to destroy invalid chronom_t!");
        return;
    }

    memzero(chronomp->data, sizeof(chronom_data_t));
    free(chronomp->data);

    memzero(chronomp, sizeof(chronom_t));
    free(chronomp);
}

static void chronom_start(chronom_t * chronom)
{
    chronom_data_t * pdata = (chronom_data_t *) chronom->data;

    // Can't start an already-running instance
    if (!pdata->running)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &pdata->start);
        pdata->running = true;
    }
}

static void chronom_stop(chronom_t * chronom)
{
    chronom_data_t * pdata = (chronom_data_t *) chronom->data;

    // Can't stop an already-stopped instance
    if (pdata->running)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &pdata->stop);
        pdata->running = false;

        // Each time the chronometer is stopped, recalculate the elapsed
        // time and accumulate it into the overall elapsed time.
        struct timespec elapsed = timespec_sub(&pdata->stop, &pdata->start);
        pdata->elapsed = timespec_add(&pdata->elapsed, &elapsed);
    }
}

static void chronom_reset(chronom_t * chronom)
{
    memzero(chronom->data, sizeof(chronom_data_t));
}

static inline void chronom_resume(chronom_t * chronom)
{
    chronom->start(chronom);
}

static bool chronom_running(chronom_t * chronom)
{
    chronom_data_t * pdata = (chronom_data_t *) chronom->data;
    return pdata->running;
}

static double chronom_elapsed_seconds(chronom_t * chronom)
{
    struct timespec elapsed = chronom->elapsed(chronom);
    return timespec_to_seconds(&elapsed);
}

static struct timespec chronom_elapsed(chronom_t * chronom)
{
    chronom_data_t * pdata = (chronom_data_t *) chronom->data;

    // If the chronometer is stopped, then elapsed time is the
    // total cumulative elapsed time so far.
    if (!pdata->running)
    {
        return pdata->elapsed;
    }

    // Otherwise take a timestamp now and calculate elapsed time
    // similar to when a stop occurs
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC_RAW, &current);

    // Leave the elapsed time alone since not stopping,
    // and add in the current elapsed on return
    struct timespec elapsed = timespec_sub(&current, &pdata->start);
    return timespec_add(&pdata->elapsed, &elapsed);
}

static void chronom_report(chronom_t * chronom, const char * title)
{
    chronom_data_t * pdata = (chronom_data_t *) chronom->data;

    BLAMMO(DEBUG, "\n%s:\n"
                  "is running: %s\n"
                  "start time: sec %ld nsec %ld\n"
                  "stop time: sec %ld nsec %ld\n"
                  "elapsed time: sec %ld nsec %ld",
                  title,
                  pdata->running ? "true" : "false",
                  pdata->start.tv_sec, pdata->start.tv_nsec,
                  pdata->stop.tv_sec, pdata->stop.tv_nsec,
                  pdata->elapsed.tv_sec, pdata->elapsed.tv_nsec);

    // Annoying warning eater
    (void) pdata;
}

const chronom_t chronom_pub = {
        &chronom_create,
        &chronom_destroy,
        &chronom_start,
        &chronom_stop,
        &chronom_reset,
        &chronom_resume,
        &chronom_running,
        &chronom_elapsed_seconds,
        &chronom_elapsed,
        &chronom_report,
        NULL
};

