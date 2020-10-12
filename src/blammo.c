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
#include "blammo.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

//#define _POSIX_C_SOURCE
//#define _GNU_SOURCE
//#include <string.h>

//------------------------------------------------------------------------|
static const char * blammo_msg_t_str[] =
{
    "INFO",
    "DEBUG",
    "WARNING",
    "ERROR",
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
}
blammo_data_t;

//------------------------------------------------------------------------|
// static initialization of blammo singleton
static blammo_data_t blammo_data = { NULL, ERROR, -1 };

//------------------------------------------------------------------------|
// get a log-friendly timestamp string for current time.  Also return the
// day of the year.
static inline int timestamp(char * ts, size_t size, const char * format,
                            bool append_ms)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // consider using localtime_r() instead
    struct tm * tmp = localtime(&tv.tv_sec);
    strftime(ts, size, format, tmp);

    if (append_ms)
    {
        char msbuf[6];
        snprintf(msbuf, 6, ".%03ld", tv.tv_usec / 1000);
        strcat(ts, msbuf);
    }

    return tmp->tm_yday;
}

//------------------------------------------------------------------------|
void blammo_file(const char * filename)
{
    // This just checks if the file path is able to be written to.
	// If not, then we'll just not be logging to file!
	FILE * file = fopen(filename, "a");
    if (NULL == file)
    {
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

    // TODO: Consider mutex here if this logger is to be thread-safe

    va_list args;
    char * fname = basename((char *) fpath);
    char time[48];
    int yday = timestamp(time, 48, "%T", true);

    // TODO: Log date, day of week verbosely and recursively
    // whenever the day changes.  Initialize to -1 or something
    // invalid so it happens on first call also.  This will
    // save horizontal space by allowing date to be exluded
    // from every message
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
}

#endif
