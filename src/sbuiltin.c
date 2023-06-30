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
#include "sparser.h"
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
    console_t * console = scallop->console(scallop);
    scallop_cmd_t * cmds = scallop->commands(scallop);

    // If the caller provided additional keywords I.E. "help thingy"
    // then try to provide help for the specified command instead
    // of just everything (by default)
    scallop_cmd_t * focus = NULL;
    scallop_cmd_t * found = NULL;

    // TODO: Support more than one topic
    if (argc > 1)
    {
        found = cmds->find_by_keyword(cmds, args[1]);
        if (!found)
        {
            console->error(console, "command %s not found", args[1]);
            return -1;
        }

        // Make a copy of found command so the original doesn't get destroyed
        found = found->copy(found);

        focus = scallop_cmd_pub.create(NULL, NULL, NULL, NULL, NULL);
        focus->register_cmd(focus, found);
        cmds = focus;
    }

    // Getting the longest command has to occur before diving into
    // recursive help, because the top level help must be told.
    // otherwise, recursive help will only determine the longest
    // for each sub-branch.
    const char * start = "\r\ncommands:\r\n\r\n";
    bytes_t * help = bytes_pub.create(start, strlen(start));
    size_t longest_kw_and_hints = 0;
    cmds->longest(cmds, &longest_kw_and_hints, NULL, NULL, NULL);

    // TODO: Improve help column formatting so that indented sub-commands
    //  align consistently with base level help.
    int result = cmds->help(cmds, help, 0, longest_kw_and_hints);
    if (result < 0)
    {
        console->error(console, "help for commands failed with %d", result);
        help->destroy(help);
        return result;
    }

    console->print(console, "%s", help->cstr(help));
    help->destroy(help);
    if (focus) { focus->destroy(focus); }

    return result;
}

//------------------------------------------------------------------------|
static int builtin_handler_alias(void * scmd,
                                 void * context,
                                 int argc,
                                 char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

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
    if (argc < 2)
    {
        console->error(console, "expected an alias keyword");
        return -1;
    }
    else if (argc < 3)
    {
        console->error(console, "expected a command to be aliased");
        return -2;
    }

    // Find the command that is referenced
    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[2]);
    if (!cmd)
    {
        console->error(console, "command %s not found", args[2]);
        return -3;
    }

    // re-register the same command under a different keyword
    scallop_cmd_t * alias = scope->alias(cmd, args[1]);
    if (!scope->register_cmd(scope, alias))
    {
        console->error(console, "failed to register alias %s to %s",
                                args[1], args[2]);
        return -4;
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
        console->error(console, "expected a command keyword to unregister");
        return -1;
    }

    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[1]);
    if (!cmd)
    {
        console->error(console, "command %s not found", args[1]);
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
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a log sub-command");
        return -1;
    }

    // Find and execute subcommand
    scallop_cmd_t * log = (scallop_cmd_t *) scmd;
    scallop_cmd_t * cmd = log->find_by_keyword(log, args[1]);
    if (!cmd)
    {
        console->error(console, "log sub-command %s not found", args[1]);
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
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a numeric log level 0-5");
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
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a boolean value");
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
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a file path/name");
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
    long result = 0;
    int argnum = 1;

    // Need something to print!
    if (argc < 2)
    {
        console->error(console, "expected an expression to print");
        return -1;
    }

    // Print each argument/expression on its own line
    // only keep track of and return the most recent result
    for (argnum = 1; argnum < argc; argnum++)
    {
        // Still testing this approach of having the sparser
        // determine if the expression should be evaluated.
        // 'print' tries to mimic the behavior of 'assign'
        // Only printing instead of assigning.
        if (sparser_is_expr(args[argnum]))
        {
            result = sparser_evaluate(console->error, console, args[argnum]);
            if (result == SPARSER_INVALID_EXPRESSION)
            {
                console->error(console,
                               "invalid expression \'%\'",
                               args[argnum]);
            }
            else
            {
                console->print(console, "%ld", result);
            }
        }
        else
        {
            console->print(console, "%s", args[argnum]);
        }
    }

    return (int) result;
}

//------------------------------------------------------------------------|
static int builtin_handler_assign(void * scmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);
    long result = 0;

    if (argc < 2)
    {
        console->error(console, "expected a variable name");
        return -1;
    }
    else if (argc < 3)
    {
        console->error(console, "expected a variable value");
        return -2;
    }

    // Evaluate expression down to a numeric value if it looks like
    // we should, based on some very shallow and flakey criteria
    if (sparser_is_expr(args[2]))
    {
        result = sparser_evaluate(console->error, console, args[2]);
        if (result == SPARSER_INVALID_EXPRESSION)
        {
            console->error(console,
                           "not assigning \'%s\' from invalid expression \'%\'",
                           args[1],
                           args[2]);
            return -3;
        }

        // Numeric assignment
        bytes_t * value = bytes_pub.print_create("%ld", result);
        scallop->assign_variable(scallop, args[1], value->cstr(value));
        value->destroy(value);
    }
    else
    {
        // Direct string assignment
        scallop->assign_variable(scallop, args[1], args[2]);
    }

    return result;
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
        console->error(console, "expected a file path argument");
        return -1;
    }

    FILE * source = fopen(args[1], "r");
    if (!source)
    {
        console->error(console, "could not open %s for reading", args[1]);
        return -2;
    }

    // Stash the current console input source & swap to source file
    FILE * input = console->get_inputf(console);
    console->set_inputf(console, source);

    // Now get an dispatch commands from the script until EOF
    int result = scallop->loop(scallop, false);

    // Put console back to original state
    console->set_inputf(console, input);

    // Done with script file
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
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_MUTABLE);

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
        console->error(console, "expected a routine name");
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
        console->error(console, "create routine \'%s\' failed", args[1]);
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

    // TODO: add 'while' as a language construct.
    //  need to add an expression evaluator to do this.
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_help,
        scallop,
        "help",
        NULL,
        "show a list of commands with hints and description"));
    // TODO? To assist with allowing 'help' to target specific subcommands,
    // all base level commands would have to be re-registered
    // as subcommands under help in order for tab-completion to work.
    // Is this even feasible?  Or would it break the whole model?

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
        "unreg",
        " <command-keyword>",
        "unregister a mutable command"));

    scallop_cmd_t * log = cmds->create(
        builtin_handler_log,
        scallop,
        "log",
        " <log-command> <...>",
        "change blammo logger options");

    success &= cmds->register_cmd(cmds, log);

    success &= log->register_cmd(log, log->create(
        builtin_handler_log_level,
        scallop,
        "level",
        " <0..5>",
        "change the blammo log message level (0=VERBOSE, 5=FATAL)"));

    success &= log->register_cmd(log, log->create(
        builtin_handler_log_stdout,
        scallop,
        "stdout",
        " <true/false>",
        "enable or disable logging to stdout"));

    success &= log->register_cmd(log, log->create(
        builtin_handler_log_file,
        scallop,
        "file",
        " <log-file-path>",
        "change the blammo log file path"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_print,
        scallop,
        "print",
        " [arbitrary-expression(s)]",
        "print expressions, strings, and variables"));

    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_assign,
        scallop,
        "assign",
        " <var-name> <value>",
        "assign a value to a variable"));

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
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT);
    success &= cmds->register_cmd(cmds, cmd);

    cmd = cmds->create(
        builtin_handler_end,
        scallop,
        "end",
        NULL,
        "finalize a multi-line language construct");
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT);
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
