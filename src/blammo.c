//------------------------------------------------------------------------|
// Copyright (c) 2018-2020 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

#ifdef BLAMMO_ENABLE

//------------------------------------------------------------------------|
#define _POSIX_C_SOURCE 1        // enable localtime_r()
#define _GNU_SOURCE              // PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP

#include "blammo.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <libgen.h>              // basename()
#include <sys/time.h>            // gettimeofday()
#include <pthread.h>

//------------------------------------------------------------------------|
static const char * blammo_msg_t_str[] =
{
    "VERBOSE",
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL",
    NULL
};

//------------------------------------------------------------------------|
typedef struct
{
    // Log file name (if specified AND can be written to)
    char * filename;

    // The blammo log level
    blammo_msg_t level;

    // Last known day of the year 0-365
    int yday;

    // Mutex for thread-safety
    pthread_mutex_t lock;
}
blammo_data_t;

//------------------------------------------------------------------------|
// static initialization of blammo singleton
static blammo_data_t blammo_data = {
        NULL,
        ERROR,
        -1,
        PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
};

//------------------------------------------------------------------------|
// get a log-friendly timestamp string for current time.  Also return the
// day of the year.
static inline int timestamp(char * tstr, size_t size, const char * format,
                            bool append_ms)
{
    struct timeval tv;
    struct tm time;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &time);
    strftime(tstr, size, format, &time);

    if (append_ms)
    {
        size_t len = strlen(tstr);
        snprintf(tstr + len, size - len, ".%03ld", tv.tv_usec / 1000);
    }

    return time.tm_yday;
}

//------------------------------------------------------------------------|
void blammo_file(const char * filename)
{
    // This just checks if the file path is able to be written to.
    // If not, then we'll just not be logging to file!
    FILE * file = fopen(filename, "a");
    if (NULL == file)
    {
    	// TODO: fprintf(stderr if fopen fails
        BLAMMO(ERROR, "fopen(%s) for append failed", filename);
        return;
    }

    // File path can be written to
    // WARNING: Because this is a pointer assignment, it relies on the
    // string constant remaining active in scope throughout runtime.
    // E.G. setting this from a parsed command-line argument is OK,
    // but dynamically building a string in a function that returns will
    // fail spectacularly.  TODO: Consider refactoring this later.
    blammo_data.filename = (char *) filename;
    fclose(file);
}

//------------------------------------------------------------------------|
inline void blammo_level(blammo_msg_t level)
{
    blammo_data.level = level;
}

//------------------------------------------------------------------------|
void blammo(const char * fpath, int line, const char * func,
            const blammo_msg_t type, const char * format, ...)
{
    // If the message doesn't rise to the set level, then discard it
    if (type < blammo_data.level)
    {
        return;
    }

    // Mutex here for multi-threaded applications
    int error = pthread_mutex_lock(&blammo_data.lock);
    if (error != 0)
    {
        fprintf(stderr, "%s: pthread_mutex_lock() returned %d\n", __FUNCTION__, error);
        return;
    }

    va_list args;
    char * fname = basename((char *) fpath);
    char time[48];
    int yday = timestamp(time, 48, "%T", true);

    // Log date and day of week verbosely and recursively whenever the day
    // changes.  Initialized to -1 so it happens on first call also.  This
    // saves horizontal space by allowing full date to be excluded from
    // every single message.
    if (yday != blammo_data.yday)
    {
        char date[64];
        blammo_data.yday = timestamp(date, 64, "%A %m/%d/%Y", false);
        BLAMMO(INFO, "--- %s ---", date);
    }

    // Always log to stdout
    fprintf(stdout, "%s %s %s:%d ", time, blammo_msg_t_str[type], fname, line);
    va_start(args, format);
    vfprintf(stdout, format, args); 
    va_end(args);
    fprintf(stdout, "\n");

    // Log to file if available
    if (NULL != blammo_data.filename)
    {
        FILE * file = fopen(blammo_data.filename, "a");
        // TODO: fprintf(stderr if fopen fails
        if (NULL != file)
        {
            fprintf(file, "%s %s %s:%d ", time, blammo_msg_t_str[type], fname, line);
            va_start(args, format);
            vfprintf(file, format, args);
            va_end(args);
            fprintf(file, "\n");
            fflush(file);
            fclose(file);
        }
    }

    pthread_mutex_unlock(&blammo_data.lock);
}

#endif
