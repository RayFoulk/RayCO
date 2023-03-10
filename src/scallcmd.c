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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

#include "scallcmd.h"
#include "chain.h"
#include "bytes.h"
#include "blammo.h"

//------------------------------------------------------------------------|
// Container for a command - private data
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
    scallop_cmd_handler_f handler;

    // A generic 'context' pointer.  This part will vary according
    // to the specific use case of the command being handled.
    // This is simply carried here and passed to the handler.
    // It may be NULL if the handler doesn't use it.
    void * context;

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
scallop_cmd_priv_t;

//------------------------------------------------------------------------|
static scallop_cmd_t * scallop_cmd_create(scallop_cmd_handler_f handler,
                                          void * context,
                                          const char * keyword,
                                          const char * arghints,
                                          const char * description)
{
    // Allocate and initialize public interface
    scallop_cmd_t * cmd = (scallop_cmd_t *) malloc(sizeof(scallop_cmd_t));
    if (!cmd)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_cmd_t)) failed");
        return NULL;
    }

    // Bulk copy all function pointers and initialize opaque ptr
    memcpy(cmd, &scallop_cmd_pub, sizeof(scallop_cmd_t));

    // Allocate and initialize private implementation
    cmd->priv = malloc(sizeof(scallop_cmd_priv_t));
    if (!cmd->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_cmd_priv_t)) failed");
        free(cmd);
        return NULL;
    }

    memset(cmd->priv, 0, sizeof(scallop_cmd_priv_t));
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    // Most commands are likely not going to have recursive
    // sub-commands, so don't waste memory allocating a bunch
    // of empty lists everywhere.  Do lazy/late initialization
    // of the sub-commands list in register()
    //priv->cmds = NULL;  // redundant

    // Initialize most members
    priv->handler = handler;
    priv->context = context;
    priv->keyword = (char *) keyword;
    priv->arghints = (char *) arghints;
    priv->description = (char *) description;

    return cmd;
}

//------------------------------------------------------------------------|
static void scallop_cmd_destroy(void * scallcmd)
{
    scallop_cmd_t * cmd = (scallop_cmd_t *) scallcmd;

    // guard against accidental double-destroy or early-destroy
    if (!cmd || !cmd->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    // Recursively destroy command tree, if there are any nodes
    if (priv->cmds)
    {
        priv->cmds->destroy(priv->cmds);
        //priv->cmds = NULL;  // redundant
    }

    // zero out and destroy the private data
    memset(cmd->priv, 0, sizeof(scallop_cmd_priv_t));
    free(cmd->priv);

    // zero out and destroy the public interface
    memset(cmd, 0, sizeof(scallop_cmd_t));
    free(cmd);
}

//------------------------------------------------------------------------|
static int scallop_cmd_keyword_compare(const void * scallcmd1,
                                       const void * scallcmd2)
{
    scallop_cmd_t * cmd1 = (scallop_cmd_t *) scallcmd1;
    scallop_cmd_priv_t * priv1 = (scallop_cmd_priv_t *) cmd1->priv;

    scallop_cmd_t * cmd2 = (scallop_cmd_t *) scallcmd2;
    scallop_cmd_priv_t * priv2 = (scallop_cmd_priv_t *) cmd2->priv;

    // Commands are identified through their keyword,
    // which must be unique within the same context
    // (keywords in different contexts can be reused)
    // FIXME: Danger of buffer overrun here.
    return strcmp(priv1->keyword, priv2->keyword);
}

//------------------------------------------------------------------------|
static scallop_cmd_t * scallop_cmd_find_by_keyword(scallop_cmd_t * scallcmd,
                                                  const char * keyword)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;

    if (!priv->cmds)
    {
        BLAMMO(VERBOSE, "Empty command registry");
        return NULL;
    }

    // create a temporary stack private data for search purposes
    scallop_cmd_t cmd_to_find;
    scallop_cmd_priv_t priv_to_find;

    cmd_to_find.priv = &priv_to_find;
    priv_to_find.keyword = (char *) keyword;

    // Do the search
    return (scallop_cmd_t *) priv->cmds->find(priv->cmds,
                                           &cmd_to_find,
                                           scallop_cmd_keyword_compare);
}

static chain_t * scallop_cmd_partial_matches(scallop_cmd_t * scallcmd,
                                             const char * substring,
                                             size_t * longest)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    scallop_cmd_t * cmd = NULL;
    chain_t * pmatches = chain_pub.create(NULL);

    // substring might be NULL in some cases like when hitting
    // tab after an already completed keyword without argument,
    // but also the scallcmd could be a terminator in the
    // sub-command tree, in which case priv->cmds is NULL.
    if (!substring || !priv->cmds)
    {
        BLAMMO(DEBUG, "substring: %p  sub-commands: %p",
                      substring, priv->cmds);
        return NULL;
    }

    // Keep track of the longest match if the caller has asked.
    if (longest)
    {
        *longest = 0;
    }

    // Search through all sub-commands.
    size_t length = 0;
    size_t sublength = strlen(substring);
    priv->cmds->reset(priv->cmds);
    do
    {
        cmd = (scallop_cmd_t *) priv->cmds->data(priv->cmds);

        BLAMMO(DEBUG, "checking \'%s\' against \'%s\'",
                cmd->keyword(cmd), substring);
        if (!strncmp(cmd->keyword(cmd), substring, sublength))
        {
            pmatches->insert(pmatches, (void *) cmd->keyword(cmd));
            // Keep track of the longest match if the caller has asked.
            if (longest)
            {
                length = strlen(cmd->keyword(cmd));
                if (length > *longest) { *longest = length; }
            }
        }

        priv->cmds->spin(priv->cmds, 1);
    }
    while (!priv->cmds->origin(priv->cmds));

    return pmatches;
}

//------------------------------------------------------------------------|
static inline int scallop_cmd_exec(scallop_cmd_t * scallcmd,
                                   int argc,
                                   char ** args)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;

    return priv->handler ?
           priv->handler(scallcmd, priv->context, argc, args) :
           0;
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_keyword(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->keyword;
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_arghints(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->arghints;
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_description(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->description;
}

//------------------------------------------------------------------------|
static int scallop_cmd_help(scallop_cmd_t * scallcmd,
                            bytes_t * help,
                            size_t depth)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    scallop_cmd_t * cmd = NULL;
    bytes_t * subhelp = NULL;
    bytes_t * pad = NULL;

    // TODO: Append stuff about THIS command in the report???
    // probably not since starting at root with nothing.

    // Done if no sub-commands
    if (!priv->cmds)
    {
        return 0;
    }

    // Recursively get help for all sub-commands
    pad = bytes_pub.create("                      ", depth * 4);
    priv->cmds->reset(priv->cmds);
    do
    {
        cmd = (scallop_cmd_t *) priv->cmds->data(priv->cmds);
        subhelp = bytes_pub.create(NULL, 0);

        subhelp->print(subhelp,
                       "%s%s  %s  %s\r\n",
                       pad->data(pad) ? pad->data(pad) : "",
                       cmd->keyword(cmd),
                       cmd->arghints(cmd),
                       cmd->description(cmd));

        cmd->help(cmd, subhelp, ++depth);

        help->append(help,
                     subhelp->data(subhelp),
                     subhelp->size(subhelp));

        subhelp->destroy(subhelp);

        priv->cmds->spin(priv->cmds, 1);
    }
    while (!priv->cmds->origin(priv->cmds));

    pad->destroy(pad);
    return 0;
}

//------------------------------------------------------------------------|
bool scallop_cmd_register_cmd(scallop_cmd_t * parent,
                              scallop_cmd_t * child)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) parent->priv;

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
        scallop_cmd_t * found = (scallop_cmd_t *) priv->cmds->find(priv->cmds,
                                                             child,
                                                             scallop_cmd_keyword_compare);

        if (found)
        {
            BLAMMO_DECLARE(scallop_cmd_priv_t * found_priv = (scallop_cmd_priv_t *) found->priv);
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
bool scallop_cmd_unregister_cmd(scallop_cmd_t * parent,
                                scallop_cmd_t * child)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) parent->priv;

    if (!priv->cmds)
    {
        BLAMMO(VERBOSE, "Empty command registry");
        return false;
    }

    // Do the search for the child command
    scallop_cmd_t * found = (scallop_cmd_t *) priv->cmds->find(priv->cmds,
                                                              child,
                                                              scallop_cmd_keyword_compare);

    if (!found)
    {
        BLAMMO_DECLARE(scallop_cmd_priv_t * child_priv = (scallop_cmd_priv_t *) child->priv)
        BLAMMO(ERROR, "Command \'%s\' not found", child_priv->keyword);
        return false;
    }

    // Remove the found command link
    priv->cmds->remove(priv->cmds);
    return true;
}

//------------------------------------------------------------------------|
const scallop_cmd_t scallop_cmd_pub = {
    &scallop_cmd_create,
    &scallop_cmd_destroy,
    &scallop_cmd_find_by_keyword,
    &scallop_cmd_partial_matches,
    &scallop_cmd_exec,
    &scallop_cmd_keyword,
    &scallop_cmd_arghints,
    &scallop_cmd_description,
    &scallop_cmd_help,
    &scallop_cmd_register_cmd,
    &scallop_cmd_unregister_cmd,
    NULL
};
