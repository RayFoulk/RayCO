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
#include "bytes.h"          // bytes_t

//------------------------------------------------------------------------|
// Bit-flags for command attributes.  It's likely safe to assume the
// compiler will use at least 16 bits for this type, even on many 8-bit
// systems/compilers.  Not many are needed, but keep a few in reserve
// for potential user-implementation use cases later on.
typedef enum
{
    SCALLOP_CMD_ATTR_NONE       = 0,

    // Whether the command is an alias to another.
    SCALLOP_CMD_ATTR_ALIAS      = (1 << 0),

    // Whether the command can be dynamically unregistered or altered
    // at runtime as part of a handler's actions.  Most commands are
    // compiled in and registered at program initialization, and are not
    // mutable.  Examples of mutable commands are routines and aliases.
    SCALLOP_CMD_ATTR_MUTABLE    = (1 << 1),

    // Whether the command is part of a multi-line language construct
    // that is pushed onto the construct stack.  These are handled
    // differently than single-line commands in dispatch().  Examples
    // of language constructs include 'routine' and 'end', and anything
    // that causes a construct stack push or pop.
    SCALLOP_CMD_ATTR_CONSTRUCT  = (1 << 2)

    // All further higher bits are reserved for later use or special-case
    // implementations that use scallop as a CLI toolkit.  Those should
    // probably start at bit 16 or 8 and work their way down to avoid
    // conflict in future revisions of scallop.
}
scallop_cmd_attr_t;

//------------------------------------------------------------------------|
// Command handler function signature.
typedef int (*scallop_cmd_handler_f) (void * cmd,
                                      void * context,
                                      int argc,
                                      char ** args);

//------------------------------------------------------------------------|
typedef struct scallop_cmd_t
{
    // Scallop command factory function
    struct scallop_cmd_t * (*create)(scallop_cmd_handler_f handler,
                                     void * context,
                                     const char * keyword,
                                     const char * arghints,
                                     const char * description);

    // Shell command destructor function
    void (*destroy)(void * cmd);

    // Shell command copy function
    // Caller is responsible for destroying the copy.
    void * (*copy)(const void * cmd);

    // Psuedo-factory: Create an alias of an existing command
    struct scallop_cmd_t * (*alias)(struct scallop_cmd_t * cmd,
                                    const char * keyword);

    // Find a registered command by keyword
    struct scallop_cmd_t * (*find_by_keyword)(struct scallop_cmd_t * cmd,
                                              const char * keyword);

    // Get a list of partially matching keywords.  The returned chain_t
    // instance is expected to be destroyed by the caller.
    struct chain_t * (*partial_matches)(struct scallop_cmd_t * cmd,
                                        const char * substring,
                                        size_t * longest);

    // Execute the command's handler function with args
    int (*exec)(struct scallop_cmd_t * cmd,
                int argc,
                char ** args);

    // The majority of commands should be non-mutable & non-construct
    // that is: created at program initialization for the lifetime
    // of the process, and not a special part of a multi-line sequence.
    // Use these cautiously: intended to be called once after creation.
    // Once attribute bits are set they are not intended to be unset.
    void (*set_attributes)(struct scallop_cmd_t * cmd,
                           scallop_cmd_attr_t attributes);

    // Get whether this command is an alias to another command
    bool (*is_alias)(struct scallop_cmd_t * cmd);

    // Get whether this command was registered at runtime, and can
    // be unregistered or redefined.
    bool (*is_mutable)(struct scallop_cmd_t * cmd);

    // Get whether this command is part of a multi-line language construct
    bool (*is_construct)(struct scallop_cmd_t * cmd);

    // Get keyword for _this_ command
    const char * (*keyword)(struct scallop_cmd_t * cmd);

    // Get argument hints for _this_ command
    const char * (*arghints)(struct scallop_cmd_t * cmd);

    // Get description for _this_ command
    const char * (*description)(struct scallop_cmd_t * cmd);

    // Find the longest length fields in the command tree
    void (*longest)(struct scallop_cmd_t * pcmd,
                    size_t * keyword_plus_arghints_longest,
                    size_t * keyword_longest,
                    size_t * arghints_longest,
                    size_t * description_longest);

    // Recursively get full help text for _this_ and all sub-commands
    int (*help)(struct scallop_cmd_t * cmd,
                bytes_t * help,
                size_t depth,
                size_t longest_kw_and_hints);

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
