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
#define BLAMMO(msgt, fmt, ...)  blammo(__FUNCTION__, msgt, fmt, \
                                       ## __VA_ARGS__)

//------------------------------------------------------------------------|
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
//#include <types.h>
#include <time.h>

//------------------------------------------------------------------------|
typedef enum
{
    INFO    = 0,
    DEBUG   = 1,
    WARNING = 2,
    ERROR   = 3
}
blammo_msg_t;

//------------------------------------------------------------------------|
void blammo_file(const char * filename);
void blammo_level(blammo_msg_t level);
void blammo(const char * func, const blammo_msg_t type,
            const char * format, ...);

#endif // #ifdef BLAMMO_ENABLE
