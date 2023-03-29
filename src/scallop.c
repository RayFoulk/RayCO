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
#include "sroutine.h"
#include "console.h"
#include "chain.h"
#include "bytes.h"
#include "utils.h"
#include "blammo.h"

//------------------------------------------------------------------------|
// String constants that define the syntax/dialect of scallop's "language"
// These could be made options if necessary, but I don't anticipate having
// a need to do so.

// The 'end-cap' that goes on the command line prompt
static const char * scallop_prompt_finale = " > ";

// This character is used to delimit nested context names
static const char * scallop_prompt_delim = ".";

// command line delimiters are pretty universal: whitespace is best.
static const char * scallop_cmd_delim = " \t\n";

// The string that designates everything to the right as a comment.
static const char * scallop_cmd_comment = "#";

//------------------------------------------------------------------------|
// scallop private implementation data
typedef struct
{
    // set true to break out of main loop
    bool quit;

    // Recursion depth for when executing scripts/procedures
    size_t depth;

    // Language construct stack used to keep track of nested routine
    // definitions, while loops, if-else and any other construct that
    // requires a beginning and end keyword with body in between that
    // is not immediately executed.
    chain_t * constructs;

    // Current prompt text buffer, rebuilt whenever context changes.
    bytes_t * prompt;

    // master list of top-level context commands
    scallop_cmd_t * cmds;

    // The list of all defined routines
    chain_t * routines;

    // Pointer to the console object for user I/O
    console_t * console;
}
scallop_priv_t;

// Private data structure for context stack
// All data members must be unmanaged by this struct,
// but point elsewhere to persistent data, at least
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
    char ** args = linebytes->tokenize(linebytes,
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
    args = linebytes->marktokens(linebytes,
                                 scallop_cmd_delim,
                                 scallop_cmd_comment,
                                 &argc);

    // Get the offset to where the argument needing tab completion begins
    ssize_t offset = linebytes->offset(linebytes, args[nest]);

    // iterate through partially matched keywords
    char * keyword = NULL;
    pmatches->reset(pmatches);
    do
    {
        keyword = (char *) pmatches->data(pmatches);

        // Add keyword + primary delimiter character at end of completion
        linebytes->resize(linebytes, offset);
        linebytes->append(linebytes, keyword, strlen(keyword));
        linebytes->append(linebytes, scallop_cmd_delim, 1);

        BLAMMO(DEBUG, "Adding tab completion line: \'%s\'",
                      linebytes->cstr(linebytes));

        priv->console->add_tab_completion(priv->console,
                                          linebytes->cstr(linebytes));
    }
    while (pmatches->spin(pmatches, 1));

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
    char ** args = linebytes->tokenize(linebytes,
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
    char ** hints = hintbytes->marktokens(hintbytes,
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

    BLAMMO(DEBUG, "Uncoerced index is %d", hindex);
    if (hindex < 0 || hindex >= hintc)
    {
        BLAMMO(DEBUG, "Invalid hint index");
        return NULL;
    }

    // TODO: Make these constants or defines
    *color = 35;
    *bold = 0;

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

    // Create context stack
    priv->constructs = chain_pub.create(free);
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
    priv->cmds = scallop_cmd_pub.create(NULL, false, NULL, NULL, NULL, NULL);
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

    // Create the list of routines
    priv->routines = chain_pub.create(scallop_rtn_pub.destroy);
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
    if (priv->cmds)
    {
        priv->cmds->destroy(priv->cmds);
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
static inline chain_t * scallop_routines(scallop_t * scallop)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    return priv->routines;
}

//------------------------------------------------------------------------|
static int scallop_dispatch(scallop_t * scallop, char * line)
{
    // Guard block NULL line ptr
    if (!line)
    {
        BLAMMO(VERBOSE, "Ignoring NULL line");
        return 0;
    }

    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    BLAMMO(VERBOSE, "priv: %p depth: %u line: %s",
                    priv, priv->depth, line);

    // Keep track of recursion depth here
    priv->depth++;
    if (priv->depth > SCALLOP_MAX_RECURS)
    {
        BLAMMO(ERROR, "Maximum recursion depth %u reached",
                      SCALLOP_MAX_RECURS);
        priv->depth--;
        return -1;
    }

     // FIXME / TODO : preprocessor-like variable substitution
    //  probably needs to happen here BEFORE calling split.
    //  Also strongly consider re-implementing this part inside
    //  bytes_t, since replacement will generally be a different
    //  size than original.

    // ORDER MATTERS HERE
    // 1.) need to know the command to be executed
    // AND have the unaltered line SIMULTANEOUSLY
    // because the command->is_construct needs to be known
    // in order to disposition the RAW LINE.
    //  therefore it cannot be chopped up destructively.
    // 2.) THEN, if and only if the line is to be executed,
    // should it have variable substitution/evaluation performed
    // on it.  I.E. do not store mangled lines in constructs
    // that have not yet been executed!

    bytes_t * linebytes = bytes_pub.create(line, strlen(line));
    size_t argc = 0;
    char ** args = linebytes->tokenize(linebytes,
                                       scallop_cmd_delim,
                                       scallop_cmd_comment,
                                       &argc);

    // Ignore empty input
    if (argc == 0)
    {
        BLAMMO(VERBOSE, "Ignoring empty line");

        linebytes->destroy(linebytes);
        priv->depth--;
        return 0;
    }

    // Try to find the command being invoked
    scallop_cmd_t * cmd = priv->cmds->find_by_keyword(priv->cmds, args[0]);
    if (!cmd)
    {
        priv->console->print(priv->console,
                             "Unknown command \'%s\'.  Try \'help\'",
                             args[0]);

        linebytes->destroy(linebytes);
        priv->depth--;
        return -1;
    }


    // TODO: If in the mode of defining a routine, then
    // the command should not be executed, but instead
    // should be added to the routine definition UNTIL
    // an 'immediate' command is encountered.
    // WWWWWWWWWWWWWWW
    // answer: private get_context() struct

    // Select the top item of the construct stack
    priv->constructs->reset(priv->constructs);
    priv->constructs->spin(priv->constructs, 1);

    // Get the current construct
    scallop_construct_t * construct = (scallop_construct_t *)
            priv->constructs->data(priv->constructs);

    int result = 0;
    // if in the middle of defining a routine, while loop,
    // or other user-registered language construct, then
    // call that construct's line handler function rather
    // than executing the line directly. AND if and only
    // if the command itself is not a construct keyword.
    if (!cmd->is_construct(cmd) && construct && construct->linefunc)
    {
        result = construct->linefunc(construct->context,
                                     construct->object,
                                     line);
    }
    else
    {
        result = cmd->exec(cmd, argc, args);
    }

    linebytes->destroy(linebytes);
    priv->depth--;
    return result;
}

//------------------------------------------------------------------------|
static int scallop_loop(scallop_t * scallop, bool interactive)
{
    scallop_priv_t * priv = (scallop_priv_t *) scallop->priv;
    char * line = NULL;
    int result = 0;

    while (!priv->console->input_eof(priv->console) && !priv->quit)
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
        result = scallop->dispatch(scallop, line);
        BLAMMO(INFO, "Result of dispatch(%s) is %d", line, result);

        // TODO possibly store result in priv, or else report on console
        // TODO: need to implement variables (dict)
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
    priv->constructs->reset(priv->constructs);
    do
    {
        // put a visual/syntactical delimiter between constructs
        // shown within the prompt.
        if (construct)
        {
            priv->prompt->append(priv->prompt,
                                 scallop_prompt_delim,
                                 strlen(scallop_prompt_delim));
        }

        // start at the bottom of the stack (origin)
        // and work towards the top (origin+1) by
        // working around backwards.
        construct = (scallop_construct_t *)
                priv->constructs->data(priv->constructs);
        if (!construct)
        {
            BLAMMO(ERROR, "NULL construct in stack!");
            break;
        }

        priv->prompt->append(priv->prompt,
                             construct->name,
                             strlen(construct->name));

    }
    while (priv->constructs->spin(priv->constructs, -1));

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

    // Push the context onto the stack, treating the link after the
    // origin as the 'top' of the stack, pushing all other links forward.
    priv->constructs->reset(priv->constructs);
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
        priv->console->print(priv->console, "error: construct stack is empty");
        return -1;
    }

    // Select the top item
    priv->constructs->reset(priv->constructs);
    priv->constructs->spin(priv->constructs, 1);

    // Get the context
    scallop_construct_t * construct = (scallop_construct_t *)
            priv->constructs->data(priv->constructs);
    int result = 0;

    // Call the pop function if provided
    if (construct->popfunc)
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
    &scallop_routines,
    &scallop_dispatch,
    &scallop_loop,
    &scallop_quit,
    &scallop_construct_push,
    &scallop_construct_pop,
    NULL
};
