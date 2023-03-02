//------------------------------------------------------------------------|
// Copyright (c) 2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

#include <stdio.h>      // FILE
#include <stdbool.h>    // bool

//------------------------------------------------------------------------|
#define CONSOLE_BUFFER_SIZE     4096

//------------------------------------------------------------------------|
typedef struct console_t
{
    // Console factory function
    struct console_t * (*create)(FILE * input, FILE * output);

    // Console destructor
    void (*destroy)(void * console);

    // Explicitly lock the console from current thread
    bool (*lock)(struct console_t * console);

    // Explicitly unlock the console from current thread
    void (*unlock)(struct console_t * console);

    // Get a line of input
    ssize_t (*getline)(struct console_t * console, char ** lineptr, size_t * nchars);

    // Printf-style output function
    int (*print)(struct console_t * console, const char * format, ...);

    // Update a previously printed string in-place
    int (*reprint)(struct console_t * console, const char * format, ...);

    // Private data
    void * priv;
}
console_t;

//------------------------------------------------------------------------|
// Public console interface
extern const console_t console_pub;
