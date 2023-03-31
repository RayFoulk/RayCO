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

#include "scommand.h"
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

    // Whether the command can be dynamically unregistered or altered
    // at runtime as part of a handler's actions, normal shutdown exluded
    bool is_mutable;

    // Whether the command is part of a multi-line language construct
    // that is pushed onto the construct stack.  If so, it must be handled
    // by dispatch --> exec() regardless of whether we are in the middle
    // of processing/defining a contruct. Examples are 'routine' and 'end'
    // and anything that causes a construct stack push or pop.
    bool is_construct;

    // command handler function taking arguments are returning int
    scallop_cmd_handler_f handler;

    // A generic 'context' pointer.  This part will vary according
    // to the specific use case of the command being handled.
    // This is simply carried here and passed to the handler.
    // It may be NULL if the handler doesn't use it.
    void * context;

    // this object must own the memory for keyword, arghints, and
    // description, because in some cases the original source can
    // be volatile, as in a heap allocated command line.  Particularly
    // the alias command and potentially others to follow.

    // command keyword string
    bytes_t * keyword;

    // argument hint strings
    bytes_t * arghints;

    // description of what the command does
    bytes_t * description;
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
    //priv->cmds = NULL;  // redundant due to earlier memset()

    // Also every command created by create() is immutable and non-construct
    // by default until permanently set otherwise.

    // Initialize most members
    priv->handler = handler;
    priv->context = context;
    priv->keyword = bytes_pub.create(keyword,
                                     keyword ? strlen(keyword) : 0);
    priv->arghints = bytes_pub.create(arghints,
                                      arghints ? strlen(arghints) : 0);
    priv->description = bytes_pub.create(description,
                                         description ? strlen(description) : 0);

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

    // destroy managed strings
    priv->description->destroy(priv->description);
    priv->arghints->destroy(priv->arghints);
    priv->keyword->destroy(priv->keyword);

    // Recursively destroy command tree, if there are any nodes,
    // and IF the pointer to those nodes is not an alias copy
    if (priv->cmds && !priv->is_mutable)
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
static scallop_cmd_t * scallop_cmd_alias(scallop_cmd_t * scallcmd,
                                         const char * keyword)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    bytes_t * description = bytes_pub.create(NULL, 0);

    // Make a mostly-copy of the original command with a few things
    // distinctly changed.  It should function the same but with a
    // new keyword.  Scope is not determined here.
    description->print(description,
                       "alias for %s",
                       priv->keyword->cstr(priv->keyword));

    scallop_cmd_t * alias = scallcmd->create(priv->handler,
                                             priv->context,
                                             keyword,
                                             priv->arghints->cstr(priv->arghints),
                                             description->cstr(description));

    // All aliases can be unregistered, but construct depends
    // on the original command.
    alias->set_mutable(alias);
    if (priv->is_construct)
    {
        alias->set_construct(alias);
    }


    // Manually copy the sub-command pointer to the original
    // command being aliased so that things will work properly.
    // Be cautious when destroying/unregistering objects to not
    // impropoerly corrupt the original!  Even this, in principal,
    // should not segfault if gaurd-blocks are in place, but it
    // could lead to some weird unexpected behavior.
    scallop_cmd_priv_t * alias_priv = (scallop_cmd_priv_t *) alias->priv;
    alias_priv->cmds = priv->cmds;

    // Don't need this temporary buffer anymore
    description->destroy(description);

    return alias;
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
    return priv1->keyword->compare(priv1->keyword, priv2->keyword);
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

    memset(&cmd_to_find, 0, sizeof(scallop_cmd_t));
    memset(&priv_to_find, 0, sizeof(scallop_cmd_priv_t));

    cmd_to_find.priv = &priv_to_find;
    priv_to_find.keyword = bytes_pub.create(keyword, strlen(keyword));

    // Do the search
    scallop_cmd_t * cmd = (scallop_cmd_t *)
            priv->cmds->find(priv->cmds,
                             &cmd_to_find,
                             scallop_cmd_keyword_compare);

    priv_to_find.keyword->destroy(priv_to_find.keyword);
    return cmd;
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
    }
    while (priv->cmds->spin(priv->cmds, 1));

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
static void scallop_set_mutable(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    priv->is_mutable = true;
}

//------------------------------------------------------------------------|
static void scallop_set_construct(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    priv->is_construct = true;
}

//------------------------------------------------------------------------|
static inline bool scallop_cmd_is_mutable(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->is_mutable;
}

//------------------------------------------------------------------------|
static inline bool scallop_cmd_is_construct(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->is_construct;
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_keyword(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->keyword->cstr(priv->keyword) ?
           priv->keyword->cstr(priv->keyword) : "";
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_arghints(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->arghints->cstr(priv->arghints) ?
           priv->arghints->cstr(priv->arghints) : "";
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_description(scallop_cmd_t * scallcmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) scallcmd->priv;
    return priv->description->cstr(priv->description) ?
           priv->description->cstr(priv->description) : "";
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

    // Done if no sub-commands
    // Fix-it-twice:
    if (!priv->cmds || !priv->cmds->priv)
    {
        return 0;
    }

    // Recursively get help for all sub-commands
    pad = bytes_pub.create(NULL, depth * 4);
    pad->fill(pad, ' ');

    // show explicit context: parent command
    // FIXME: Does not build up a complete string including
    //  grandparents -- does not always show complete context!
    if (scallcmd->keyword(scallcmd))
    {
        pad->append(pad, scallcmd->keyword(scallcmd),
                    strlen(scallcmd->keyword(scallcmd)));
        pad->append(pad, " ", 1);
    }

    priv->cmds->reset(priv->cmds);
    do
    {
        cmd = (scallop_cmd_t *) priv->cmds->data(priv->cmds);
        subhelp = bytes_pub.create(NULL, 0);

        subhelp->print(subhelp,
                       "%s%s  %s  %s\r\n",
                       pad->cstr(pad) ? pad->cstr(pad) : "",
                       cmd->keyword(cmd),
                       cmd->arghints(cmd),
                       cmd->description(cmd));

        cmd->help(cmd, subhelp, ++depth);
        depth--;

        help->append(help,
                     subhelp->data(subhelp),
                     subhelp->size(subhelp));

        subhelp->destroy(subhelp);
    }
    while (priv->cmds->spin(priv->cmds, 1));

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

    // Insert the new command link.  Let items appear in the order
    // they were registered
    priv->cmds->reset(priv->cmds);
    priv->cmds->spin(priv->cmds, -1);
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

    // Remove the found command link -- this also destroys the found command
    priv->cmds->remove(priv->cmds);

    //  TODO: FIXME: Use extra caution with
    //  unregistereing/destroying alias'ed commands as the sub-command
    //  link may be a duplicate pointer to the original command!

    return true;
}

//------------------------------------------------------------------------|
const scallop_cmd_t scallop_cmd_pub = {
    &scallop_cmd_create,
    &scallop_cmd_destroy,
    &scallop_cmd_alias,
    &scallop_cmd_find_by_keyword,
    &scallop_cmd_partial_matches,
    &scallop_cmd_exec,
    &scallop_set_mutable,
    &scallop_set_construct,
    &scallop_cmd_is_mutable,
    &scallop_cmd_is_construct,
    &scallop_cmd_keyword,
    &scallop_cmd_arghints,
    &scallop_cmd_description,
    &scallop_cmd_help,
    &scallop_cmd_register_cmd,
    &scallop_cmd_unregister_cmd,
    NULL
};
