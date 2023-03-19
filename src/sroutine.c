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

//#include <stdio.h>
//#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
//#include <stddef.h>
//#include <errno.h>

//#include "scallop.h"
//#include "scommand.h"
#include "sroutine.h"
#include "console.h"
#include "chain.h"
#include "bytes.h"
#include "utils.h"
#include "blammo.h"

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
// TODO: fairly certain going to need the console pointer here TBD
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

    memset(routine->priv, 0, sizeof(scallop_rtn_priv_t));
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
    // need to track the nested depth of an 'end' keyword (if multi-use)
    // or else have a special 'end-routine' keyword.  TODO: figure out
    // how to cleanly implement this.
    priv->lines = chain_pub.create(bytes_pub.destroy);
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

    memset(routine->priv, 0, sizeof(scallop_rtn_priv_t));
    free(routine->priv);

    // zero out and destroy the public interface
    memset(routine, 0, sizeof(scallop_rtn_t));
    free(routine);
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
    bytes_t * bline = bytes_pub.create(line, strlen(line));

    // Make no assumptions about the state of the chain, but always
    // force the insert at the 'end' of the chain, which is always at -1
    // since the chain is circular.
    priv->lines->reset(priv->lines);
    priv->lines->spin(priv->lines, -1);
    priv->lines->insert(priv->lines, bline);
}

//------------------------------------------------------------------------|
static int scallop_rtn_handler(void * scmd, void * context, int argc, char ** args)
{
    // The command handler function for every routine once registered.
    // get the routine by name, perform substitutions on lines/args,
    // and iterate through the lines calling dispatch on each one
    // until running out and then return.

    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return 0;
}

//------------------------------------------------------------------------|
const scallop_rtn_t scallop_rtn_pub = {
    &scallop_rtn_create,
    &scallop_rtn_destroy,
    &scallop_rtn_name,
    &scallop_rtn_append,
    &scallop_rtn_handler,
    NULL
};
