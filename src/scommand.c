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
#include "utils.h"              // memzero()
#include "blammo.h"
#include "chain.h"
#include "bytes.h"

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

    // Attribute flags for this command
    scallop_cmd_attr_t attributes;

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

    memzero(cmd->priv, sizeof(scallop_cmd_priv_t));
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    // Most commands are likely not going to have recursive
    // sub-commands, so don't waste memory allocating a bunch
    // of empty lists everywhere.  Do lazy/late initialization
    // of the sub-commands list in register()
    //priv->cmds = NULL;  // redundant due to earlier memzero()

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
static void scallop_cmd_destroy(void * cmd_ptr)
{
    scallop_cmd_t * cmd = (scallop_cmd_t *) cmd_ptr;

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
    // FIXME: This may not longer be correct given the distinction
    // between alias and mutable.!!
    if (priv->cmds && !(priv->attributes & SCALLOP_CMD_ATTR_ALIAS))
    {
        priv->cmds->destroy(priv->cmds);
        //priv->cmds = NULL;  // redundant
    }

    // zero out and destroy the private data
    memzero(cmd->priv, sizeof(scallop_cmd_priv_t));
    free(cmd->priv);

    // zero out and destroy the public interface
    memzero(cmd, sizeof(scallop_cmd_t));
    free(cmd);
}

//------------------------------------------------------------------------|
static void * scallop_cmd_copy(const void * cmd_ptr)
{
    scallop_cmd_t * cmd = (scallop_cmd_t *) cmd_ptr;
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    scallop_cmd_t * copy =
        scallop_cmd_pub.create(priv->handler,
                               priv->context,
                               priv->keyword->cstr(priv->keyword),
                               priv->arghints->cstr(priv->arghints),
                               priv->description->cstr(priv->description));

    scallop_cmd_priv_t * copy_priv = (scallop_cmd_priv_t *) copy->priv;

    copy_priv->attributes = priv->attributes;

    // Command may or may not have sub-commands
    copy_priv->cmds = priv->cmds ?
                            priv->cmds->copy(priv->cmds) :
                            NULL;

    return copy;
}

//------------------------------------------------------------------------|
static scallop_cmd_t * scallop_cmd_alias(scallop_cmd_t * cmd,
                                         const char * keyword)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    bytes_t * description = bytes_pub.create(NULL, 0);

    // Make a mostly-copy of the original command with a few things
    // distinctly changed.  It should function the same but with a
    // new keyword.  Scope is not determined here.
    description->print(description,
                       "alias for %s",
                       priv->keyword->cstr(priv->keyword));

    scallop_cmd_t * alias = cmd->create(priv->handler,
                                        priv->context,
                                        keyword,
                                        priv->arghints->cstr(priv->arghints),
                                        description->cstr(description));

    // All aliases ARE aliases, AND are mutable, but construct depends
    // on the original command.
    scallop_cmd_attr_t attributes =
            SCALLOP_CMD_ATTR_ALIAS | SCALLOP_CMD_ATTR_MUTABLE;

    if (priv->attributes & SCALLOP_CMD_ATTR_CONSTRUCT)
    {
        attributes |= SCALLOP_CMD_ATTR_CONSTRUCT;
    }

    alias->set_attributes(alias, attributes);

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
static int scallop_cmd_keyword_compare(const void * cmd_ptr1,
                                       const void * cmd_ptr2)
{
    scallop_cmd_t * cmd1 = (scallop_cmd_t *) cmd_ptr1;
    scallop_cmd_priv_t * priv1 = (scallop_cmd_priv_t *) cmd1->priv;

    scallop_cmd_t * cmd2 = (scallop_cmd_t *) cmd_ptr2;
    scallop_cmd_priv_t * priv2 = (scallop_cmd_priv_t *) cmd2->priv;

    // Commands are identified through their keyword,
    // which must be unique within the same context
    // (keywords in different contexts can be reused)
    return priv1->keyword->compare(priv1->keyword, priv2->keyword);
}

//------------------------------------------------------------------------|
static scallop_cmd_t * scallop_cmd_find_by_keyword(scallop_cmd_t * cmd,
                                                   const char * keyword)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    if (!priv->cmds)
    {
        BLAMMO(VERBOSE, "Empty command registry");
        return NULL;
    }

    // create a temporary stack private data for search purposes
    scallop_cmd_t cmd_to_find;
    scallop_cmd_priv_t priv_to_find;

    memzero(&cmd_to_find, sizeof(scallop_cmd_t));
    memzero(&priv_to_find, sizeof(scallop_cmd_priv_t));

    cmd_to_find.priv = &priv_to_find;
    priv_to_find.keyword = bytes_pub.create(keyword, strlen(keyword));

    // Do the search
    scallop_cmd_t * found = (scallop_cmd_t *)
            priv->cmds->find(priv->cmds,
                             &cmd_to_find,
                             scallop_cmd_keyword_compare);

    priv_to_find.keyword->destroy(priv_to_find.keyword);
    return found;
}

//------------------------------------------------------------------------|
static chain_t * scallop_cmd_partial_matches(scallop_cmd_t * cmd,
                                             const char * substring,
                                             size_t * longest)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    // 'pmatches' is a chain of external, unmanaged pointers.
    chain_t * pmatches = chain_pub.create(NULL, NULL);

    // substring might be NULL in some cases like when hitting
    // tab after an already completed keyword without argument,
    // but also the cmd could be a terminator in the
    // sub-command tree, in which case priv->cmds is NULL.
    if (!substring || !priv->cmds)
    {
        BLAMMO(DEBUG, "substring: %p  sub-commands: %p",
                      substring, priv->cmds);
        return NULL;
    }

    // Keep track of the longest match if the caller has asked.
    if (longest) { *longest = 0; }

    // Search through all sub-commands.
    size_t length = 0;
    size_t sublength = strlen(substring);
    scallop_cmd_t * subcmd = (scallop_cmd_t *) priv->cmds->first(priv->cmds);

    while (subcmd)
    {
        BLAMMO(DEBUG, "checking \'%s\' against \'%s\'",
                subcmd->keyword(subcmd), substring);
        if (!strncmp(subcmd->keyword(subcmd), substring, sublength))
        {
            pmatches->insert(pmatches, (void *) subcmd->keyword(subcmd));
            // Keep track of the longest match if the caller has asked.
            if (longest)
            {
                length = strlen(cmd->keyword(cmd));
                if (length > *longest) { *longest = length; }
            }
        }

        subcmd = (scallop_cmd_t *) priv->cmds->next(priv->cmds);
    }

    return pmatches;
}

//------------------------------------------------------------------------|
static inline int scallop_cmd_exec(scallop_cmd_t * cmd,
                                   int argc,
                                   char ** args)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;

    return priv->handler ?
           priv->handler(cmd, priv->context, argc, args) :
           0;
}

//------------------------------------------------------------------------|
static void scallop_set_attributes(scallop_cmd_t * cmd,
                                   scallop_cmd_attr_t attributes)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    priv->attributes |= attributes;
}

//------------------------------------------------------------------------|
static inline bool scallop_cmd_is_alias(scallop_cmd_t * cmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    return priv->attributes & SCALLOP_CMD_ATTR_ALIAS;
}

//------------------------------------------------------------------------|
static inline bool scallop_cmd_is_mutable(scallop_cmd_t * cmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    return priv->attributes & SCALLOP_CMD_ATTR_MUTABLE;
}

//------------------------------------------------------------------------|
static inline bool scallop_cmd_is_construct(scallop_cmd_t * cmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    return priv->attributes & SCALLOP_CMD_ATTR_CONSTRUCT;
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_keyword(scallop_cmd_t * cmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    return priv->keyword->cstr(priv->keyword) ?
           priv->keyword->cstr(priv->keyword) : "";
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_arghints(scallop_cmd_t * cmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    return priv->arghints->cstr(priv->arghints) ?
           priv->arghints->cstr(priv->arghints) : "";
}

//------------------------------------------------------------------------|
static inline const char * scallop_cmd_description(scallop_cmd_t * cmd)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) cmd->priv;
    return priv->description->cstr(priv->description) ?
           priv->description->cstr(priv->description) : "";
}

//------------------------------------------------------------------------|
static void scallop_cmd_longest(scallop_cmd_t * pcmd,
                                size_t * keyword_plus_arghints_longest,
                                size_t * keyword_longest,
                                size_t * arghints_longest,
                                size_t * description_longest)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) pcmd->priv;

    // Get lengths for this command node
    size_t keyword_len = priv->keyword->size(priv->keyword);
    size_t arghints_len = priv->arghints->size(priv->arghints);
    size_t keyword_plus_arghints_len = keyword_len + arghints_len;
    size_t description_len = priv->description->size(priv->description);

    // update longest where necessary
    if (keyword_plus_arghints_longest &&
        (keyword_plus_arghints_len > *keyword_plus_arghints_longest))
    {
        *keyword_plus_arghints_longest = keyword_plus_arghints_len;
    }

    if (keyword_longest && (keyword_len > *keyword_longest))
    {
        *keyword_longest = keyword_len;
    }

    if (arghints_longest && (arghints_len > *arghints_longest))
    {
        *arghints_longest = arghints_len;
    }

    if (description_longest && (description_len > *description_longest))
    {
        *description_longest = description_len;
    }

    // Done when we hit a terminal command node
    if (!priv->cmds || !priv->cmds->priv)
    {
        return;
    }

    // Now recursively descend on all sub-commands
    scallop_cmd_t * subcmd = priv->cmds->first(priv->cmds);
    while (subcmd)
    {
        subcmd->longest(subcmd, keyword_plus_arghints_longest,
                                keyword_longest,
                                arghints_longest,
                                description_longest);

        subcmd = (scallop_cmd_t *) priv->cmds->next(priv->cmds);
    }
}

//------------------------------------------------------------------------|
static int scallop_cmd_help(scallop_cmd_t * pcmd,
                            bytes_t * help,
                            size_t depth,
                            size_t longest_kw_and_hints)
{
    scallop_cmd_priv_t * priv = (scallop_cmd_priv_t *) pcmd->priv;
    bytes_t * keyword = priv->keyword;
    bytes_t * subhelp = NULL;
    bytes_t * indent = NULL;
    bytes_t * pad = NULL;

    // Done if no sub-commands. Fix-it-twice
    if (!priv->cmds || !priv->cmds->priv)
    {
        return 0;
    }

    indent = bytes_pub.create(NULL, depth * 4);
    indent->fill(indent, ' ');

    // FIXME: Padding for nested sub-commands does not factor into
    // the calculation for longest kw+hints, so the descriptions
    // are mis-aligned in the help screen.  this somehow needs
    // to be intrinsic behavior within recursive 'longest'.
    pad = bytes_pub.create(NULL, 0);

    // show full nested context: parent command
    // FIXME: Does not build up a complete string including
    //  grandparents -- does not always show complete context!
    //  would probably need to put parent links in all commands
    if (!keyword->empty(keyword))
    {
        BLAMMO(VERBOSE, "indentation for command: %s", keyword->cstr(keyword));
        indent->append(indent, keyword->data(keyword),
                               keyword->size(keyword));
        indent->append(indent, " ", 1);
    }

    // Recursively get help for all sub-commands
    scallop_cmd_priv_t * subpriv = NULL;
    scallop_cmd_t * subcmd = priv->cmds->first(priv->cmds);
    while (subcmd)
    {
        BLAMMO(VERBOSE, "getting help for sub-command: %s", subcmd->keyword(subcmd));
        subpriv = (scallop_cmd_priv_t *) subcmd->priv;

        // align the description column by the longest keyword+args
        pad->resize(pad, longest_kw_and_hints
                    - subpriv->keyword->size(subpriv->keyword)
                    - subpriv->arghints->size(subpriv->arghints)
                    + 4);
        pad->fill(pad, ' ');

        subhelp = bytes_pub.create(NULL, 0);
        subhelp->print(subhelp,
                       "%s%s%s%s%s\r\n",
                       indent->cstr(indent) ? indent->cstr(indent) : "",
                       subcmd->keyword(subcmd),
                       subcmd->arghints(subcmd),
                       pad->cstr(pad),
                       subcmd->description(subcmd));
        BLAMMO(VERBOSE, "subhelp->cstr: %s", subhelp->cstr(subhelp));

        subcmd->help(subcmd, subhelp, ++depth, longest_kw_and_hints);
        depth--;

        help->append(help,
                     subhelp->data(subhelp),
                     subhelp->size(subhelp));

        subhelp->destroy(subhelp);

        subcmd = priv->cmds->next(priv->cmds);
    }

    pad->destroy(pad);
    indent->destroy(indent);
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
        priv->cmds = chain_pub.create(child->copy,
                                      child->destroy);
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
        scallop_cmd_t * found = (scallop_cmd_t *)
                priv->cmds->find(priv->cmds,
                                 child,
                                 scallop_cmd_keyword_compare);

        if (found)
        {
            BLAMMO(ERROR, "Command \'%s\' already registered",
                    ((scallop_cmd_priv_t *) found->priv)->keyword);
            return false;
        }
        // new command not found in existing chain.  safe to insert
    }

    // Insert the new command link.  Let items appear in the order
    // they were registered
    priv->cmds->last(priv->cmds);
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
    scallop_cmd_t * found = (scallop_cmd_t *)
            priv->cmds->find(priv->cmds,
                             child,
                             scallop_cmd_keyword_compare);

    if (!found)
    {
        // TODO: Get a handle to console here.  Cannot guarantee
        // that the parent->context is going to be the grandparent
        // scallop instance.  Probably another reason to consider
        // putting parent links in every command. (see _help)
        BLAMMO(ERROR, "Command \'%s\' not found",
                ((scallop_cmd_priv_t *) child->priv)->keyword);
        return false;
    }

    // FIXME: When unregistering ANY command, search through the list
    // of all registered commands (within this scope) and also unregister
    // any and all aliases to that command.  Need to extract from 'found'
    // enough information to identify them in the list of commands.
    scallop_cmd_priv_t * other_priv = (scallop_cmd_priv_t *) found->priv;
    scallop_cmd_handler_f found_handler = other_priv->handler;

    // Remove the found command link -- this also destroys the found command
    priv->cmds->remove(priv->cmds);

#if 0
    // EXPERIMENTAL.  It's unclear what should happen if the alias
    // is to a routine, since all routines have the same handler.
    // this appears to be apples and oranges but some changes may
    // be necessary to support handling this case properly
    scallop_cmd_t * other = priv->cmds->first(priv->cmds);
    while (other)
    {
        other_priv = (scallop_cmd_priv_t *) other->priv;

        // The (possibly flawed) condition by which aliases are
        // opportunistically culled from the registry here
        if ((other_priv->handler == found_handler)
                && (other->is_alias(other)))
        {
            BLAMMO(WARNING, "also removing alias: %s",
                            other->keyword(other));
            priv->cmds->remove(priv->cmds);
            // remove() reverts back to previous link, so the iterator
            // might repeat the same link once before moving forward on
            // the next iteration, but at least it's better to be
            // thorough than to miss one.
        }

        other = priv->cmds->next(priv->cmds);
    }

#else
    (void) other_priv;
    (void) found_handler;
#endif

    return true;
}

//------------------------------------------------------------------------|
const scallop_cmd_t scallop_cmd_pub = {
    &scallop_cmd_create,
    &scallop_cmd_destroy,
    &scallop_cmd_copy,
    &scallop_cmd_alias,
    &scallop_cmd_find_by_keyword,
    &scallop_cmd_partial_matches,
    &scallop_cmd_exec,
    &scallop_set_attributes,
    &scallop_cmd_is_alias,
    &scallop_cmd_is_mutable,
    &scallop_cmd_is_construct,
    &scallop_cmd_keyword,
    &scallop_cmd_arghints,
    &scallop_cmd_description,
    &scallop_cmd_longest,
    &scallop_cmd_help,
    &scallop_cmd_register_cmd,
    &scallop_cmd_unregister_cmd,
    NULL
};
