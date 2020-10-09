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
#include <string.h>

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
}
blammo_data_t;

//------------------------------------------------------------------------|
// static initialization of blammo singleton
static blammo_data_t blammo_data = { NULL, ERROR };

//------------------------------------------------------------------------|
// get a log-friendly timestamp string for current time.  This is NOT
// thread-safe or re-entrant safe
static inline void timestamp(char * ts, size_t size)
{
    time_t currtime = time(NULL);
    struct tm * tmp = localtime(&currtime);
    strftime (ts, size, "%F %T", tmp);
}

//------------------------------------------------------------------------|
void blammo_file(const char * filename)
{
    // This just checks if the file path is able to be written to.
	// If not, then we'll just not be logging to file!
	FILE * file = fopen(filename, "a");
    if (NULL == file)
    {
        BLAMMO(ERROR, "fopen(%s) for write failed", filename);
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
    char ts[48];
    char * fname = basename((char *) fpath);
    char * bname = (char *) malloc(strlen(fname) + 1);
    //char * dontcare = NULL;
    char * tag = NULL;

    timestamp(ts, 48);
    strcpy(bname, fname);
    //tag = strtok_r(bname, ".", &dontcare);
    tag = strtok(bname, ".");

    // Always log to stdout
    fprintf(stdout, "%s %s %s:%d ", ts, blammo_msg_t_str[type], tag, line);
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
            fprintf(file, "%s %s %s:%d ", ts, blammo_msg_t_str[type], tag, line);
            va_start(args, format);
            vfprintf(file, format, args);
            va_end(args);
            fprintf(file, "\n");
            fflush(file);
            fclose(file);
        }
    }

    free(bname);
}

#endif
