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

#include "shell.h"
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
    chain_t * subcmds;

    // command handler function taking arguments are returning int
    cmd_handler_f handler;

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
shell_cmd_t;

// Destructor function for instance of shell_cmd_t
static void shell_cmd_destroy(void * cmdptr)
{
    if (!cmdptr) { return; }

    // Recursively destroy all sub-commands
    shell_cmd_t * cmd = (shell_cmd_t *) cmdptr;
    cmd->subcmds->destroy(cmd->subcmds);
}

// Finder function for searching through registered commands
static int shell_cmd_compare(const void * cmdptr1, const void * cmdptr2)
{
    shell_cmd_t * cmd1 = (shell_cmd_t *) cmdptr1;
    shell_cmd_t * cmd2 = (shell_cmd_t *) cmdptr2;

    // Commands are identified through their keyword,
    // which must be unique within the same context
    // (keywords in different contexts can be reused)
    // FIXME: Danger of buffer overrun here.
    return strcmp(cmd1->keyword, cmd2->keyword);
}

//------------------------------------------------------------------------|
// shell private implementation data
typedef struct
{
    // set true to break out of main loop
    bool quit;

    // FIXME: Eventually make this more dynamic
    // maybe use bytes_t?  remember size?
    char * prompt;

    // master list of top-level context commands
    chain_t * cmds;
}
shell_priv_t;

//------------------------------------------------------------------------|
// Built-In command handler functions
static void * builtin_handler_log(void * object, void * context,
                                  int argc, char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

static void * builtin_handler_log_level(void * object, void * context,
                                        int argc, char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

static void * builtin_handler_log_file(void * object, void * context,
                                       int argc, char ** args)
{
    // Everyone needs a log.  You're gonna love it, log.
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

static void * builtin_handler_help(void * object, void * context,
                                   int argc, char ** args)
{
    // The ubiquitous 'help' command is always there.
    // FIXME: this is probably broken for non-root contexts
    shell_t * shell = (shell_t *) object;
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    shell_cmd_t * cmd = NULL;

    priv->cmds->reset(priv->cmds);
    priv->cmds->spin(priv->cmds, 1);
    while (!priv->cmds->origin(priv->cmds))
    {
        cmd = (shell_cmd_t *) priv->cmds->data(priv->cmds);
        (void) cmd;

        // FIXME: Console output versus blammo output
        BLAMMO(INFO, "%s  %s  %s", cmd->keyword,
                                   cmd->arghints,
                                   cmd->description);

        priv->cmds->spin(priv->cmds, 1);
    }

    return context;
}

static void * builtin_handler_quit(void * object, void * context,
                                   int argc, char ** args)
{
    shell_t * shell = (shell_t *) object;
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    priv->quit = true;
    return NULL;
}

//------------------------------------------------------------------------|
static shell_t * shell_create(const char * prompt)
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

    // Create the top-level list of commands
    priv->cmds = chain_pub.create(shell_cmd_destroy);

    // Register all Built-In commands
    void * context = shell->register_cmd(shell,
                                         NULL,
                                         builtin_handler_log,
                                         "log",
                                         "level|file <...>",
                                         "change blammo logger options");

    shell->register_cmd(shell,
                        context,
                        builtin_handler_log_level,
                        "level",
                        "<0..5>",
                        "change the blammo log message level (0=VERBOSE, 5=FATAL)");

    shell->register_cmd(shell,
                        context,
                        builtin_handler_log_file,
                        "file",
                        "<path>",
                        "change the blammo log file path");

    shell->register_cmd(shell,
                        NULL,
                        builtin_handler_help,
                        "help",
                        NULL,
                        "show a list of commands with hints and description");

    shell->register_cmd(shell,
                        NULL,
                        builtin_handler_quit,
                        "quit",
                        NULL,
                        "exit out of the shell command handling loop");

    return shell;
}

//------------------------------------------------------------------------|
static void shell_destroy(void * shell_ptr)
{
    shell_t * shell = (shell_t *) shell_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!shell || !shell->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy\n");
        return;
    }

    // Recursively destroy command tree
    shell_priv_t * priv = (shell_priv_t *) shell->priv;
    priv->cmds->destroy(priv->cmds);

    // zero out and destroy the private data
    memset(shell->priv, 0, sizeof(shell_priv_t));
    free(shell->priv);

    // zero out and destroy the public interface
    memset(shell, 0, sizeof(shell_t));
    free(shell);
}

//------------------------------------------------------------------------|
static void * shell_register_cmd(shell_t * shell,
                                 void * context,
                                 cmd_handler_f handler,
                                 const char * keyword,
                                 const char * arghints,
                                 const char * description)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;

    // get a handle on the intended context / scope
    chain_t * cmds = context ? (chain_t *) context : priv->cmds;

    // FIXME: OK, FINE.  shellcmd deserves it's own file/object
    // create a shell command to either serve as a temporary placeholder
    // or else it will be added to the list of commands if all goes well.
    shell_cmd_t * cmd = (shell_cmd_t *) malloc(sizeof(shell_cmd_t));
    if (!cmd)
    {
        BLAMMO(FATAL, "malloc(sizeof(shell_cmd_t)) failed");
        return NULL;
    }

    cmd->keyword = (char *) keyword;
    cmd->arghints = (char *) arghints;
    cmd->description = (char *) description;
    cmd->handler = handler;
    cmd->subcmds = NULL;

    // ensure that the requested keyword is unique within the given context.
    shell_cmd_t * found = (shell_cmd_t *) cmds->find(cmds, cmd, shell_cmd_compare);
    if (found)
    {
        BLAMMO(ERROR, "Command \'%s\' already registered", found->keyword);
        free(cmd);
        return NULL;
    }

    // Insert the new command link
    cmds->insert(cmds, cmd);
    return cmd;
}

//------------------------------------------------------------------------|
static void * shell_unregister_cmd(shell_t * shell,
                                   void * context,
                                   const char * keyword)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;

    // get a handle on the intended context / scope
    chain_t * cmds = context ? (chain_t *) context : priv->cmds;

    // A command structure on the stack is sufficient for find()
    // in this case because we're not planning on inserting it
    shell_cmd_t cmd;

    cmd.keyword = (char *) keyword;
    shell_cmd_t * found = (shell_cmd_t *) cmds->find(cmds, &cmd, shell_cmd_compare);
    if (!found)
    {
        // Nothing was found to unregister
        BLAMMO(ERROR, "Command \'%s\' not found", found->keyword);
        return NULL;
    }

    // Removes currently selected link, which is the found
    // link if find() was successful.
    cmds->remove(cmds);

    // Returns pointer to removed memory to indicate success.
    // DO NOT DEREFERENCE --- Do Better
    return found;
}

static int shell_loop(shell_t * shell)
{
    shell_priv_t * priv = (shell_priv_t *) shell->priv;

    while (!priv->quit)
    {
        // TODO: Use either linenoise or getline
        // interpret command and execute handler
    }

    return 0;
}


//------------------------------------------------------------------------|
const shell_t shell_pub = {
    &shell_create,
    &shell_destroy,
    &shell_register_cmd,
    &shell_unregister_cmd,
    &shell_loop,
    NULL
};

