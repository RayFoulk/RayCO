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

#include "chain.h"          // chain_t

//------------------------------------------------------------------------|
// Command handler function signature.
typedef int (*scallop_cmd_handler_f) (void * scallcmd,
                                      void * context,
                                      int argc,
                                      char ** args);

//------------------------------------------------------------------------|
typedef struct scallop_cmd_t
{
    // Shell command factory function
    struct scallop_cmd_t * (*create)(scallop_cmd_handler_f handler,
                                     void * context,
                                     const char * keyword,
                                     const char * arghints,
                                     const char * description);

    // Shell command destructor function
    void (*destroy)(void * scallcmd);

    // Find a registered command by keyword
    struct scallop_cmd_t * (*find_by_keyword)(struct scallop_cmd_t * scallcmd,
                                             const char * keyword);

    // Get a list of partially matching keywords.  The returned chain_t
    // instance is expected to be destroyed by the caller.
    struct chain_t * (*partial_matches)(struct scallop_cmd_t * scallcmd,
                                        const char * substring,
                                        size_t * longest);

    // Execute the command's handler function with args
    int (*exec)(struct scallop_cmd_t * scallcmd,
                int argc,
                char ** args);

    // Get keyword for _this_ command
    const char * (*keyword)(struct scallop_cmd_t * scallcmd);

    // Get argument hints for _this_ command
    const char * (*arghints)(struct scallop_cmd_t * scallcmd);

    // Get description for _this_ command
    const char * (*description)(struct scallop_cmd_t * scallcmd);

    // Recursively get full help text for _this_ and all sub-commands
    int (*help)(struct scallop_cmd_t * scallcmd,
                char ** helptext,
                size_t * size);

    // Register a sub-command within the context of this command.
    // If this is serving as the root-level command, then this
    // represents a base level command
    bool (*register_cmd)(struct scallop_cmd_t * parent,
                         struct scallop_cmd_t * child);

    // Unregister a command from this context
    bool (*unregister_cmd)(struct scallop_cmd_t * parent,
                           struct scallop_cmd_t * child);

    // Private data
    void * priv;
}
scallop_cmd_t;

//------------------------------------------------------------------------|
// Public scallop command interface
extern const scallop_cmd_t scallop_cmd_pub;
