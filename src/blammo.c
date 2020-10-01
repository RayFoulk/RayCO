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
    // Pointer to the log file (if specified)
    FILE * fileptr;

    // The blammo log level
    blammo_msg_t level;
}
blammo_data_t;

//------------------------------------------------------------------------|
static blammo_data_t blammo_data = { NULL, ERROR };

//------------------------------------------------------------------------|
// get a log-friendly timestamp string for current time.  This is NOT
// thread-safe or re-entrant safe
static void timestamp (char * ts, size_t size)
{
    time_t currtime = time(NULL);
    struct tm * tmp = localtime(&currtime);
    strftime (ts, size, "%F %T", tmp);
}

//------------------------------------------------------------------------|
void blammo_file(const char * filename)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");

    // TODO: touch the file for write, report error if unable
    // set FILE * in singleton and use in blammo() if successful

}

//------------------------------------------------------------------------|
inline void blammo_level(blammo_msg_t level)
{
    blammo_data.level = level;
}

//------------------------------------------------------------------------|
void blammo(const char * func, const blammo_msg_t type,
            const char * format, ...)
{
    // If the message doesn't rise to the set level, then discard it
    if (type < blammo_data.level)
    {
        return;
    }

    // TODO: Consider mutex here if this logger is to be thread-safe

    char ts[48];
    va_list args;

    timestamp(ts, 48);
    fprintf(stdout, "%s %s ", ts, blammo_msg_t_str[type]);

    va_start(args, format);
    vfprintf(stdout, format, args); 
    va_end(args);

    fprintf(stdout, "\n");
}

#endif
