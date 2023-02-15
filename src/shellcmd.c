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

#include "shellcmd.h"
#include "chain.h"
#include "blammo.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

//------------------------------------------------------------------------|
// Container for a command
typedef struct
{
    // list of sub-commands to this command, allowing for recursive parsing
    // For example this allows commands like the following to exist and
    // have clean implementations:
    // 'log console off', 'log console on', 'log file ./test.log'
    // 'list objects' or 'list types' -- rather than ugly case-switching.
    // This can be NULL if there are no sub-commands
    chain_t * cmds;

    // command handler function taking arguments are returning int
    shellcmd_handler_f handler;

    // Note that this container does NOT own the memory for the command
    // keyword, argument hints, or description.  This is assumed to be
    // initialized from a string constant I.E:
    // register_cmd("something", "hints", ...);
    // This should normally be fine because the handler function must also
    // be known at compile-time (since C has no reflection).
    // command keyword string
    char * keyword;

    // argument hint strings
    char * arghints;

    // description of what the command does
    char * description;
}
shellcmd_priv_t;

//------------------------------------------------------------------------|
static shellcmd_t * shellcmd_create(shellcmd_handler_f handler,
                                    const char * keyword,
                                    const char * arghints,
                                    const char * description)
{
    // Allocate and initialize public interface
    shellcmd_t * cmd = (shellcmd_t *) malloc(sizeof(shellcmd_t));
    if (!cmd)
    {
        BLAMMO(FATAL, "malloc(sizeof(shellcmd_t)) failed");
        return NULL;
    }

    // Bulk copy all function pointers and initialize opaque ptr
    memcpy(cmd, &shellcmd_pub, sizeof(shellcmd_t));

    // Allocate and initialize private implementation
    cmd->priv = malloc(sizeof(shellcmd_priv_t));
    if (!cmd->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(shellcmd_priv_t)) failed");
        free(cmd);
        return NULL;
    }

    memset(cmd->priv, 0, sizeof(shellcmd_priv_t));
    shellcmd_priv_t * priv = (shellcmd_priv_t *) cmd->priv;

    // Most commands are likely not going to have recursive
    // sub-commands, so don't waste memory allocating a bunch
    // of empty lists everywhere.  Do lazy/late initialization
    // of the sub-commands list in register()
    //priv->cmds = NULL;  // redundant

    // Initialize most members
    priv->handler = handler;
    priv->keyword = (char *) keyword;
    priv->arghints = (char *) arghints;
    priv->description = (char *) description;

    return cmd;
}

//------------------------------------------------------------------------|
static void shellcmd_destroy(void * shellcmd)
{
    shellcmd_t * cmd = (shellcmd_t *) shellcmd;

    // guard against accidental double-destroy or early-destroy
    if (!cmd || !cmd->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    shellcmd_priv_t * priv = (shellcmd_priv_t *) cmd->priv;

    // Recursively destroy command tree, if there are any nodes
    if (priv->cmds)
    {
        priv->cmds->destroy(priv->cmds);
        //priv->cmds = NULL;  // redundant
    }

    // zero out and destroy the private data
    memset(cmd->priv, 0, sizeof(shellcmd_priv_t));
    free(cmd->priv);

    // zero out and destroy the public interface
    memset(cmd, 0, sizeof(shellcmd_t));
    free(cmd);
}

//------------------------------------------------------------------------|
static int shellcmd_keyword_compare(const void * shellcmd1,
                                    const void * shellcmd2)
{
    shellcmd_t * cmd1 = (shellcmd_t *) shellcmd1;
    shellcmd_priv_t * priv1 = (shellcmd_priv_t *) cmd1->priv;

    shellcmd_t * cmd2 = (shellcmd_t *) shellcmd2;
    shellcmd_priv_t * priv2 = (shellcmd_priv_t *) cmd2->priv;

    // Commands are identified through their keyword,
    // which must be unique within the same context
    // (keywords in different contexts can be reused)
    // FIXME: Danger of buffer overrun here.
    return strcmp(priv1->keyword, priv2->keyword);
}

//------------------------------------------------------------------------|
static shellcmd_t * shellcmd_find_by_keyword(shellcmd_t * shellcmd,
                                             const char * keyword)
{
    shellcmd_priv_t * priv = (shellcmd_priv_t *) shellcmd->priv;

    if (!priv->cmds)
    {
        BLAMMO(VERBOSE, "Empty command registry");
        return NULL;
    }

    // create a temporary stack private data for search purposes
    shellcmd_t cmd_to_find;
    shellcmd_priv_t priv_to_find;

    cmd_to_find.priv = &priv_to_find;
    priv_to_find.keyword = (char *) keyword;

    // Do the search
    return (shellcmd_t *) priv->cmds->find(priv->cmds,
                                           &cmd_to_find,
                                           shellcmd_keyword_compare);
}

//------------------------------------------------------------------------|
bool shellcmd_register_cmd(shellcmd_t * parent,
                           shellcmd_t * child)
{
    shellcmd_priv_t * priv = (shellcmd_priv_t *) parent->priv;

    // Do some checks before inserting the child command
    if (!priv->cmds)
    {
        // Allocate the list of sub-commands if it does not
        // already exist.  It will not for terminal tree links
        priv->cmds = chain_pub.create(child->destroy);
        if (!priv->cmds)
        {
            BLAMMO(FATAL, "chain_pub.create() failed");
            return false;
        }
        // command chain is new and empty.  safe to insert without search.
    }
    else
    {
        // command chain already exists.  must search before insert.
        // ensure that the requested keyword is unique within the given context.
        shellcmd_t * found = (shellcmd_t *) priv->cmds->find(priv->cmds,
                                                             child,
                                                             shellcmd_keyword_compare);

        if (found)
        {
            BLAMMO_DECLARE(shellcmd_priv_t * found_priv = (shellcmd_priv_t *) found->priv);
            BLAMMO(ERROR, "Command \'%s\' already registered", found_priv->keyword);
            return false;
        }
        // new command not found in existing chain.  safe to insert
    }

    // Insert the new command link
    priv->cmds->insert(priv->cmds, child);
    return true;
}

//------------------------------------------------------------------------|
bool shellcmd_unregister_cmd(shellcmd_t * parent,
                             shellcmd_t * child)
{
    shellcmd_priv_t * priv = (shellcmd_priv_t *) parent->priv;

    if (!priv->cmds)
    {
        BLAMMO(VERBOSE, "Empty command registry");
        return false;
    }

    // Do the search for the child command
    shellcmd_t * found = (shellcmd_t *) priv->cmds->find(priv->cmds,
                                                         child,
                                                         shellcmd_keyword_compare);

    if (!found)
    {
        BLAMMO_DECLARE(shellcmd_priv_t * child_priv = (shellcmd_priv_t *) child->priv)
        BLAMMO(ERROR, "Command \'%s\' not found", child_priv->keyword);
        return false;
    }

    // Remove the found command link
    priv->cmds->remove(priv->cmds);
    return true;
}

//------------------------------------------------------------------------|
const shellcmd_t shellcmd_pub = {
    &shellcmd_create,
    &shellcmd_destroy,
    //&shellcmd_keyword_compare,
    &shellcmd_find_by_keyword,
    &shellcmd_register_cmd,
    &shellcmd_unregister_cmd,
    NULL
};