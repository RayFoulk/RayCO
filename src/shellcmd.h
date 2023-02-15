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
// Command handler function signature.
typedef void * (*shellcmd_handler_f) (void * shell,
                                      void * context,
                                      int argc,
                                      char ** args);

//------------------------------------------------------------------------|
typedef struct shellcmd_t
{
    // Shell command factory function
    struct shellcmd_t * (*create)(shellcmd_handler_f handler,
                                  const char * keyword,
                                  const char * arghints,
                                  const char * description);

    // Shell command destructor function
    void (*destroy)(void * shellcmd);

    // callback function for searching through registered commands
    // Signature must match what is used with qsort()
    // TODO: Make this private if possible later
    //int (*keyword_compare)(const void * shellcmd1,
    //                       const void * shellcmd2);

    // Find a registered command by keyword
    struct shellcmd_t * (*find_by_keyword)(struct shellcmd_t * shellcmd,
                                           const char * keyword);

    // Register a sub-command within the context of this command.
    // If this is serving as the root-level command, then this
    // represents a base level command
    bool (*register_cmd)(struct shellcmd_t * parent,
                         struct shellcmd_t * child);

    // Unregister a command from this context
    bool (*unregister_cmd)(struct shellcmd_t * parent,
                           struct shellcmd_t * child);

    // Private data
    void * priv;
}
shellcmd_t;

//------------------------------------------------------------------------|
// Public shell command interface
extern const shellcmd_t shellcmd_pub;