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
#include "scommand.h"

//------------------------------------------------------------------------|
// Maximum number of individual arguments
#define SCALLOP_MAX_ARGS          32

// Maximum recursion depth to avoid stack smashing
#define SCALLOP_MAX_RECURS        64

//------------------------------------------------------------------------|
// Language construct line handler function signature
typedef int (*scallop_construct_line_f)(void * context,
                                        void * object,
                                        char * line);

// Language construct stack pop handler function signature.
typedef int (*scallop_construct_pop_f)(void * context,
                                       void * object);

//------------------------------------------------------------------------|
typedef struct scallop_t
{
    struct scallop_t * (*create)(console_t * console,
                                 const char * prompt_base);

    // Scallop destructor function
    void (*destroy)(void * scallop);

    // Get access to the console object
    console_t * (*console)(struct scallop_t * scallop);

    // Get access to the command registry interface.
    // This is necessary for third-party command registration!
    scallop_cmd_t * (*commands)(struct scallop_t * scallop);

    // Get access to the list of routines, so that builtins
    // can define new ones or alter existing ones as needed.
    chain_t * (*routines)(struct scallop_t * scallop);

    // Handle a raw line of input, calling whatever
    // handler functions are necessary.
    int (*dispatch)(struct scallop_t * scallop, char * line);

    // Main interactive prompt loop
    int (*loop)(struct scallop_t * scallop, bool interactive);

    // Explicitely quit the main loop
    void (*quit)(struct scallop_t * scallop);

    // Push a full context onto the context stack
    void (*construct_push)(struct scallop_t * scallop,
                         const char * name,
                         void * context,
                         void * object,
                         scallop_construct_line_f linefunc,
                         scallop_construct_pop_f popfunc);

    // Pop a context name off the top of the context stack
    int (*construct_pop)(struct scallop_t * scallop);

    // Get the name of the current context (top of stack).
    // Returns NULL when on "bottom" (which is actually
    // the base prompt left untouched)
    const char * (*construct_name)(struct scallop_t * scallop);

    // Private data
    void * priv;
}
scallop_t;

//------------------------------------------------------------------------|
// Public scallop interface
extern const scallop_t scallop_pub;
