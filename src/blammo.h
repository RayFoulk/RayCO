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

#pragma once

//------------------------------------------------------------------------|
// only really enable the BLAMMO*() macros if the preprocessor directive
// BLAMMO_ENABLE is defined on the command line during build.  otherwise
// BLAMMO*() will be a set of empty macros.
#ifndef BLAMMO_ENABLE
#define BLAMMO_FILE(path)
#define BLAMMO_LEVEL(level)
#define BLAMMO(msgt, fmt, ...)

#else
#define BLAMMO_FILE(path)       blammo_file(path)
#define BLAMMO_LEVEL(level)     blammo_level(level)
#define BLAMMO(msgt, fmt, ...)  blammo(__FILE__, __LINE__, __FUNCTION__, \
                                       msgt, fmt, ## __VA_ARGS__)

//------------------------------------------------------------------------|
typedef enum
{
    // The most verbose & spammy messages, normally these would be kept
    // hidden (e.g. function entry/exit, checkpoints, etc.)
    VERBOSE = 0,

    // Debugging messages for troubleshooting & introspection (e.g.
    // hexdumps, object state reports, etc.)
    DEBUG   = 2,

    // Helpful informative messages.  These are things that the user would
    // normally want to see, and don't indicate a problem.
    INFO    = 1,

    // Warnings indicate that something unusual or unlikely happened, but
    // the program can continue normally.  There might be something the
    // user can do about them at the system or configuration level.
    WARNING = 3,

    // Errors indicate something went wrong.  It may be possible for the
    // program to continue or recover, possibly in a limited fashion.
    ERROR   = 4,

    // Fatal errors are when something is terribly wrong and the program
    // should exit immediately.  Examples may include malloc() failure,
    // stack corruption, or filesystem i/o failure.
    FATAL   = 5,
}
blammo_msg_t;

//------------------------------------------------------------------------|
void blammo_file(const char * filename);
void blammo_level(blammo_msg_t level);
void blammo(const char * fpath, int line, const char * func,
            const blammo_msg_t type, const char * format, ...);

#endif // #ifdef BLAMMO_ENABLE
