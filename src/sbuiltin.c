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
// Built-In command handler functions
static int builtin_handler_help(void * scmd,
                                void * context,
                                int argc,
                                char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop_cmd_t * cmds = scallop->commands(scallop);
    console_t * console = scallop->console(scallop);
    bytes_t * help = bytes_pub.create(NULL, 0);

    help->print(help, "commands:\r\n");

    int result = cmds->help(cmds, help, 0);
    if (result < 0)
    {
        BLAMMO(ERROR, "cmds->help() returned %d", result);
        help->destroy(help);
        return result;
    }

    //BLAMMO(DEBUG, "help hexdump:\n%s\n", help->hexdump(help));
    // TODO: run help text through a column formatter
    //  potentially build this functionality into bytes_t

    console->print(console, "%s", help->cstr(help));
    help->destroy(help);
    return result;
}

//------------------------------------------------------------------------|
static int builtin_handler_alias(void * scmd,
                                 void * context,
                                 int argc,
                                 char ** args)
{
    scallop_t * scallop = (scallop_t *) context;

    // TODO: Consider supporting aliasing nested commands
    //  via some syntax like 'alias stuff log.file'.  or else
    //  change root command context 'change log' and then
    // 'alias stuff file'.  Then again, maybe we're going over
    // the rails a bit here.

    // want to actually register a new command here, under
    // the keyword of the alias, but with the callback of the
    // original keyword.  TBD under what scope this applies.
    // NOTE: this is distinct from bash's alias which can
    // encompass pretty much anything.
    // this will be saved for the 'routine' keyword

    if (argc < 3)
    {
        BLAMMO(ERROR, "Expected new and original keyword for alias");
        return -1;
    }

    // Find the command that is referenced
    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[2]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Command %s not found", args[2]);
        return -2;
    }

    // re-register the same command under a different keyword
    scallop_cmd_t * alias = scope->alias(cmd, args[1]);
    if (!scope->register_cmd(scope, alias))
    {
        BLAMMO(ERROR, "Failed to register alias %s to %s",
                      args[1], args[2]);
        return -3;
    }

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_unalias(void * scmd,
                                   void * context,
                                   int argc,
                                   char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected keyword to unalias");
        return -1;
    }

    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[1]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Command %s not found", args[1]);
        return -2;
    }

    if (!cmd->is_alias(cmd))
    {
        // FIXME: Borrow message types from blammo
        console->print(console,
                "error: %s is not an alias",
                cmd->keyword(cmd));
        return -3;
    }

    if (!scope->unregister_cmd(scope, cmd))
    {
        console->print(console,
                "error: unregister_cmd(%s) failed",
                cmd->keyword(cmd));
        return -4;
    }

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_log(void * scmd,
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
    scallop_cmd_t * log = (scallop_cmd_t *) scmd;
    scallop_cmd_t * cmd = log->find_by_keyword(log, args[1]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Sub-command %s not found", args[1]);
        return -2;
    }

    return cmd->exec(cmd, --argc, &args[1]);
}

//------------------------------------------------------------------------|
static int builtin_handler_log_level(void * scmd,
                                     void * context,
                                     int argc,
                                     char ** args)
{
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

//------------------------------------------------------------------------|
static int builtin_handler_log_stdout(void * scmd,
                                      void * context,
                                      int argc,
                                      char ** args)
{
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

//------------------------------------------------------------------------|
static int builtin_handler_log_file(void * scmd,
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

//------------------------------------------------------------------------|
static int builtin_handler_source(void * scmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
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
        // TODO: Consider temporarily swapping console pipes
        // and using the getter from the console interface.
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

//------------------------------------------------------------------------|
static int builtin_handler_quit(void * scmd,
                                void * context,
                                int argc,
                                char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop->quit(scallop);
    return 0;
}

//------------------------------------------------------------------------|
bool register_builtin_commands(scallop_t * scallop)
{
    scallop_cmd_t * cmds = scallop->commands(scallop);
    bool success = true;

    // TODO: print, routine, eval, invert, jump-if, label?

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_help,
        scallop,
        "help",
        NULL,
        "show a list of commands with hints and description"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_alias,
        scallop,
        "alias",
        " <alias-keyword> <original-keyword>",
        "alias one command keyword to another"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_unalias,
        scallop,
        "unalias",
        " <alias-keyword>",
        "unregister a command alias"));

    scallop_cmd_t * log = cmds->create(
        builtin_handler_log,
        NULL,
        "log",
        " <logcmd> <...>",
        "change blammo logger options");

    success &= cmds->register_cmd(cmds, log);

    success &= log->register_cmd(log, log->create(
        builtin_handler_log_level,
        NULL,
        "level",
        " <0..5>",
        "change the blammo log message level (0=VERBOSE, 5=FATAL)"));

    success &= log->register_cmd(log, log->create(
        builtin_handler_log_stdout,
        NULL,
        "stdout",
        " <true/false>",
        "enable or disable logging to stdout"));

    success &= log->register_cmd(log, log->create(
        builtin_handler_log_file,
        NULL,
        "file",
        " <path>",
        "change the blammo log file path"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_source,
        scallop,
        "source",
        " <path>",
        "load and run a command script"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_quit,
        scallop,
        "quit",
        NULL,
        "exit the scallop command handling loop"));

#if 0
    // BEGIN: NESTING TEST
    // Visually check whether the help indentation
    // and text formatting is working properly.
    // TODO: Remove this code once it is.
    //  and add a unit test to verify it remains so.
    scallop_cmd_t * horse = priv->cmds->create(
        NULL,
        NULL,
        "horse",
        " <nosir>",
        "i don't like it");

    success &= horse->register_cmd(horse, horse->create(
        NULL,
        NULL,
        "hockey",
        " <walrus>",
        "rubber walrus protectors"));

    success &= log->register_cmd(log, horse);
    // END: NESTING TEST
#endif

    return success;
}


