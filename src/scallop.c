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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include "scallop.h"
#include "scommand.h"
#include "sbuiltin.h"
#include "console.h"
#include "chain.h"
#include "bytes.h"
#include "utils.h"
#include "blammo.h"

//------------------------------------------------------------------------|
// scallop private implementation data
typedef struct
{
    // set true to break out of main loop
    bool quit;

    // Recursion depth for when executing scripts/procedures
    size_t depth;

    // Current prompt text buffer
    bytes_t * prompt;

    // command line delimiters are pretty universal.  usually whitespace,
    // but leaving this as an option in case there is a need.
    char * delim;

    // The string that designates everything to the right as a comment.
    char * comment;

    // master list of top-level context commands
    scallop_cmd_t * cmds;

    // The list of all defined routines
    chain_t * rtns;

    // Pointer to the console object for user I/O
    console_t * console;
}
scallop_priv_t;

//------------------------------------------------------------------------|
// Small private helper function to strip comments
static int scallop_ignore_comments(scallop_t * scallop, int argc, char ** args)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    size_t words = 0;

    // Specifically ignore comments by searching through
    // the list and simply reducing the effective arg count
    // where comment string is identified
    for (words = 0; words < argc; words++)
    {
        if (!strncmp(args[words], priv->comment, strlen(priv->comment)))
        {
            BLAMMO(VERBOSE, "Found comment %s at arg %u",
                            priv->comment, words);
            break;
        }
    }

    return words;
}

//------------------------------------------------------------------------|
static void scallop_tab_completion(void * object, const char * buffer)
{
    BLAMMO(DEBUG, "buffer: \'%s\'", buffer);

    // Always get a handle on the singleton scallop
    scallop_t * scallop = (scallop_t *) object;
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Do full split on mock command line so that we can match
    // fully qualified keywords up to and including incomplete
    // commands that need tab-completion added.  The original
    // buffer cannot be altered or it will break line editing.
    char * line = strdup(buffer);
    char * args[SCALLOP_MAX_ARGS];
    size_t argc = 0;

    // Clear and split the line into args[]
    memset(args, 0, sizeof(args));
    argc = splitstr(args, SCALLOP_MAX_ARGS, line, priv->delim);
    argc = scallop_ignore_comments(scallop, argc, args);
    BLAMMO(DEBUG, "argc: %u", argc);

    // Ignore empty input
    if (argc == 0)
    {
        free(line);
        return;
    }

    // Strategy: the final unmatched command is the one that
    // may require tab completion.  Match known command keywords
    // as far as possible before making suggestions on incomplete
    // arguments/keywords
    scallop_cmd_t * parent = priv->cmds;
    scallop_cmd_t * cmd = NULL;
    int nest = 0;

    for (nest = 0; nest < argc; nest++)
    {
        // Try to find each nested command by keyword
        cmd = priv->cmds->find_by_keyword(parent, args[nest]);
        if (!cmd)
        {
            BLAMMO(DEBUG, "Command %s not found", args[nest]);
            break;
        }

        BLAMMO(DEBUG, "Command %s found!", args[nest]);
        parent = cmd;
    }

    BLAMMO(DEBUG, "parent keyword: %s  args[%d]: %s",
            parent->keyword(parent), nest, args[nest]);

    // Search through the parent command's sub-commands, but by starting
    // letter(s)/sub-string match rather than by first full keyword
    // match.  This returns a list of possible keywords that match the
    // given substring.  I.E. given: "lo" return [logoff, log, local]
    size_t longest = 0;
    chain_t * pmatches = parent->partial_matches(parent, args[nest], &longest);
    free(line);
    if (!pmatches || pmatches->length(pmatches) == 0) { return; }
    BLAMMO(DEBUG, "partial_matches length: %u  longest: %u",
                  pmatches->length(pmatches), longest);

    // Need to duplicate the line again, but this time leave the copy
    // mostly intact except that we want to modify the potentially
    // tab-completed argument in place and then feed the whole thing
    // back to console.  Just re-use the existing declared variables.
    // Basically strdup() manually, but allow some extra room for tab completed
    // keyword.  Maybe can use strndup() with a big n instead???
    char * keyword = NULL;
    size_t length = strlen(buffer);
    size_t lsize = length + longest;
    line = (char *) malloc(lsize);
    memcpy(line, buffer, length);
    line[length] = 0;

    // get markers within unmodified copy of line
    memset(args, 0, sizeof(args));
    markstr(args, SCALLOP_MAX_ARGS, line, priv->delim);

    // iterate through partially matched keywords
    pmatches->reset(pmatches);
    do
    {
        keyword = (char *) pmatches->data(pmatches);

        // This is overkill, but easier to understand and implement than
        // trying to track where the terminating zero is while iterating
        // through keywords of varying length.  I'm sure there are tons
        // of bugs in this code.  TODO: harden it up later with some
        // static and dynamic analysis and complete unit tests!!!
        memset(args[nest], 0, strlen(keyword) + 2);
        memcpy(args[nest], keyword, strlen(keyword));

        // Add primary delimiter character at end of completion
        args[nest][strlen(keyword)] = priv->delim[0];

        BLAMMO(DEBUG, "Adding tab completion line: \'%s\'", line);
        priv->console->add_tab_completion(priv->console, line);
    }
    while (pmatches->spin(pmatches, 1));

    free(line);
    pmatches->destroy(pmatches);
}

// callback for argument hints
static char * scallop_arg_hints(void * object,
                                const char * buffer,
                                int * color,
                                int * bold)
{
    BLAMMO(DEBUG, "buffer: \'%s\'", buffer);

    // Always get a handle on the singleton scallop
    scallop_t * scallop = (scallop_t *) object;
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Do full split on mock command line in order to preemptively
    // distinguish which specific command is about to be invoked so that
    // proper hints can be given.  Otherwise this cannot
    // distinguish between 'create' and 'created' for example.
    char * line = strdup(buffer);
    char * args[SCALLOP_MAX_ARGS];
    size_t argc = 0;

    // Clear and split the line into args[]
    memset(args, 0, sizeof(args));
    argc = splitstr(args, SCALLOP_MAX_ARGS, line, priv->delim);
    argc = scallop_ignore_comments(scallop, argc, args);
    BLAMMO(DEBUG, "argc: %u", argc);

    // Ignore empty input
    if (argc == 0)
    {
        free(line);
        return NULL;
    }

    // Hint strategy: match as many keywords as possible, traversing a single
    // path down the sub-command tree.  When we no longer have a matching
    // keyword, then display the hints for the last known matching command.
    // This should fail and return NULL right out of the gate if the first
    // keyord does not match.
    scallop_cmd_t * parent = priv->cmds;
    scallop_cmd_t * cmd = NULL;
    int nest = 0;

    for (nest = 0; nest < argc; nest++)
    {
        // Try to find each nested command by keyword
        cmd = priv->cmds->find_by_keyword(parent, args[nest]);
        if (!cmd)
        {
            BLAMMO(DEBUG, "Command %s not found", args[nest]);
            break;
        }

        BLAMMO(DEBUG, "Command %s found!", args[nest]);
        parent = cmd;
    }

    // No longer referencing args, OK to free underlying line
    free(line);

    // Return early before processing hints if there are none
    if (!parent->arghints(parent))
    {
        BLAMMO(DEBUG, "No arg hints to provide");
        return NULL;
    }

    // https://stackoverflow.com/questions/13078926/is-there-a-way-to-count-tokens-in-c
    // Count tokens without modifying the original string, rather than
    // allocating another buffer.   The purpose is to determine which args
    // have been fulfilled and to only show hints for unfulfilled args.
    char * hints[SCALLOP_MAX_ARGS] = { NULL };
    int hintc = 0;
    int hindex = 0;

    memset(hints, 0, sizeof(hints));
    hintc = markstr(hints, SCALLOP_MAX_ARGS, parent->arghints(parent), priv->delim);
    BLAMMO(DEBUG, "arghints: %s  hintc: %d  argc: %d  nest: %d",
            parent->arghints(parent), hintc, argc, nest);

    // Only show hints for expected arguments that have not already been
    // fulfilled.  Number of hints is relative to the nested sub-command
    // and not absolute to the base command.  Also, the User may provide
    // extra arguments which will likely just be ignored by whatever
    // handler function consumes them.  The Number of hints to show is a
    // function of number of hints _available_ (hintc), number of args
    // provided (argc), and the nest level.
    hindex = argc - nest;

    BLAMMO(DEBUG, "Uncoerced index is %d", hindex);
    if (hindex < 0 || hindex >= hintc)
    {
        BLAMMO(DEBUG, "Invalid hint index");
        return NULL;
    }

    // TODO: Make these constants or defines
    *color = 35;
    *bold = 0;

    // Backup one character assuming there is a leading space
    return hints[hindex] - 1;
}

//------------------------------------------------------------------------|
static scallop_t * scallop_create(console_t * console,
                                  const char * prompt_base)
{
    // Allocate and initialize public interface
    scallop_t * scallop = (scallop_t *) malloc(sizeof(scallop_t));
    if (!scallop)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_t)) failed");
        return NULL;
    }

    // Bulk copy all function pointers and init opaque ptr
    memcpy(scallop, &scallop_pub, sizeof(scallop_t));

    // Allocate and initialize private implementation
    scallop->priv = malloc(sizeof(scallop_priv_t));
    if (!scallop->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_priv_t)) failed");
        free(scallop);
        return NULL;
    }

    memset(scallop->priv, 0, sizeof(scallop_priv_t));
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Expect that a console must be given
    if (!console)
    {
        BLAMMO(FATAL, "No console provided!");
        scallop->destroy(scallop);
        return NULL;
    }

    // cross-link the console into scallop, in terms
    // of callbacks for tab completion and object reference.
    priv->console = console;
    console->set_line_callbacks(console,
                                scallop_tab_completion,
                                scallop_arg_hints,
                                scallop);

    // Initialize prompt
    priv->prompt = bytes_pub.create(NULL, 0);
    scallop->prompt_push(scallop, prompt_base);

    // Set up some dialect 'constants'
    priv->delim = SCALLOP_CMD_DELIM;
    priv->comment = SCALLOP_CMD_COMMENT;

     // Create the top-level list of commands
    priv->cmds = scallop_cmd_pub.create(NULL, NULL, NULL, NULL, NULL);
    if (!priv->cmds)
    {
        BLAMMO(FATAL, "scallop_cmd_pub.create() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // Register all Built-In commands
    if (!register_builtin_commands(scallop))
    {
        BLAMMO(FATAL, "register_builtin_commands() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // XXXXXXXXXXXXXXXXXXXXXXXX
    //priv->rtns = create!!!

    return scallop;
}

//------------------------------------------------------------------------|
static void scallop_destroy(void * scallop_ptr)
{
    scallop_t * scallop = (scallop_t *) scallop_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!scallop || !scallop->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Recursively destroy command tree
    if (priv->cmds)
    {
        priv->cmds->destroy(priv->cmds);
    }

    // zero out and destroy the private data
    memset(scallop->priv, 0, sizeof(scallop_priv_t));
    free(scallop->priv);

    // zero out and destroy the public interface
    memset(scallop, 0, sizeof(scallop_t));
    free(scallop);
}

//------------------------------------------------------------------------|
static inline console_t * scallop_console(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    return priv->console;
}

//------------------------------------------------------------------------|
static inline scallop_cmd_t * scallop_commands(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    return priv->cmds;
}

//------------------------------------------------------------------------|
static int scallop_dispatch(scallop_t * scallop, char * line)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    BLAMMO(VERBOSE, "priv: %p depth: %u line: %s",
                    priv, priv->depth, line);

    // Keep track of recursion depth here
    priv->depth++;
    if (priv->depth > SCALLOP_MAX_RECURS)
    {
        BLAMMO(ERROR, "Maximum recursion depth %u reached",
                      SCALLOP_MAX_RECURS);
        return -1;
    }

    // For getting a handle on the command to be executed
    scallop_cmd_t * cmd = NULL;
    int result = 0;

    // For splitting command line into args[]
    char * args[SCALLOP_MAX_ARGS];
    size_t argc = 0;

    // Clear and split the line into args[]
    memset(args, 0, sizeof(args));
    argc = splitstr(args, SCALLOP_MAX_ARGS, line, priv->delim);
    argc = scallop_ignore_comments(scallop, argc, args);

    // Ignore empty input
    if (argc == 0)
    {
        BLAMMO(VERBOSE, "Ignoring empty line");
        priv->depth--;
        return 0;
    }

    // Try to find the command being invoked
    cmd = priv->cmds->find_by_keyword(priv->cmds, args[0]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Command %s not found", args[0]);
        priv->depth--;
        return -1;
    }

    result = (int) (ssize_t) cmd->exec(cmd, argc, args);
    priv->depth--;
    return result;
}

//------------------------------------------------------------------------|
static int scallop_loop(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    char * line = NULL;
    int result = 0;

    while (!priv->quit)
    {
        // Get a line of raw user input
        line = priv->console->get_line(priv->console,
                                       priv->prompt->cstr(priv->prompt),
                                       true);
        BLAMMO(DEBUG, "line: %s", line);

        result = scallop->dispatch(scallop, line);
        BLAMMO(INFO, "Result of dispatch(%s) is %d", line, result);

        // TODO possibly store result in priv, or else report on console
        (void) result;

        free(line);
        line = NULL;
    }

    return 0;
}

//------------------------------------------------------------------------|
static void scallop_quit(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    priv->quit = true;
    // TODO: Need to pump a newline into the console here
    // just to get it off the blocking call?
}

//------------------------------------------------------------------------|
void scallop_prompt_push(scallop_t * scallop, const char * name)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    bytes_t * prompt = priv->prompt;
    ssize_t offset = prompt->find_right(prompt,
                                        SCALLOP_PROMPT_FINAL,
                                        strlen(SCALLOP_PROMPT_FINAL));

    // first truncate at the finale if it exists.
    if (offset > 0)
    {
        prompt->resize(prompt, offset);

        // For initialization reasons, only append
        // the context delimiter if this is NOT the
        // first time, otherwise the base prompt
        // would have one also.
        prompt->append(prompt,
                       SCALLOP_PROMPT_DELIM,
                       strlen(SCALLOP_PROMPT_DELIM));
    }

    // Append the new context name on the end, and then the finale
    prompt->append(prompt,
                   name,
                   strlen(name));

    prompt->append(prompt,
                   SCALLOP_PROMPT_FINAL,
                   strlen(SCALLOP_PROMPT_FINAL));
}

//------------------------------------------------------------------------|
void scallop_prompt_pop(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    bytes_t * prompt = priv->prompt;
    ssize_t offset = prompt->find_right(prompt,
                                        SCALLOP_PROMPT_DELIM,
                                        strlen(SCALLOP_PROMPT_DELIM));

    if (offset > 0)
    {
        prompt->resize(prompt, offset);

        prompt->append(prompt,
                       SCALLOP_PROMPT_FINAL,
                       strlen(SCALLOP_PROMPT_FINAL));
    }
}

//------------------------------------------------------------------------|
const scallop_t scallop_pub = {
    &scallop_create,
    &scallop_destroy,
    &scallop_console,
    &scallop_commands,
    &scallop_dispatch,
    &scallop_loop,
    &scallop_quit,
    &scallop_prompt_push,
    &scallop_prompt_pop,
    NULL
};

