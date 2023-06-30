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
#include "utils.h"              // memzero(), function signatures
#include "blammo.h"
#include "scommand.h"
#include "sbuiltin.h"
#include "sroutine.h"
#include "console.h"
#include "collect.h"
#include "chain.h"
#include "bytes.h"

//------------------------------------------------------------------------|
// Various constants that define the syntax/dialect/behavior of scallop's
// "language" and environment.  Any of these could be made into factory
// options if necessary, but I don't anticipate having a need to do so.
static const int scallop_arg_hints_color = 35;
static const int scallop_arg_hints_bold = 0;

// The 'end-cap' that goes on the command line prompt
static const char * scallop_prompt_finale = " > ";

// This character is used to delimit nested context names
static const char * scallop_prompt_delim = ".";

// command line delimiters are pretty universal: whitespace is best.
// These come directly from 'man isspace'
static const char * scallop_cmd_delim = " \t\n\r\f\v";

// The string that designates everything to the right as a comment.
static const char * scallop_cmd_comment = "#";

// The begin/end markers for variable and argument substitution in
// unparsed command lines and routine arguments.  Whitespace between
// brackets may produce unexpected behavior!
static const char * scallop_var_begin = "{";
static const char * scallop_var_end = "}";
static const char * scallop_arg_prefix = "%";
static const char * scallop_arg_count = "n";        // "{%n}"
static const char * scallop_var_result = "?";       // "{%?}"

// Encapsulated token delimiters.  Allows for quoted strings and
// parenthetical expressions.
static const char * scallop_encaps_pairs[] = {
    "\"\"",     // "quoted strings"
    "()",       // (parenthetical expressions)
    NULL
};

//------------------------------------------------------------------------|
// scallop private implementation data
typedef struct
{
    // set true to break out of main loop
    bool quit;

    // Recursion depth for when executing scripts/procedures
    size_t depth;

    // There is intent to port this code to cc65 to target the C64
    // and possibly the Commander X16.  The cc65 compiler _does_ have
    // putenv() and getenv(), which I was tempted to use for all
    // variables, however putenv() has the limitation that it does not
    // copy the data, leaving the application to manage memory.  So
    // then if we're going to have to maintain a list of pointers
    // anyway, we might as well do variable management ourselves and
    // remain more portable that way.
    collect_t * variables;

    // Language construct stack used to keep track of nested routine
    // definitions, while loops, if-else and any other construct that
    // requires a beginning and end keyword with body in between that
    // is not immediately executed.
    chain_t * constructs;

    // Current prompt text buffer, rebuilt whenever context changes.
    bytes_t * prompt;

    // Root of the command-tree
    scallop_cmd_t * commands;

    // The list of all defined routines
    chain_t * routines;

    // Pointer to the console object for user I/O
    console_t * console;
}
scallop_priv_t;

//------------------------------------------------------------------------|
// Private data structure for context stack.  All data members must be
// unmanaged by this struct, but point elsewhere to data that persists
// for the lifetime of the context.
typedef struct
{
    // Name of this language construct
    char * name;

    // The context under which this item was pushed to the stack
    // I.E. It's probably the scallop instance itself, or the same
    // thing that was passed as the context pointer to a command
    // handler.
    void * context;

    // The construct object being operated on.  This might be the
    // 'routine' instance, or a 'while loop' instance or some other
    // added-on language construct.
    void * object;

    // The function to be called when a line is provided by
    // the user or a script.  If this is NULL then the line
    // should go directly to dispatch()or exec().  If not, then the
    // line handler function can decide if it needs to dispatch
    // or cache the line inside the construct object.
    scallop_construct_line_f linefunc;

    // The function to be called when this item is popped
    scallop_construct_pop_f popfunc;
}
scallop_construct_t;

//------------------------------------------------------------------------|
// trivial language construct copy function for chain inclusion
static void * scallop_construct_copy(const void * construct)
{
    scallop_construct_t * copy = (scallop_construct_t *)
            malloc(sizeof(scallop_construct_t));

    if (!copy)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_construct_t) failed!");
        return NULL;
    }

    // Shallow copy is OK for these structures
    memcpy(copy, construct, sizeof(scallop_construct_t));
    return copy;
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
    bytes_t * linebytes = bytes_pub.create(buffer, strlen(buffer));
    size_t argc = 0;

    char ** args = linebytes->tokenizer(linebytes,
                                        true,
                                        scallop_encaps_pairs,
                                        scallop_cmd_delim,
                                        scallop_cmd_comment,
                                        &argc);

    // Ignore empty input
    if (argc == 0)
    {
        //free(line);
        linebytes->destroy(linebytes);
        return;
    }

    // Strategy: the final unmatched command is the one that
    // may require tab completion.  Match known command keywords
    // as far as possible before making suggestions on incomplete
    // arguments/keywords
    scallop_cmd_t * parent = priv->commands;
    scallop_cmd_t * command = NULL;
    int nest = 0;

    for (nest = 0; nest < argc; nest++)
    {
        // Try to find each nested command by keyword
        command = priv->commands->find_by_keyword(parent, args[nest]);
        if (!command)
        {
            BLAMMO(DEBUG, "Command %s not found", args[nest]);
            break;
        }

        BLAMMO(DEBUG, "Command %s found!", args[nest]);
        parent = command;
    }

    BLAMMO(DEBUG, "parent keyword: %s  args[%d]: %s",
            parent->keyword(parent), nest, args[nest]);

    // Search through the parent command's sub-commands, but by starting
    // letter(s)/sub-string match rather than by first full keyword
    // match.  This returns a list of possible keywords that match the
    // given substring.  I.E. given: "lo" return [logoff, log, local]
    size_t longest = 0;
    chain_t * pmatches = parent->partial_matches(parent, args[nest], &longest);
    linebytes->destroy(linebytes);

    if (!pmatches || pmatches->length(pmatches) == 0) { return; }
    BLAMMO(DEBUG, "partial_matches length: %u  longest: %u",
                  pmatches->length(pmatches), longest);

    // Need to duplicate the line again, but this time leave the copy
    // mostly intact except that we want to modify the potentially
    // tab-completed argument in place and then feed the whole thing
    // back to console.  Just re-use the existing declared variables.
    // Basically strdup() manually, but allow some extra room for tab completed
    // keyword.  Maybe can use strndup() with a big n instead???
    linebytes = bytes_pub.create(buffer, strlen(buffer));
    linebytes->resize(linebytes, linebytes->size(linebytes) + longest);


    // get markers within unmodified copy of line
    args = linebytes->tokenizer(linebytes,
                                false,
                                scallop_encaps_pairs,
                                scallop_cmd_delim,
                                scallop_cmd_comment,
                                &argc);

    // Get the offset to where the argument needing tab completion begins
    ssize_t offset = linebytes->offset(linebytes, args[nest]);

    // iterate through partially matched keywords
    char * keyword = (char *) pmatches->first(pmatches);
    while (keyword)
    {
        // Add keyword + primary delimiter character at end of completion
        linebytes->resize(linebytes, offset);
        linebytes->append(linebytes, keyword, strlen(keyword));
        linebytes->append(linebytes, scallop_cmd_delim, 1);

        BLAMMO(DEBUG, "Adding tab completion line: \'%s\'",
                      linebytes->cstr(linebytes));

        priv->console->add_tab_completion(priv->console,
                                          linebytes->cstr(linebytes));

        keyword = (char *) pmatches->next(pmatches);
    }

    linebytes->destroy(linebytes);
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
    bytes_t * linebytes = bytes_pub.create(buffer, strlen(buffer));
    size_t argc = 0;

    char ** args = linebytes->tokenizer(linebytes,
                                        true,
                                        scallop_encaps_pairs,
                                        scallop_cmd_delim,
                                        scallop_cmd_comment,
                                        &argc);

    // Ignore empty input
    if (argc == 0)
    {
        linebytes->destroy(linebytes);
        return NULL;
    }

    // Hint strategy: match as many keywords as possible, traversing a single
    // path down the sub-command tree.  When we no longer have a matching
    // keyword, then display the hints for the last known matching command.
    // This should fail and return NULL right out of the gate if the first
    // keyord does not match.
    scallop_cmd_t * parent = priv->commands;
    scallop_cmd_t * command = NULL;
    int nest = 0;

    for (nest = 0; nest < argc; nest++)
    {
        // Try to find each nested command by keyword
        command = priv->commands->find_by_keyword(parent, args[nest]);
        if (!command)
        {
            BLAMMO(DEBUG, "Command %s not found", args[nest]);
            break;
        }

        BLAMMO(DEBUG, "Command %s found!", args[nest]);
        parent = command;
    }

    // No longer referencing args, OK to free underlying line
    linebytes->destroy(linebytes);

    // Return early before processing hints if there are none
    const char * arghints = parent->arghints(parent);
    if (!arghints)
    {
        BLAMMO(DEBUG, "No arg hints to provide");
        return NULL;
    }

    // https://stackoverflow.com/questions/13078926/is-there-a-way-to-count-tokens-in-c
    // Count tokens without modifying the original string, rather than
    // allocating another buffer.   The purpose is to determine which args
    // have been fulfilled and to only show hints for unfulfilled args.
    bytes_t * hintbytes = bytes_pub.create(arghints, strlen(arghints));
    ssize_t hindex = 0;
    size_t hintc = 0;

    char ** hints = hintbytes->tokenizer(hintbytes,
                                         false,
                                         scallop_encaps_pairs,
                                         scallop_cmd_delim,
                                         scallop_cmd_comment,
                                         &hintc);

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

    BLAMMO(DEBUG, "Un-coerced index is %d", hindex);
    if (hindex < 0 || hindex >= hintc)
    {
        BLAMMO(DEBUG, "Invalid hint index");
        return NULL;
    }

    // Set color/bold for hints.  Currently this is just a constant,
    // but could potentially be made contextual, perhaps reflicting
    // the type of argument expected.
    *color = scallop_arg_hints_color;
    *bold = scallop_arg_hints_bold;

    // Trick to avoid memory leak.  Need to deallocate hintbytes
    // but need to return hints.  offset within copy should be the
    // same as within the original.
    // Backup one character assuming there is a leading space
    ssize_t offset = hintbytes->offset(hintbytes, hints[hindex] - 1);

    // Destroy the copy of the arghints string
    hintbytes->destroy(hintbytes);

    return (char *) (arghints + offset);
}

//------------------------------------------------------------------------|
static scallop_t * scallop_create(console_t * console,
                                  scallop_registration_f registration,
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

    memzero(scallop->priv, sizeof(scallop_priv_t));
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

    // Create variables collection
    priv->variables = collect_pub.create();
    if (!priv->variables)
    {
        BLAMMO(FATAL, "collect_pub.create() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // Create context stack.  Could likely have just passed NULL
    // for the copy function, since we never intend to copy the
    // context, but it might happen later when we get into
    // more complex language constructs.
    priv->constructs = chain_pub.create(scallop_construct_copy,
                                        free);
    if (!priv->constructs)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // Initialize prompt, push prompt_base as initial 'construct'
    priv->prompt = bytes_pub.create(NULL, 0);
    scallop->construct_push(scallop, prompt_base, NULL, NULL, NULL, NULL);

    // Create the top-level list of commands
    priv->commands = scallop_cmd_pub.create(NULL, NULL, NULL, NULL, NULL);
    if (!priv->commands)
    {
        BLAMMO(FATAL, "scallop_cmd_pub.create() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // Register all initial commands if given a callback on create
    if (registration && !registration(scallop))
    {
        BLAMMO(FATAL, "register_builtin_commands() failed");
        scallop->destroy(scallop);
        return NULL;
    }

    // Create the list of routines.  Routines are not copied (for now)
    priv->routines = chain_pub.create(NULL,
                                      scallop_rtn_pub.destroy);
    if (!priv->routines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        scallop->destroy(scallop);
        return NULL;
    }

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

    // Destroy all routines
    if (priv->routines)
    {
        priv->routines->destroy(priv->routines);
    }

    // Recursively destroy command tree
    if (priv->commands)
    {
        priv->commands->destroy(priv->commands);
    }

    // Destroy the prompt
    if (priv->prompt)
    {
        priv->prompt->destroy(priv->prompt);
    }

    // Destroy language construct stack
    if (priv->constructs)
    {
        priv->constructs->destroy(priv->constructs);
    }

    // Destroy variables collection
    if (priv->variables)
    {
        priv->variables->destroy(priv->variables);
    }

    // zero out and destroy the private data
    memzero(scallop->priv, sizeof(scallop_priv_t));
    free(scallop->priv);

    // zero out and destroy the public interface
    memzero(scallop, sizeof(scallop_t));
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
    return priv->commands;
}

//------------------------------------------------------------------------|
static scallop_rtn_t * scallop_routine_by_name(scallop_t * scallop,
                                               const char * name)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Create a temporary working copy of a routine object
    scallop_rtn_t * routine = scallop_rtn_pub.create(name);
    if (!routine)
    {
        BLAMMO(ERROR, "scallop_rtn_pub.create(%s) failed", name);
        return NULL;
    }

    // Check if there is already a routine by the given name
    scallop_rtn_t * found = (scallop_rtn_t *)
            priv->routines->find(priv->routines,
                                 routine,
                                 routine->compare_name);

    // Destroy temporary working copy
    routine->destroy(routine);

    // Return the routine if it was found or NULL if not.
    return found;
}

//------------------------------------------------------------------------|
static scallop_rtn_t * scallop_routine_insert(scallop_t * scallop,
                                              const char * name)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Create a unique new routine object
    scallop_rtn_t * routine = scallop_rtn_pub.create(name);
    if (!routine)
    {
        BLAMMO(ERROR, "scallop_rtn_pub.create(%s) failed", name);
        return NULL;
    }

    // Store the new routine in the chain.
    priv->routines->insert(priv->routines, routine);
    return routine;
}

//------------------------------------------------------------------------|
static void scallop_routine_remove(scallop_t * scallop,
                                   const char * name)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    scallop_rtn_t * routine = scallop->routine_by_name(scallop, name);
    if (!routine)
    {
        BLAMMO(WARNING, "Routine \'%s\' not found", name);
        return;
    }

    // TODO: Consider locking for thread safety.  The routines chain should
    // be left on the link to remove after the find, but another thread
    // could conceivably perform an 'insert' in between calls.
    priv->routines->remove(priv->routines);
}

//------------------------------------------------------------------------|
static void scallop_store_args(scallop_t * scallop, int argc, char ** args)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // First clear out any old args, but this is only necessary if
    // there were previously more than there are now, since collect->set()
    // overwrites old entries.

    // Format of args in scallop 'code' will be: [%1] [%2] [%3] etc...
    // The literal name of the variables will be "%1" "%2" "%3" etc...
    // The number of total arguments is stored as "%N"

    bytes_t * varname = bytes_pub.print_create("%s%s",
                                               scallop_arg_prefix,
                                               scallop_arg_count);

    bytes_t * varvalue = priv->variables->get(priv->variables,
                                              varname->cstr(varname));

    // Having a "%N" stored is synonymous with having args stored,
    // but it is only necessary to clear excess previous args.
    int argc_stored = varvalue ? atoi(varvalue->cstr(varvalue)) : 0;
    int arg_num = 0;

    if (varvalue && (argc_stored > argc))
    {
        for (arg_num = argc; arg_num < argc_stored; arg_num++)
        {
            varname->print(varname, "%s%d", scallop_arg_prefix, arg_num);
            priv->variables->remove(priv->variables, varname->cstr(varname));
        }
    }

    // Store new arg count.  Collection takes ownership of object memory.
    // All scallop variables are stored as bytes_t instances.
    varname->print(varname, "%s%s", scallop_arg_prefix, scallop_arg_count);
    varvalue = bytes_pub.print_create("%d", argc);

    // Store the new argument count
    priv->variables->set(priv->variables,
                         varname->cstr(varname),
                         varvalue,
                         bytes_pub.copy,
                         bytes_pub.destroy);

    // Store all arguments
    for (arg_num = 0; arg_num < argc; arg_num++)
    {
        varname->print(varname, "%s%d", scallop_arg_prefix, arg_num);
        varvalue = bytes_pub.create(args[arg_num], strlen(args[arg_num]));

        priv->variables->set(priv->variables,
                             varname->cstr(varname),
                             varvalue,
                             bytes_pub.copy,
                             bytes_pub.destroy);
    }
}

//------------------------------------------------------------------------|
static void scallop_assign_variable(scallop_t * scallop,
                                    const char * varname,
                                    const char * varvalue)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    bytes_t * valuebytes = bytes_pub.create(varvalue,
                                            strlen(varvalue));

    priv->variables->set(priv->variables,
                         varname,
                         valuebytes,
                         bytes_pub.copy,
                         bytes_pub.destroy);
}

//------------------------------------------------------------------------|
// Private helper function to perform variable substitution
static bool scallop_variable_substitution(scallop_t * scallop,
                                          bytes_t * linebytes)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    ssize_t offset_begin = 0;
    ssize_t offset_end = 0;
    bytes_t * varname = bytes_pub.create(NULL, 0);
    bytes_t * varvalue = NULL;

    // work through the entire raw line, replacing variable references
    // "[variable_name]" with the string value of each variable.
    while (offset_begin >= 0)
    {
        offset_begin = linebytes->find_forward(linebytes,
                                               offset_end,
                                               scallop_var_begin,
                                               strlen(scallop_var_begin));
        if(offset_begin < 0)
        {
            BLAMMO(DEBUG, "No further variable references found.  begin: %ld",
                          offset_begin);
            break;
        }

        offset_end = linebytes->find_forward(linebytes,
                                             offset_begin,
                                             scallop_var_end,
                                             strlen(scallop_var_end));
        if(offset_end < 0)
        {
            BLAMMO(DEBUG, "No variable reference ending token found.  end: %ld",
                          offset_end);
            break;
        }

        // Extract variable name out of line
        varname->assign(varname, &linebytes->data(linebytes)[offset_begin + 1],
                                 offset_end - offset_begin - 1);
        BLAMMO(DEBUG, "varname: \'%s\'", varname->cstr(varname));

        // Get the variable referenced
        varvalue = (bytes_t *) priv->variables->get(priv->variables,
                                                    varname->cstr(varname));
        if (!varvalue)
        {
            priv->console->error(priv->console,
                                 "variable \'%s\' not found",
                                 varname->cstr(varname));
            //continue;
            varname->destroy(varname);
            return false;
        }

        linebytes->remove(linebytes,
                          offset_begin,
                          offset_end - offset_begin + 1);
        BLAMMO(DEBUG, "modified linebytes: %s", linebytes->cstr(linebytes));

        linebytes->insert(linebytes,
                          offset_begin,
                          varvalue->data(varvalue),
                          varvalue->size(varvalue));
        BLAMMO(DEBUG, "modified linebytes: %s", linebytes->cstr(linebytes));
    }

    varname->destroy(varname);
    return true;
}

//------------------------------------------------------------------------|
// Private helper function to set return value from last dispatch
// should return the same value that is passed in, but sets the
// special "%?" return value in the variable collection
static int scallop_set_result(scallop_t * scallop, int result)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    bytes_t * varname = bytes_pub.print_create("%s%s",
                                               scallop_arg_prefix,
                                               scallop_var_result);

    bytes_t * varvalue = bytes_pub.print_create("%d", result);

    priv->variables->set(priv->variables,
                         varname->cstr(varname),
                         varvalue,
                         bytes_pub.copy,
                         bytes_pub.destroy);

    varname->destroy(varname);

    return result;
}

//------------------------------------------------------------------------|
// Need to know the command to be executed AND have the unaltered
// line SIMULTANEOUSLY because the command->is_construct needs to be
// known in order to disposition the RAW LINE.
// NOTE: If and only if the line is to be tokenized & executed, should
// it have variable substitution/evaluation performed on it.  I.E. do
// not store mangled lines in constructs that have not yet been executed!
// TODO: In the rare case where a line is dispatched directly from
// the application's command-line (piped from stdin or given an option)
// it's conceivable that extra unparsed argc/argv given to main() could
// be overflowed into 'routine' style argument substitution, in addition
// to variable-sustitution. -- TBD at a later time.
// This would require a call to scallop_store_args() from main().

static void scallop_dispatch(scallop_t * scallop, const char * line)
{
    // Guard block NULL line ptr, or trivially empty line
    if (!line || !line[0])
    {
        BLAMMO(VERBOSE, "Ignoring NULL/empty line");
        // Don't update stored result for empty lines
        return;
    }

    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    BLAMMO(VERBOSE, "priv: %p depth: %u line: %s",
                    priv, priv->depth, line);

    // Limit recursion depth here
    priv->depth++;
    if (priv->depth > SCALLOP_MAX_RECURS)
    {
        priv->console->error(priv->console,
                             "maximum recursion depth %u reached",
                             SCALLOP_MAX_RECURS);
        priv->depth--;
        scallop_set_result(scallop, -1);
        return;
    }

    bytes_t * linebytes = bytes_pub.create(line, strlen(line));

    // Preprocessor-like variable substitution needs to happen
    // before calling tokenize, to allow for supporting spaces
    // in variables, multiple variables inside arguments, and
    // complex expression evaluation.
    if (!scallop_variable_substitution(scallop, linebytes))
    {
        priv->depth--;
        scallop_set_result(scallop, -2);
        return;
    }

    size_t argc = 0;
    char ** args = linebytes->tokenizer(linebytes,
                                        true,
                                        scallop_encaps_pairs,
                                        scallop_cmd_delim,
                                        scallop_cmd_comment,
                                        &argc);

    // Ignore empty input
    if (argc == 0)
    {
        BLAMMO(VERBOSE, "Ignoring empty tokenized line");
        linebytes->destroy(linebytes);
        priv->depth--;
        // Don't update stored result for empty lines
        return;
    }

    // Try to find the command being invoked
    scallop_cmd_t * command = priv->commands->find_by_keyword(priv->commands, args[0]);
    if (!command)
    {
        priv->console->error(priv->console,
                             "unknown command \'%s\'.  try \'help\'",
                             args[0]);

        linebytes->destroy(linebytes);
        priv->depth--;
        scallop_set_result(scallop, -3);
        return;
    }

    // Select the top item of the language construct stack
    // and get the current construct
    scallop_construct_t * construct = (scallop_construct_t *)
            priv->constructs->last(priv->constructs);

    // if in the middle of defining a routine, while loop,
    // or other user-registered language construct, then
    // call that construct's line handler function rather
    // than executing the line directly. AND if and only
    // if the command itself is not a construct keyword.
    int result = 0;
    if (!command->is_construct(command) && construct && construct->linefunc)
    {
        result = construct->linefunc(construct->context,
                                     construct->object,
                                     line);
    }
    else
    {
        result = command->exec(command, argc, args);
    }

    linebytes->destroy(linebytes);
    priv->depth--;
    scallop_set_result(scallop, result);
}

//------------------------------------------------------------------------|
static int scallop_loop(scallop_t * scallop, bool interactive)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    char * line = NULL;

    while (!priv->console->inputf_eof(priv->console) && !priv->quit)
    {
        // Get a line of raw user input
        line = priv->console->get_line(priv->console,
                                       priv->prompt->cstr(priv->prompt),
                                       interactive);
        if (!line)
        {
            BLAMMO(DEBUG, "get_line() returned NULL");
            continue;
        }

        BLAMMO(DEBUG, "About to dispatch(\'%s\')", line);
        scallop->dispatch(scallop, line);
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
    // TODO: Determine if it is ever necessary to pump a newline into
    // the console here just to get it off the blocking call?
    // This might not be known until threads are implemented.
}

//------------------------------------------------------------------------|
// Private prompt rebuild function on context push/pop
static void scallop_rebuild_prompt(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    scallop_construct_t * construct = NULL;

    priv->prompt->resize(priv->prompt, 0);

    // start at the bottom of the stack (first) and work towards the top
    construct = (scallop_construct_t *)
            priv->constructs->first(priv->constructs);
    while (construct)
    {
        if (!construct->name)
        {

            priv->console->error(priv->console,
                                 "NULL name for construct %p",
                                 construct);
            break;
        }

        priv->prompt->append(priv->prompt,
                             construct->name,
                             strlen(construct->name));

        construct = (scallop_construct_t *)
                priv->constructs->next(priv->constructs);

        // put a visual/syntactical delimiter between constructs
        // shown within the prompt.
        if (construct)
        {
            priv->prompt->append(priv->prompt,
                                 scallop_prompt_delim,
                                 strlen(scallop_prompt_delim));
        }
    }

    priv->prompt->append(priv->prompt,
                         scallop_prompt_finale,
                         strlen(scallop_prompt_finale));
}

//------------------------------------------------------------------------|
static void scallop_construct_push(scallop_t * scallop,
                                   const char * name,
                                   void * context,
                                   void * object,
                                   scallop_construct_line_f linefunc,
                                   scallop_construct_pop_f popfunc)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Create a context structure to be pushed
    scallop_construct_t * construct = (scallop_construct_t *)
            malloc(sizeof(scallop_construct_t));
    construct->name = (char *) name;
    construct->context = context;
    construct->object = object;
    construct->linefunc = linefunc;
    construct->popfunc = popfunc;

    // Push the context onto the stack, treating the 'last' link as
    // the top of the stack.  New construct becomes the new 'last'
    priv->constructs->last(priv->constructs);
    priv->constructs->insert(priv->constructs, construct);

    scallop_rebuild_prompt(scallop);
}

//------------------------------------------------------------------------|
static int scallop_construct_pop(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;

    // Do not allow popping the final element, which is the base prompt
    if (priv->constructs->length(priv->constructs) <= 1)
    {
        priv->console->error(priv->console, "construct stack is empty");
        return -1;
    }

    // Select the top item and get the context
    scallop_construct_t * construct = (scallop_construct_t *)
        priv->constructs->last(priv->constructs);

    // Call the pop function if one is provided
    int result = 0;
    if (construct && construct->popfunc)
    {
        result = construct->popfunc(construct->context, construct->object);
    }

    // Remove item from the stack, moving all other links back
    priv->constructs->remove(priv->constructs);

    scallop_rebuild_prompt(scallop);
    return result;
}

//------------------------------------------------------------------------|
const scallop_t scallop_pub = {
    &scallop_create,
    &scallop_destroy,
    &scallop_console,
    &scallop_commands,
    &scallop_routine_by_name,
    &scallop_routine_insert,
    &scallop_routine_remove,
    &scallop_store_args,
    &scallop_assign_variable,
    &scallop_dispatch,
    &scallop_loop,
    &scallop_quit,
    &scallop_construct_push,
    &scallop_construct_pop,
    NULL
};
