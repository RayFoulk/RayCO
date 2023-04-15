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

#include "sroutine.h"
#include "utils.h"              // memzero()
#include "blammo.h"
#include "scommand.h"
#include "scallop.h"
#include "console.h"
#include "chain.h"
#include "bytes.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The name of this routine
    bytes_t * name;

    // Raw command lines consisting of the routine body
    chain_t * lines;
}
scallop_rtn_priv_t;

//------------------------------------------------------------------------|
static scallop_rtn_t * scallop_rtn_create(const char * name)
{
    scallop_rtn_t * routine = (scallop_rtn_t *) malloc(
            sizeof(scallop_rtn_t));
    if (!routine)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_rtn_t)) failed");
        return NULL;
    }

    memcpy(routine, &scallop_rtn_pub, sizeof(scallop_rtn_t));

    routine->priv = malloc(sizeof(scallop_rtn_priv_t));
    if (!routine->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_rtn_priv_t)) failed");
        free(routine);
        return NULL;
    }

    memzero(routine->priv, sizeof(scallop_rtn_priv_t));
    scallop_rtn_priv_t * priv = (scallop_rtn_priv_t *) routine->priv;

    // Name of this routine (NO SPACES!!! - FIXME filter this)
    priv->name = bytes_pub.create(name, strlen(name));
    if (!priv->name)
    {
        BLAMMO(FATAL, "bytes_pub.create(%s) failed", name);
        routine->destroy(routine);
        return NULL;
    }

    // List of raw (mostly) uninterpreted command lines consisting
    // of the body of the routine.  One exception to this is we'll
    // need to track the nested depth of an 'end' keyword (multi-use)
    priv->lines = chain_pub.create(bytes_pub.copy,
                                   bytes_pub.destroy);
    if (!priv->lines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        routine->destroy(routine);
        return NULL;
    }

    return routine;
}

//------------------------------------------------------------------------|
static void scallop_rtn_destroy(void * routine_ptr)
{
    scallop_rtn_t * routine = (scallop_rtn_t *) routine_ptr;
    if (!routine || !routine->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    scallop_rtn_priv_t * priv = (scallop_rtn_priv_t *) routine->priv;

    if (priv->lines)
    {
        priv->lines->destroy(priv->lines);
    }

    if (priv->name)
    {
        priv->name->destroy(priv->name);
    }

    memzero(routine->priv, sizeof(scallop_rtn_priv_t));
    free(routine->priv);

    // zero out and destroy the public interface
    memzero(routine, sizeof(scallop_rtn_t));
    free(routine);
}

static int scallop_rtn_compare_name(const void * routine,
                                    const void * other)
{
    scallop_rtn_t * routine1 = (scallop_rtn_t *) routine;
    scallop_rtn_priv_t * priv1 = (scallop_rtn_priv_t *) routine1->priv;

    scallop_rtn_t * routine2 = (scallop_rtn_t *) other;
    scallop_rtn_priv_t * priv2 = (scallop_rtn_priv_t *) routine2->priv;

    return priv1->name->compare(priv1->name, priv2->name);
}
//------------------------------------------------------------------------|
static inline const char * scallop_rtn_name(scallop_rtn_t * routine)
{
    scallop_rtn_priv_t * priv = (scallop_rtn_priv_t *) routine->priv;
    return priv->name->cstr(priv->name);
}

//------------------------------------------------------------------------|
static void scallop_rtn_append(scallop_rtn_t * routine, const char * line)
{
    scallop_rtn_priv_t * priv = (scallop_rtn_priv_t *) routine->priv;

    // First create the line object
    bytes_t * linebytes = bytes_pub.create(line, strlen(line));

    // Make no assumptions about the state of the chain, but always
    // force the insert at the 'end' of the chain, which is always at -1
    // since the chain is circular.
    priv->lines->reset(priv->lines);
    priv->lines->spin(priv->lines, -1);
    priv->lines->insert(priv->lines, linebytes);
}

//------------------------------------------------------------------------|
static int scallop_rtn_handler(void * scmd,
                               void * context,
                               int argc,
                               char ** args)
{
    // The command handler function for every routine once registered.
    // get the routine by name, perform substitutions on lines/args,
    // and iterate through the lines calling dispatch on each one
    // until running out and then return.
    // args[0] happens to be the name of the routine,
    // But it is more reliably also cmd->name.
    scallop_cmd_t * cmd = (scallop_cmd_t *) scmd;
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    // Find _this_ routine.  TODO: Is there a faster way to
    // find/get the routine associated with the command we're
    // executing?  seems like there should be.  revisit this later.
    scallop_rtn_t * routine = scallop->routine_by_name(scallop,
                                                       cmd->keyword(cmd));
    if (!routine)
    {
        console->error(console,
                       "routine \'%s\' not found",
                       cmd->keyword(cmd));
        return -1;
    }

    scallop_rtn_priv_t * priv = (scallop_rtn_priv_t *) routine->priv;
    bytes_t * linebytes = NULL;

    // Iterate through all lines and dispatch each
    priv->lines->reset(priv->lines);
    do
    {
        linebytes = (bytes_t *) priv->lines->data(priv->lines);
        if (linebytes)
        {
            // Store subroutine arguments in scallop's variable
            // collection so dispatch can perform substitution.
            scallop->store_args(scallop, argc, args);

            BLAMMO(DEBUG, "About to dispatch(\'%s\')",
                          linebytes->cstr(linebytes));

            scallop->dispatch(scallop, (char *)
                              linebytes->cstr(linebytes));
        }
    }
    while(priv->lines->spin(priv->lines, 1));

    return 0;
}

//------------------------------------------------------------------------|
const scallop_rtn_t scallop_rtn_pub = {
    &scallop_rtn_create,
    &scallop_rtn_destroy,
    &scallop_rtn_compare_name,
    &scallop_rtn_name,
    &scallop_rtn_append,
    &scallop_rtn_handler,
    NULL
};
