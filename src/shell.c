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

// This is really kind of terrible, forcing a singleton-ish pattern
// on the shell object.  Conceivably, you could have multiple simultaneous
// shell instances operating over different pipes, but in practice you can
// only have one (per tty, anyway) that operates on stdin/stdout.
// linenoise itself would have to be modified to support contexts passed
// into the callbacks in order to avoid this.
static shellcmd_t * shellcmdptr = NULL;
#endif

//------------------------------------------------------------------------|
// shell private implementation data
typedef struct
{
    // set true to break out of main loop
    bool quit;

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
static void * builtin_handler_log(void * shellcmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Not enough arguments for log command");
        return NULL;
    }

    // Find and execute subcommand
    shellcmd_t * log = (shellcmd_t *) shellcmd;
    shellcmd_t * cmd = log->find_by_keyword(log, args[1]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Sub-command %s not found", args[1]);
        return NULL;
    }

    return cmd->exec(cmd, --argc, &args[1]);
}

static void * builtin_handler_log_level(void * shellcmd,
                                        void * context,
                                        int argc,
                                        char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a numeric argument for level");
        return NULL;
    }

    size_t level = strtoul(args[1], NULL, 0);
    BLAMMO(INFO, "Setting log level to %u", level);
    BLAMMO_LEVEL(level);

    return NULL;
}

static void * builtin_handler_log_file(void * shellcmd,
                                       void * context,
                                       int argc,
                                       char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a file path argument");
        return NULL;
    }

    BLAMMO(INFO, "Setting log file path to %s", args[1]);
    BLAMMO_FILE(args[1]);

    return NULL;
}

static void * builtin_handler_help(void * shellcmd,
                                   void * context,
                                   int argc,
                                   char ** args)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");

    // The ubiquitous 'help' command is always there.
    // FIXME: this is probably broken for non-root contexts

//    shell_t * shellptr = (shell_t *) shell;
//    shell_priv_t * priv = (shell_priv_t *) shellptr->priv;
//    shellcmd_t * cmd = NULL;

    /*  NEED RECURSIVE GET_HELP FROM ALL SUBCMDS
    priv->cmds->reset(priv->cmds);
    priv->cmds->spin(priv->cmds, 1);
    while (!priv->cmds->origin(priv->cmds))
    {
        cmd = (shellcmd_t *) priv->cmds->data(priv->cmds);
        (void) cmd;

        // FIXME: Console output versus blammo output
        BLAMMO(INFO, "%s  %s  %s", cmd->keyword,
                                   cmd->arghints,
                                   cmd->description);

        priv->cmds->spin(priv->cmds, 1);
    }
    */

    return NULL;
}

static void * builtin_handler_script(void * shellcmd,
                                     void * context,
                                     int argc,
                                     char ** args)
{
    // FIXME: Implement me
    BLAMMO(ERROR, "NOT IMPLEMENTED");

}

static void * builtin_handler_quit(void * shellcmd,
                                   void * context,
                                   int argc,
                                   char ** args)
{
    shell_t * shell = (shell_t *) context;
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    priv->quit = true;
    return NULL;
}

//------------------------------------------------------------------------|
#ifdef LINENOISE_ENABLE

// Linenoise-only callback function for tab completion
static void linenoise_completion(const char * buf, linenoiseCompletions * lc)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
}

// Linenoise-only callback for argument hints
static char * linenoise_hints(const char * buf, int * color, int * bold)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

// Redirect get_command_line to linenoise
#define get_command_line(p)     linenoise(p)

#else

// Function signature matches linenoise function,
// but wraps around getline() library function
static char * get_command_line(const char * prompt)
{
    fprintf(stdout, "%s", prompt);

    char * line = NULL;
    size_t nalloc = 0;
    ssize_t nchars = getline(&line, &nalloc, stdin);
    if (nchars < 0)
    {
        BLAMMO(ERROR, "getline() failed with errno %d", errno);
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
            "level|file <...>",
            "change blammo logger options");

    priv->cmds->register_cmd(priv->cmds, log);

    log->register_cmd(log,
            log->create(
                builtin_handler_log_level,
                NULL,
                "level",
                "<0..5>",
                "change the blammo log message level (0=VERBOSE, 5=FATAL)"));

    log->register_cmd(log,
            log->create(
                builtin_handler_log_file,
                NULL,
                "file",
                "<path>",
                "change the blammo log file path"));

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

    // Set global pointer to shell commands.  This will break if
    // ever multiple shells are created.
    shellcmdptr = priv->cmds;
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
}

//------------------------------------------------------------------------|
//static inline shellcmd_t * shell_commands(shell_t * shell)
//{
//    shell_priv_t * priv = (shell_priv_t *) shell->priv;
//    return priv->cmds;
//}

//------------------------------------------------------------------------|
static int shell_loop(shell_t * shell)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    char * line = NULL;

    // For splitting command line into args[]
    char * args[SHELL_MAX_ARGS];
    size_t argc = 0;
    size_t words = 0;

    // For getting a handle on the command to be executed
    shellcmd_t * cmd = NULL;
    void * result = NULL;

    while (!priv->quit)
    {
        // Get a line of raw user input
        line = get_command_line(priv->prompt);
        BLAMMO(DEBUG, "line: %s", line);

        // Clear and split the line into args[]
        memset(args, 0, sizeof(args));
        argc = splitstr(args, SHELL_MAX_ARGS, line, priv->delim);

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
        argc = words;

        // Ignore empty input
        if (argc == 0)
        {
            BLAMMO(VERBOSE, "Ignoring empty line");
            continue;
        }

        // Try to find the command being invoked
        cmd = priv->cmds->find_by_keyword(priv->cmds, args[0]);
        if (!cmd)
        {
            BLAMMO(WARNING, "Command %s not found", args[0]);
        }
        else
        {
            result = cmd->exec(cmd, argc, args);
            BLAMMO(INFO, "Result of exec(%s) is %p", args[0], result);
        }

        free(line);
    }

    return 0;
}


//------------------------------------------------------------------------|
const shell_t shell_pub = {
    &shell_create,
    &shell_destroy,
    //&shell_commands,
    &shell_loop,
    NULL
};

