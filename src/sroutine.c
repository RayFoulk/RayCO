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
//#include <string.h>
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
static scallop_rtn_t * scallop_rtn_create(const char * name)
{

    return NULL;
}

//------------------------------------------------------------------------|
static void scallop_rtn_destroy(void * routine)
{

}

//------------------------------------------------------------------------|
static const char * scallop_rtn_name(scallop_rtn_t * routine)
{

    return "";
}

//------------------------------------------------------------------------|
static bool scallop_rtn_append(scallop_rtn_t * routine, const char * line)
{

    return true;
}

//------------------------------------------------------------------------|
static int scallop_rtn_exec(scallop_rtn_t * routine, int argc, char ** args)
{

    return 0;
}

//------------------------------------------------------------------------|
const scallop_rtn_t scallop_rtn_pub = {
    &scallop_rtn_create,
    &scallop_rtn_destroy,
    &scallop_rtn_name,
    &scallop_rtn_append,
    &scallop_rtn_exec,
    NULL
};
