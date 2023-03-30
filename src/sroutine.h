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

//------------------------------------------------------------------------|
// A scallop routine is something like a source script except that it
// resides in memory, and is defined by the keyword 'routine' and
// finalized by the keyword 'end'.  routines are going to need to be able
// to accept arguments from when they are called, and to evaluate their
// values in-place at arbitrary points of execution.
typedef struct scallop_rtn_t
{
    // Routine factory function
    struct scallop_rtn_t * (*create)(const char * name);

    // Scallop destructor function
    void (*destroy)(void * routine);

    // Name comparator for finding routines in a chain
    int (*compare_name)(const void * routine, const void * other);

    // Get the name of the routine
    const char * (*name)(struct scallop_rtn_t * routine);

    // Append a line to the routine
    void (*append)(struct scallop_rtn_t * routine, const char * line);

    // Execute the routine with arguments
    int (*handler)(void * scmd, void * context, int argc, char ** args);

    // Private data
    void * priv;
}
scallop_rtn_t;

//------------------------------------------------------------------------|
extern const scallop_rtn_t scallop_rtn_pub;
