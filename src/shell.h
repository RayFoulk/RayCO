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
// Command handler function signature
typedef void * (*cmd_handler_f) (void * object,
                                 void * context,
                                 int argc,
                                 char ** args);

//------------------------------------------------------------------------|
typedef struct shell_t
{
    struct shell_t * (*create)(const char * prompt);

    // Shell destructor function
    void (*destroy)(void * shell);

    // Register a command
    void * (*register_cmd)(struct shell_t * shell,
                           void * context,
                           cmd_handler_f handler,
                           const char * keyword,
                           const char * arghints,
                           const char * description);

    // Unregister a command
    void * (*unregister_cmd)(struct shell_t * shell,
                             void * context,
                             const char * keyword);

    // Main interactive prompt loop
    int (*loop)(struct shell_t * shell);

    // TODO: get/set prompt

    // Private data
    void * priv;
}
shell_t;

//------------------------------------------------------------------------|
// Public shell interface
extern const shell_t shell_pub;
