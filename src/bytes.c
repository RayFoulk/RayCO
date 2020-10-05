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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

//------------------------------------------------------------------------|
// bytes private implementation data
typedef struct
{
    // The total number of bytes allocated
    size_t size;

    // The raw data array
    uint8_t * data;

    // TODO: Add tokenization data, parallel metadata, etc...
}
bytes_priv_t;

//------------------------------------------------------------------------|
static inline const uint8_t * bytes_data(bytes_t * bytes)
{
    return (const uint8_t *) ((bytes_priv_t *) bytes->priv)->data;
}

//------------------------------------------------------------------------|
static inline const char * bytes_cstr(bytes_t * bytes)
{
    return (const char *) ((bytes_priv_t *) bytes->priv)->data;
}

//------------------------------------------------------------------------|
static inline size_t bytes_size(bytes_t * bytes)
{
    return ((bytes_priv_t *) bytes->priv)->size;
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

    if (NULL != priv->data)
    {
        // TODO: Deal with compiler optimization problem
        memset(priv->data, 0, priv->size);
        free(priv->data);
    }

    memset(bytes->priv, 0, sizeof(bytes_priv_t));
}

//------------------------------------------------------------------------|
static void bytes_resize(bytes_t * bytes, size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Don't do anything if size doesn't change
    if (priv->size == size)
    {
        return;
    }

    // Use realloc to resize byte array
    priv->data = (uint8_t *) realloc(priv->data, size + 1);
    if (NULL == priv->data)
    {
        BLAMMO(ERROR, "malloc(%zu) failed\n", size + 1);
        return;
    }

    // Zero out the new memory
    if (size > priv->size)
    {
        memset(priv->data + priv->size, 0, size - priv->size);
    }

    // Update size and explicitly terminate buffer
    priv->size = size;
    priv->data[size] = 0;
    return;
}

//------------------------------------------------------------------------|
static int bytes_format(bytes_t * bytes, const char * format, ...)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    int nchars = 0;
    bool redo = false;
    va_list args;

    if (NULL == format)
    {
        BLAMMO(ERROR, "Format string is NULL");
        return -1;
    }

    // First try should be successful if there is enough room
    va_start (args, format);
    nchars = vsnprintf((char *) priv->data, priv->size, format, args);
    va_end (args);

    // Return early if error occurred
    if (nchars < 0)
    {
        BLAMMO(ERROR, "vsnprintf(%p, %zu, %s, ...) returned %d",
            priv->data, priv->size, format, nchars);
        return nchars;
    }

    // If vsnprintf returns larger than existing size, then another pass
    // is necessary.  Either way, we'll resize to either shrink down or
    // grow to fit.  If the two are the same then resize checks for that.
    redo = (nchars > priv->size); 
    bytes->resize(bytes, (size_t) nchars);

    if (redo)
    {
        // Second pass will pick up the full formatted buffer
        va_start (args, format);
        nchars = vsnprintf((char *) priv->data, priv->size, format, args);
        va_end (args);
    }    

    return nchars;
}

//------------------------------------------------------------------------|
static void bytes_assign(bytes_t * bytes, const char * str, size_t size)
{
	bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

	// TODO: Impose some reasonable size checks here?  get available free
	// memory?  Return bool failure/success?
	bytes->resize(bytes, size);

	// buffer was already terminated in resize
	memcpy(priv->data, str, size);
}

//------------------------------------------------------------------------|
static size_t bytes_trim(bytes_t * bytes)
{
    return 0;
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
void bytes_destroy(void * bytes_ptr)
{
    bytes_t * bytes = (bytes_t *) bytes_ptr;

    bytes->clear(bytes);
 
    if (NULL != bytes->priv)
    {
        free(bytes->priv);
    }

    if (NULL != bytes)
    {
        // TODO: Deal with compiler optimization problem
        memset(bytes, 0, sizeof(bytes_t));
        free(bytes);
    }
}

//------------------------------------------------------------------------|
static const bytes_t bytes_calls = {
    &bytes_create,
    &bytes_destroy,
    &bytes_data,
    &bytes_cstr,
    &bytes_size,
    &bytes_empty,
    &bytes_clear,
    &bytes_resize,
    &bytes_format,
    &bytes_assign,
    &bytes_trim,
    &bytes_copy,
    &bytes_split,
    &bytes_join,
    NULL
};

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
    memcpy(bytes, &bytes_calls, sizeof(bytes_t));

    // Allocate and initialize private implementation
    bytes->priv = malloc(sizeof(bytes_priv_t));
    if (!bytes->priv)
    {
        BLAMMO(ERROR, "malloc(sizeof(bytes_priv_t)) failed\n");
        free(bytes);
        return NULL;
    }

    memset(bytes->priv, 0, sizeof(bytes_priv_t));

    if ((NULL != str) && (size > 0))
    {
        bytes->assign(bytes, str, size);
    }

    return bytes;
}
