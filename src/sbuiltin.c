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
#include "sroutine.h"
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

    // Getting the longest command has to occur before diving into
    // recursive help, because the top level help must be told.
    // otherwise, recursive help will only determine the longest
    // for each sub-branch.
    size_t longest_kw_and_hints = 0;
    cmds->longest(cmds, &longest_kw_and_hints, NULL, NULL, NULL);

    help->print(help, "\r\ncommands:\r\n\r\n");

    int result = cmds->help(cmds, help, 0, longest_kw_and_hints);
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
static int builtin_handler_unregister(void * scmd,
                                      void * context,
                                      int argc,
                                      char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected keyword to unregister");
        return -1;
    }

    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[1]);
    if (!cmd)
    {
        BLAMMO(WARNING, "Command %s not found", args[1]);
        return -2;
    }

    if (!cmd->is_mutable(cmd))
    {
        console->error(console,
                "can't unregister immutable command \'%s\'",
                cmd->keyword(cmd));
        return -3;
    }

    // NOTE: It is highly unlikely that a command would be both
    // mutable AND a language construct.  Even if so, the construct
    // stack should probably be left alone here.

    // remove the associated routine if the command was also a routine.
    // This will just fail without consequence if the command is not.
    scallop->routine_remove(scallop, cmd->keyword(cmd));

    // Unregister the command -- this also destroys the command
    if (!scope->unregister_cmd(scope, cmd))
    {
        console->error(console,
                "unregister_cmd(%s) failed",
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

    // TODO: The answer to nested help may be staring me in the face here,
    // versus a complex approach to having parent links in all commands.
    // either-or, but the to-do is to evaluate approaches here and make
    // a decision.
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
static int builtin_handler_print(void * scmd,
                                 void * context,
                                 int argc,
                                 char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);
    int argnum = 1;

    // TODO: use bytes_t append() to avoid all the unwanted newlines
    //  and also perform variable lookup / substitution here also
    //  so that the user can view variable values interactively.
    // TODO: also evaluate expressions here, as well as 'while <expr>'
    //  and 'if <expr>'.  This would allow things like 'print <expr>'
    //  where <expr> can contain arguments or variables
    for (argnum = 1; argnum < argc; argnum++)
    {
        console->print(console, "%s ", args[argnum]);
    }

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_source(void * scmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

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

    // Stash the current console input source & swap to source file
    FILE * input = console->get_inputf(console);
    console->set_inputf(console, source);

    // Now get an dispatch commands from the script until EOF
    int result = scallop->loop(scallop, false);

    // Put console back to original state
    console->set_inputf(console, input);

    // Done with script file -- TODO: return status of script run?
    fclose(source);
    return result;
}

//------------------------------------------------------------------------|
static int builtin_linefunc_routine(void * context,
                                    void * object,
                                    const char * line)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop_rtn_t * routine = (scallop_rtn_t *) object;

    // put the raw line as-is in the routine.  variable substitutions
    // and tokenization will occur later during routine execution.
    routine->append(routine, line);
    (void) scallop;

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_popfunc_routine(void * context,
                                   void * object)
{
    BLAMMO(VERBOSE, "");

    scallop_t * scallop = (scallop_t *) context;
    scallop_rtn_t * routine = (scallop_rtn_t *) object;
    scallop_cmd_t * cmds = scallop->commands(scallop);
    bool success = true;

    // All done defining this routine.  Now register it as a
    // proper command.
    scallop_cmd_t * cmd = cmds->create(
            routine->handler,
            scallop,
            routine->name(routine),
            " [argument-list]",
            "user-registered routine");

    // should be allowed to delete/modify routines
    cmd->set_mutable(cmd);

    success = cmds->register_cmd(cmds, cmd);

    return success ? 0 : -1;
}

//------------------------------------------------------------------------|
// This routine performs the _definition_ of a routine.  Future incoming
// lines will be handled by the 'routine' language construct's linefunc
// callback.  Lines may come from the user interactively or from a script.
// The actual _execution_ of a routine will occur later after the popfunc
// is called and the routine is registered.
static int builtin_handler_routine(void * scmd,
                                   void * context,
                                   int argc,
                                   char ** args)
{
    BLAMMO(VERBOSE, "");

    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        BLAMMO(ERROR, "Expected a name for the routine");
        return -1;
    }

    // Check if there is already a routine by the given name
    scallop_rtn_t * routine = scallop->routine_by_name(scallop, args[1]);
    if (routine)
    {
        console->error(console,
                       "routine \'%s\' already exists",
                       args[1]);
        return -2;
    }

    // Create a unique new routine object
    routine = scallop->routine_insert(scallop, args[1]);
    if (!routine)
    {
        BLAMMO(ERROR, "scallop->routine_insert(%s) failed", args[1]);
        return -3;
    }

    // Push the new routine definition onto the language construct
    // stack.  This should get popped off when the matching 'end'
    // statement is reached, and at THAT point it will become
    // registered as a new command (with the ubiquitous routine handler)
    // Until then, incoming lines will be added to the construct.
    scallop->construct_push(scallop,
                            routine->name(routine),
                            context,
                            routine,
                            builtin_linefunc_routine,
                            builtin_popfunc_routine);

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_end(void * scmd,
                               void * context,
                               int argc,
                               char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    return scallop->construct_pop(scallop);
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
bool register_builtin_commands(void * scallop_ptr)
{
    scallop_t * scallop = (scallop_t *) scallop_ptr;
    scallop_cmd_t * cmds = scallop->commands(scallop);
    scallop_cmd_t * cmd = NULL;
    bool success = true;

    // TODO: add 'while' as a language construct
    // TODO: need to add variables, substitution,
    //   and an expression evaluator to do this.
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

    // TODO: Also, removing a thing should ALWAYS remove all
    //  of it's aliases.  otherwise the aliases are present
    //  but invalid and could cause weird crashes/undefined behavior.
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_unregister,
        scallop,
        "unregister",
        " <command-keyword>",
        "unregister a mutable command"));

    scallop_cmd_t * log = cmds->create(
        builtin_handler_log,
        NULL,
        "log",
        " <log-command> <...>",
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
        " <log-file-path>",
        "change the blammo log file path"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_print,
        scallop,
        "print",
        " [arbitrary-strings-and-variables]",
        "print strings and (TODO: variables)"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_source,
        scallop,
        "source",
        " <script-path>",
        "load and run a command script"));

    cmd = cmds->create(
        builtin_handler_routine,
        scallop,
        "routine",
        " <routine-name> ...",
        "define and register a new routine");
    cmd->set_construct(cmd);
    success &= cmds->register_cmd(cmds, cmd);

    cmd = cmds->create(
        builtin_handler_end,
        scallop,
        "end",
        NULL,
        "finalize a multi-line language construct");
    cmd->set_construct(cmd);
    success &= cmds->register_cmd(cmds, cmd);

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
