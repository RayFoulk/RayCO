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

#include "shell.h"
#include "shellcmd.h"
#include "chain.h"
#include "utils.h"
#include "blammo.h"

#ifdef LINENOISE_ENABLE
#include "linenoise.h"

// Kind of distasteful, but short of drastically modifying linenoise
// itself, this appears to be the cost of getting nested tab completion
// and argument hints via that submodule.  This forces a singleton-ish
// pattern on the shell object.  It's not likely to have multiple
// simultaneous shell objects per process so it's probably not a big deal.
static void * singleton_shell_ptr = NULL;

#endif

//------------------------------------------------------------------------|
// shell private implementation data
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
    shellcmd_t * cmds;
}
shell_priv_t;

//------------------------------------------------------------------------|
// Built-In command handler functions
static int builtin_handler_log(void * shellcmd,
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
    shellcmd_t * log = (shellcmd_t *) shellcmd;
    shellcmd_t * cmd = log->find_by_keyword(log, args[1]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Sub-command %s not found", args[1]);
        return -2;
    }

    return cmd->exec(cmd, --argc, &args[1]);
}

static int builtin_handler_log_level(void * shellcmd,
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

static int builtin_handler_log_file(void * shellcmd,
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

static int builtin_handler_source(void * shellcmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    BLAMMO(VERBOSE, "");

    shell_t * shell = (shell_t *) context;

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

    // Similar to shell->loop() but getting input from FILE
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

        result = shell->dispatch(shell, line);
        BLAMMO(INFO, "Result of dispatch(%s) is %d", line, result);

        // TODO possibly store result in priv, or else report on console
        (void) result;

        free(line);
        line = NULL;
    }

    fclose(source);
    return 0;
}

static int builtin_handler_help(void * shellcmd,
                                void * context,
                                int argc,
                                char ** args)
{
    shellcmd_t * cmd = (shellcmd_t *) shellcmd;

    char * helptext = NULL;
    size_t size = 0;
    int result = cmd->help(cmd, &helptext, &size);

    BLAMMO(INFO, "size: %u text: %s", size, helptext);

    if (helptext)
    {
        free(helptext);
        helptext = NULL;
        size = 0;
    }

    return result;
}

static int builtin_handler_quit(void * shellcmd,
                                void * context,
                                int argc,
                                char ** args)
{
    shell_t * shell = (shell_t *) context;
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    priv->quit = true;
    return 0;
}

//------------------------------------------------------------------------|
// Small private helper function to strip comments
static int shell_ignore_comments(shell_t * shell, int argc, char ** args)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
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
#ifdef LINENOISE_ENABLE

// Linenoise-only callback function for tab completion
static void linenoise_completion(const char * buf, linenoiseCompletions * lc)
{
    BLAMMO(DEBUG, "buf: \'%s\'", buf);

    // Always get a handle on the singleton shell
    shell_t * shell = (shell_t *) singleton_shell_ptr;
    shell_priv_t * priv = (shell_priv_t *) shell->priv;


}

// Linenoise-only callback for argument hints
static char * linenoise_hints(const char * buf, int * color, int * bold)
{
    BLAMMO(DEBUG, "buf: \'%s\'", buf);

    // Always get a handle on the singleton shell
    shell_t * shell = (shell_t *) singleton_shell_ptr;
    shell_priv_t * priv = (shell_priv_t *) shell->priv;

    // Do full split on mock command line in order to preemptively
    // distinguish which specific command is about to be invoked so that
    // proper hints can be given.  Otherwise this cannot
    // distinguish between 'create' and 'created' for example.
    char * line = strdup(buf);
    char * args[SHELL_MAX_ARGS];
    size_t argc = 0;

    // Clear and split the line into args[]
    memset(args, 0, sizeof(args));
    argc = splitstr(args, SHELL_MAX_ARGS, line, priv->delim);
    argc = shell_ignore_comments(shell, argc, args);
    BLAMMO(DEBUG, "argc: %u\r", argc);

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
    shellcmd_t * parent = priv->cmds;
    shellcmd_t * cmd = NULL;
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
    char * hints[SHELL_MAX_ARGS] = { NULL };
    int hintc = 0;
    int hindex = 0;

    memset(hints, 0, sizeof(hints));
    hintc = markstr(hints, SHELL_MAX_ARGS, parent->arghints(parent), priv->delim);
    BLAMMO(DEBUG, "arghints: %s  hintc: %d  argc: %d  nest: %d\r",
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

// Redirect get_command_line to linenoise
#define get_command_line(p)     linenoise(p)

#else

// wrap getline, and use for interactive if there is no linenoise
// submodule.  This is always used for scripts, however.
static char * get_command_line(const char * prompt)
{
    fprintf(stdout, "%s", prompt);

    char * line = NULL;
    size_t nalloc = 0;
    ssize_t nchars = getline(&line, &nalloc, stdin);
    if (nchars < 0)
    {
        BLAMMO(ERROR, "getline() failed with errno %d strerror %s",
                      errno, strerror(errno));
        free(line);
        return NULL;
    }

    return line;
}
#endif

//------------------------------------------------------------------------|
static shell_t * shell_create(const char * prompt,
                              const char * delim,
                              const char * comment)
{
    // Allocate and initialize public interface
    shell_t * shell = (shell_t *) malloc(sizeof(shell_t));
    if (!shell)
    {
        BLAMMO(FATAL, "malloc(sizeof(shell_t)) failed");
        return NULL;
    }

    // Bulk copy all function pointers and init opaque ptr
    memcpy(shell, &shell_pub, sizeof(shell_t));

    // Allocate and initialize private implementation
    shell->priv = malloc(sizeof(shell_priv_t));
    if (!shell->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(shell_priv_t)) failed");
        free(shell);
        return NULL;
    }

    memset(shell->priv, 0, sizeof(shell_priv_t));
    shell_priv_t * priv = (shell_priv_t *) shell->priv;

    // Initialize prompt string
    priv->prompt = (char *) prompt;
    priv->delim = (char *) delim;
    priv->comment = (char *) comment;

    // Create the top-level list of commands
    priv->cmds = shellcmd_pub.create(NULL, NULL, NULL, NULL, NULL);
    if (!priv->cmds)
    {
        BLAMMO(FATAL, "shellcmd_pub.create() failed");
        shell->destroy(shell);
        return NULL;
    }

    // Register all Built-In commands
    shellcmd_t * log = priv->cmds->create(
            builtin_handler_log,
            NULL,
            "log",
            " [level|file] <...>",
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
                builtin_handler_log_file,
                NULL,
                "file",
                " <path>",
                "change the blammo log file path"));

    priv->cmds->register_cmd(priv->cmds,
            priv->cmds->create(
                    builtin_handler_source,
                    shell,
                    "source",
                    " <path>",
                    "load and run a command script"));

    priv->cmds->register_cmd(priv->cmds,
            priv->cmds->create(
                    builtin_handler_help,
                    NULL,
                    "help",
                    NULL,
                    "show a list of commands with hints and description"));

    priv->cmds->register_cmd(priv->cmds,
            priv->cmds->create(
                    builtin_handler_quit,
                    shell,
                    "quit",
                    NULL,
                    "exit the shell command handling loop"));


#ifdef LINENOISE_ENABLE
    // Set the completion callback, for when <tab> is pressed
    linenoiseSetCompletionCallback(linenoise_completion);

    // Set the hints callback for when arguments are needed
    linenoiseSetHintsCallback(linenoise_hints);

    // Load command history from file.
    //linenoiseHistoryLoad("history.txt"); /* Load the history at startup */

    // Set global pointer to shell object.  WARNING: This will break if
    // multiple shells are created, resulting in hints/completion for the
    // wrong object!
    singleton_shell_ptr = (void *) shell;
#endif

    return shell;
}

//------------------------------------------------------------------------|
static void shell_destroy(void * shell_ptr)
{
    shell_t * shell = (shell_t *) shell_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!shell || !shell->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    shell_priv_t * priv = (shell_priv_t *) shell->priv;

    // Recursively destroy command tree
    if (priv->cmds)
    {
        priv->cmds->destroy(priv->cmds);
    }

    // zero out and destroy the private data
    memset(shell->priv, 0, sizeof(shell_priv_t));
    free(shell->priv);

    // zero out and destroy the public interface
    memset(shell, 0, sizeof(shell_t));
    free(shell);

#ifdef LINENOISE_ENABLE
    singleton_shell_ptr = NULL;
#endif

}

//------------------------------------------------------------------------|
static inline shellcmd_t * shell_commands(shell_t * shell)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    return priv->cmds;
}

//------------------------------------------------------------------------|
static int shell_dispatch(shell_t * shell, char * line)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    BLAMMO(VERBOSE, "priv: %p depth: %u line: %s",
                    priv, priv->depth, line);

    // Keep track of recursion depth here
    priv->depth++;
    if (priv->depth > SHELL_MAX_RECURS)
    {
        BLAMMO(ERROR, "Maximum recursion depth %u reached",
                      SHELL_MAX_RECURS);
        return -1;
    }

    // For getting a handle on the command to be executed
    shellcmd_t * cmd = NULL;
    int result = 0;

    // For splitting command line into args[]
    char * args[SHELL_MAX_ARGS];
    size_t argc = 0;

    // Clear and split the line into args[]
    memset(args, 0, sizeof(args));
    argc = splitstr(args, SHELL_MAX_ARGS, line, priv->delim);
    argc = shell_ignore_comments(shell, argc, args);

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
static int shell_loop(shell_t * shell)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    char * line = NULL;
    int result = 0;

    while (!priv->quit)
    {
        // Get a line of raw user input
        line = get_command_line(priv->prompt);
        BLAMMO(DEBUG, "line: %s", line);

        result = shell->dispatch(shell, line);
        BLAMMO(INFO, "Result of dispatch(%s) is %d", line, result);

        // TODO possibly store result in priv, or else report on console
        (void) result;

        free(line);
        line = NULL;
    }

    return 0;
}

//------------------------------------------------------------------------|
const shell_t shell_pub = {
    &shell_create,
    &shell_destroy,
    &shell_commands,
    &shell_dispatch,
    &shell_loop,
    NULL
};

