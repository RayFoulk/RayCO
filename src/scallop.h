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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "console.h"
#include "scallcmd.h"

//------------------------------------------------------------------------|
// Maximum number of individual arguments
#define SCALLOP_MAX_ARGS          32

// Maximum recursion depth to avoid stack smashing
#define SCALLOP_MAX_RECURS        64

//------------------------------------------------------------------------|
typedef struct scallop_t
{
    struct scallop_t * (*create)(console_t * console,
                                 const char * prompt,
                                 const char * delim,
                                 const char * comment);

    // Shell destructor function
    void (*destroy)(void * scallop);

    // Get access to the command registry interface.
    // This is necessary for third-party command registration!
    scallop_cmd_t * (*commands)(struct scallop_t * scallop);

    // Handle a raw line of input, calling whatever
    // handler functions are necessary.
    int (*dispatch)(struct scallop_t * scallop, char * line);

    // Main interactive prompt loop
    int (*loop)(struct scallop_t * scallop);

    // Explicitely quit the main loop
    void (*quit)(struct scallop_t * scallop);

    // TODO: get/set context-based prompt

    // Private data
    void * priv;
}
scallop_t;

//------------------------------------------------------------------------|
// Public scallop interface
extern const scallop_t scallop_pub;
