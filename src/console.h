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
#include <stdlib.h>
#include <stddef.h>     // size_t
#include <stdbool.h>    // bool

//------------------------------------------------------------------------|
// Contextual callback functions for tab completion and arg hints.
// currently only linenoise is optionally supported.
typedef void (*console_tab_completion_f)(void * object,
                                         const char * buffer);

typedef char * (*console_arg_hints_f)(void * object,
                                      const char * buffer,
                                      int * color,
                                      int * bold);

//------------------------------------------------------------------------|
typedef struct console_t
{
    // Console factory function
    struct console_t * (*create)(FILE * input,
                                 FILE * output,
                                 const char * history_file);

    // Console destructor
    void (*destroy)(void * console);

    // Explicitly lock the console from current thread
    bool (*lock)(struct console_t * console);

    // Explicitly unlock the console from current thread
    void (*unlock)(struct console_t * console);

    // Set tab completion and arg hints callbacks (if supported)
    void (*set_line_callbacks)(struct console_t * console,
                               console_tab_completion_f tab_callback,
                               console_arg_hints_f hints_callback,
                               void * object);

    // Add a tab completion (if supported)
    void (*add_tab_completion)(struct console_t * console,
                               const char * line);

    // Get the current input or output pipe.
    // Opt for this approach rather than push/pop because
    // either we manage the stack or use the OS's call stack
    // within the script handling callback.
    FILE * (*get_inputf)(struct console_t * console);
    FILE * (*get_outputf)(struct console_t * console);

    // Directly set either of the pipes.  Caller is
    // responsible for not putting the console in a bad state.
    bool (*set_inputf)(struct console_t * console, FILE * input);
    bool (*set_outputf)(struct console_t * console, FILE * output);

    // Check for EOF on input file (typically end of script)
    bool (*inputf_eof)(struct console_t * console);

    // Get a heap allocated line buffer from user.  This is intentionally
    // simplistic attempting to match linenoise() as closely as possible.
    char * (*get_line)(struct console_t * console,
                       const char * prompt,
                       bool interactive);

    // Print (and log) a warning message: conforms to generic_print_f
    int (*warning)(void * console, const char * format, ...);

    // Print (and log) an error message: conforms to generic_print_f
    int (*error)(void * console, const char * format, ...);

    // Printf-style output function: conforms to generic_print_f
    int (*print)(void * console, const char * format, ...);

    // Update a previously printed string in-place: conforms to generic_print_f
    int (*reprint)(void * console, const char * format, ...);

    // Private data
    void * priv;
}
console_t;

//------------------------------------------------------------------------|
// Public console interface
extern const console_t console_pub;
