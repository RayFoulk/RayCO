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
#include "scallcmd.h"
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

    // FIXME: Eventually make this more dynamic
    // maybe use bytes_t?  remember size?
    char * prompt;

    // command line delimiters are pretty universal.  usually whitespace,
    // but leaving this as an option in case there is a need.
    char * delim;

    // The string that designates everything to the right as a comment.
    char * comment;

    // also assumed to be ubiqui
    // master list of top-level context commands
    scallop_cmd_t * cmds;

    // Pointer to the console object for user I/O
    console_t * console;
}
scallop_priv_t;

//------------------------------------------------------------------------|
// Built-In command handler functions
static int builtin_handler_log(void * scallcmd,
                               void * context,
                               int argc,
                               char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Not enough arguments for log command");
        return -1;
    }

    // Find and execute subcommand
    scallop_cmd_t * log = (scallop_cmd_t *) scallcmd;
    scallop_cmd_t * cmd = log->find_by_keyword(log, args[1]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Sub-command %s not found", args[1]);
        return -2;
    }

    return cmd->exec(cmd, --argc, &args[1]);
}

static int builtin_handler_log_level(void * scallcmd,
                                     void * context,
                                     int argc,
                                     char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a numeric argument for level");
        return -1;
    }

    BLAMMO_DECLARE(size_t level = strtoul(args[1], NULL, 0));
    BLAMMO(INFO, "Setting log level to %u", level);
    BLAMMO_LEVEL(level);

    return 0;
}

static int builtin_handler_log_stdout(void * scallcmd,
                                      void * context,
                                      int argc,
                                      char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a boolean flag");
        return -1;
    }

    BLAMMO(INFO, "Setting log stdout to %s",
                 str_to_bool(args[1]) ? "true" : "false");
    BLAMMO_STDOUT(str_to_bool(args[1]));

    return 0;
}

static int builtin_handler_log_file(void * scallcmd,
                                    void * context,
                                    int argc,
                                    char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a file path argument");
        return -1;
    }

    BLAMMO(INFO, "Setting log file path to %s", args[1]);
    BLAMMO_FILE(args[1]);

    return 0;
}

static int builtin_handler_source(void * scallcmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    BLAMMO(VERBOSE, "");

    scallop_t * scallop = (scallop_t *) context;

    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a file path argument");
        return -1;
    }

    FILE * source = fopen(args[1], "r");
    if (!source)
    {
        BLAMMO(ERROR, "Could not open %s for reading", args[1]);
        return -2;
    }

    // Similar to scallop->loop() but getting input from FILE
    // It is a design choice to not reuse/repurpose the
    // get_command_line() macro here: That implies echoing
    // both the prompt and the commands read to stdout, and
    // that is not something I want happening.
    char * line = NULL;
    size_t nalloc = 0;
    ssize_t nchars = 0;
    int result = 0;

    while (!feof(source))
    {
        nchars = getline(&line, &nalloc, source);
        if (nchars < 0)
        {
            // Can happen normally when at last line just before EOF
            BLAMMO(WARNING, "getline(%s) failed with errno %d strerror %s",
                            args[1], errno, strerror(errno));
            fclose(source);
            free(line);
            return 0;
        }

        result = scallop->dispatch(scallop, line);
        BLAMMO(INFO, "Result of dispatch(%s) is %d", line, result);

        // TODO possibly store result in priv, or else report on console
        (void) result;

        free(line);
        line = NULL;
    }

    fclose(source);
    return 0;
}

static int builtin_handler_help(void * scallcmd,
                                void * context,
                                int argc,
                                char ** args)
{
    scallop_priv_t * priv = (scallop_priv_t *) context;
    bytes_t * help = bytes_pub.create(NULL, 0);

    help->print(help, "commands:\r\n");

    int result = priv->cmds->help(priv->cmds, help, 0);
    if (result < 0)
    {
        BLAMMO(ERROR, "cmds->help() returned %d", result);
        help->destroy(help);
        return result;
    }

    //BLAMMO(DEBUG, "help hexdump:\n%s\n", help->hexdump(help));
    // FIXME: run help text through a column formatter
    //  potentially build this into bytes_t

    priv->console->print(priv->console, "%s", help->cstr(help));
    help->destroy(help);
    return result;
}

static int builtin_handler_quit(void * scallcmd,
                                void * context,
                                int argc,
                                char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop->quit(scallop);
    return 0;
}

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
        pmatches->spin(pmatches, 1);
    }
    while (!pmatches->origin(pmatches));

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
                                  const char * prompt,
                                  const char * delim,
                                  const char * comment)
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

    // Initialize prompt string
    priv->prompt = (char *) prompt;
    priv->delim = (char *) delim;
    priv->comment = (char *) comment;

    // Create the top-level list of commands
    priv->cmds = scallop_cmd_pub.create(NULL, NULL, NULL, NULL, NULL);
    if (!priv->cmds)
    {
        BLAMMO(FATAL, "scallop_cmd_pub.create() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // Register all Built-In commands
    scallop_cmd_t * log = priv->cmds->create(
            builtin_handler_log,
            NULL,
            "log",
            " <logcmd> <...>",
            "change blammo logger options");

    priv->cmds->register_cmd(priv->cmds, log);

    log->register_cmd(log,
            log->create(
                builtin_handler_log_level,
                NULL,
                "level",
                " <0..5>",
                "change the blammo log message level (0=VERBOSE, 5=FATAL)"));

    log->register_cmd(log,
            log->create(
                builtin_handler_log_stdout,
                NULL,
                "stdout",
                " <true/false>",
                "enable or disable logging to stdout"));

    log->register_cmd(log,
            log->create(
                builtin_handler_log_file,
                NULL,
                "file",
                " <path>",
                "change the blammo log file path"));

    priv->cmds->register_cmd(priv->cmds,
            priv->cmds->create(
                    builtin_handler_source,
                    scallop,
                    "source",
                    " <path>",
                    "load and run a command script"));

    priv->cmds->register_cmd(priv->cmds,
            priv->cmds->create(
                    builtin_handler_help,
                    priv,
                    "help",
                    NULL,
                    "show a list of commands with hints and description"));

    priv->cmds->register_cmd(priv->cmds,
            priv->cmds->create(
                    builtin_handler_quit,
                    scallop,
                    "quit",
                    NULL,
                    "exit the scallop command handling loop"));

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
                                       priv->prompt,
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

static void scallop_quit(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    priv->quit = true;
    // TODO: Need to pump a newline into the console here
    // just to get it off the blocking call?
}

//------------------------------------------------------------------------|
const scallop_t scallop_pub = {
    &scallop_create,
    &scallop_destroy,
    &scallop_commands,
    &scallop_dispatch,
    &scallop_loop,
    &scallop_quit,
    NULL
};

