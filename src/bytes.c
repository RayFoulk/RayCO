//------------------------------------------------------------------------|
// Copyright (c) 2018-2020 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

#include "bytes.h"
#include "blammo.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

//------------------------------------------------------------------------|
// bytes private implementation data
typedef struct
{
    // The raw data array
    uint8_t * data;

    // The total number of bytes allocated
    size_t length;

    // TODO: Add tokenization data, parallel metadata, etc...
}
bytes_priv_t;

//------------------------------------------------------------------------|
static void * bytes_data(bytes_t * bytes)
{
    if (bytes->empty(bytes))
    {
        return NULL;
    }

    return ((bytes_priv_t *) bytes->priv)->data;
}

//------------------------------------------------------------------------|
static inline size_t bytes_length(bytes_t * bytes)
{
    return ((bytes_priv_t *) bytes->priv)->length;
}

//------------------------------------------------------------------------|
static inline bool bytes_empty(bytes_t * bytes)
{
    return (NULL == ((bytes_priv_t *) bytes->priv)->data);
}


//------------------------------------------------------------------------|
static void bytes_clear(bytes_t * bytes)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

}

//------------------------------------------------------------------------|
static size_t bytes_trim(bytes_t * bytes)
{

}

//------------------------------------------------------------------------|
static bytes_t * bytes_copy(bytes_t * bytes)
{
    bytes_t * copy = NULL;

    return copy;
}

//------------------------------------------------------------------------|
static bytes_t * bytes_split(bytes_t * bytes, size_t begin, size_t end)
{
    bytes_t * seg = NULL;

    return seg;
}
//------------------------------------------------------------------------|
static bool bytes_join(bytes_t * head, bytes_t * tail)
{

    return true;
}

//------------------------------------------------------------------------|
// Not static because also exposed via the header, so that it can be
// included as a payload in other bytess.
void bytes_destroy(void * bytes_ptr)
{
    bytes_t * bytes = (bytes_t *) bytes_ptr;

}

//------------------------------------------------------------------------|
/*
static const bytes_t bytes_calls = {
    &bytes_create,
    &bytes_destroy,
    &bytes_data,
    &bytes_length,
    &bytes_empty,
    &bytes_origin,
    &bytes_clear,
    &bytes_insert,
    &bytes_remove,
    &bytes_reset,
    &bytes_spin,
    &bytes_trim,
    &bytes_sort,
    &bytes_copy,
    &bytes_split,
    &bytes_join,
    NULL
};
*/

//------------------------------------------------------------------------|
bytes_t * bytes_create(const char * str, size_t size)
{
    // Allocate and initialize public interface
    bytes_t * bytes = (bytes_t *) malloc(sizeof(bytes_t));
    if (!bytes)
    {
        BLAMMO(ERROR, "malloc(sizeof(bytes_t)) failed\n");
        return NULL;
    }

    // bulk copy all function pointers and init opaque ptr
    /* memcpy(bytes, &bytes_calls, sizeof(bytes_t)); */

    // Allocate and initialize private implementation
    bytes->priv = malloc(sizeof(bytes_priv_t));
    if (!bytes->priv)
    {
        BLAMMO(ERROR, "malloc(sizeof(bytes_priv_t)) failed\n");
        free(bytes);
        return NULL;
    }

    memset(bytes->priv, 0, sizeof(bytes_priv_t));
    /*((bytes_priv_t *) bytes->priv)->data_destroy = data_destroy; */

    return bytes;
}
