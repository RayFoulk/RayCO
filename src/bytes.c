//------------------------------------------------------------------------|
// Copyright (c) 2018-2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
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
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>             // strchr(), others...
#include <stddef.h>

#include "bytes.h"
#include "blammo.h"
#include "utils.h"              // memzero(), function signatures

//------------------------------------------------------------------------|
// bytes private implementation data
typedef struct
{
    // The total number of bytes allocated
    size_t size;

    // The raw data array
    uint8_t * data;

    // Dynamically sized token pointer array. Used with tokenize/mark
    // number of token pointer elements in array
    char ** tokens;
    size_t maxtokens;

    // A report buffer used for hexdump, debugging, tokens? etc...
    // This is only used for certain calls, but otherwise left NULL.
    // it will be re-purposed as necessary and destroyed when the main
    // object is destroyed.
    bytes_t * buffer;
}
bytes_priv_t;

//------------------------------------------------------------------------|
static bytes_t * bytes_create(const void * data, size_t size)
{
    // Allocate and initialize public interface
    bytes_t * bytes = (bytes_t *) malloc(sizeof(bytes_t));
    if (!bytes)
    {
        BLAMMO(FATAL, "malloc(sizeof(bytes_t)) failed\n");
        return NULL;
    }

    // bulk copy all function pointers and init opaque ptr
    memcpy(bytes, &bytes_pub, sizeof(bytes_t));

    // Allocate and initialize private implementation
    bytes->priv = malloc(sizeof(bytes_priv_t));
    if (!bytes->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(bytes_priv_t)) failed\n");
        free(bytes);
        return NULL;
    }

    memzero(bytes->priv, sizeof(bytes_priv_t));
    bytes->assign(bytes, data, size);
    return bytes;
}

//------------------------------------------------------------------------|
static bytes_t * bytes_print_create(const char * format, ...)
{
    bytes_t * bytes = bytes_create(NULL, 0);
    if (!bytes)
    {
        BLAMMO(ERROR, "bytes->create() failed\n");
        return NULL;
    }

    va_list args;
    ssize_t nchars = 0;

    va_start (args, format);
    nchars = bytes->vprint(bytes, format, args);
    va_end (args);

    if (nchars < 0)
    {
        BLAMMO(ERROR, "bytes->vprint() returned %d", nchars);
        bytes->destroy(bytes);
        return NULL;
    }

    return bytes;
}

//------------------------------------------------------------------------|
static void bytes_destroy(void * bytes_ptr)
{
    bytes_t * bytes = (bytes_t *) bytes_ptr;

    bytes->clear(bytes);

    if (bytes->priv)
    {
        memzero(bytes->priv, sizeof(bytes_priv_t));
        free(bytes->priv);
    }

    if (bytes)
    {
        memzero(bytes, sizeof(bytes_t));
        free(bytes);
    }
}

//------------------------------------------------------------------------|
static int bytes_compare(const void * bytes, const void * other)
{
    bytes_t * a = (bytes_t *) bytes;
    bytes_t * b = (bytes_t *) other;

    // Sizes much first match to be equal.
    if (a->size(a) != b->size(b))
    {
        return a->size(a) - b->size(b);
    }

    // Sizes match, compare content
    return memcmp(a->data(a), b->data(b), a->size(a));
}

//------------------------------------------------------------------------|
ssize_t bytes_diff_byte(const bytes_t * bytes, const bytes_t * other)
{
    size_t offset = 0;
    size_t smallest = MIN(bytes->size(bytes), other->size(other));

    if (bytes->empty(bytes) || other->empty(other))
    {
        BLAMMO(WARNING, "empty data in comparison");
        return 0;
    }

    while ((bytes->data(bytes)[offset] == other->data(other)[offset])
           && (offset < smallest))
    {
        offset++;
    }

    // Check if no difference was found
    if (bytes->size(bytes) == other->size(other)
        && offset == smallest)
    {
        BLAMMO(VERBOSE, "buffers are identical");
        return -1;
    }

    // Either there was a different byte found, or the buffers are
    // different sizes, and therefore necessarily contain different data.
    return (ssize_t) offset;
}

//------------------------------------------------------------------------|
static inline const uint8_t * bytes_data(const bytes_t * bytes)
{
    return (const uint8_t *) ((bytes_priv_t *) bytes->priv)->data;
}

//------------------------------------------------------------------------|
static inline const char * bytes_cstr(const bytes_t * bytes)
{
    return (const char *) ((bytes_priv_t *) bytes->priv)->data;
}

//------------------------------------------------------------------------|
static inline size_t bytes_size(const bytes_t * bytes)
{
    return ((bytes_priv_t *) bytes->priv)->size;
}

//------------------------------------------------------------------------|
static inline bool bytes_empty(const bytes_t * bytes)
{
    return (NULL == ((bytes_priv_t *) bytes->priv)->data);
}

//------------------------------------------------------------------------|
static void bytes_clear(bytes_t * bytes)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Also destroy the report/working buffer
    if (priv->buffer)
    {
        priv->buffer->destroy(priv->buffer);
        priv->buffer = NULL;
    }

    // Destroy any token pointers if allocated
    if (priv->tokens)
    {
        memzero(priv->tokens, sizeof(char *) * priv->maxtokens);
        free(priv->tokens);
    }

    // Destroy the actual byte array
    if (priv->data)
    {
        memzero(priv->data, priv->size);
        free(priv->data);
    }

    memzero(bytes->priv, sizeof(bytes_priv_t));
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
    if (!priv->data)
    {
        BLAMMO(FATAL, "realloc(%zu) failed\n", size + 1);
        return;
    }

    // Zero out the new memory
    if (size > priv->size)
    {
        memzero(priv->data + priv->size, size - priv->size);
    }

    // Update size and explicitly terminate buffer
    priv->size = size;
    priv->data[size] = 0;
    return;
}

//------------------------------------------------------------------------|
ssize_t bytes_vprint(bytes_t * bytes, const char * format, va_list args)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    ssize_t nchars = 0;
    bool redo = false;
    va_list args_copy;

    // Guard-block against NULL format string
    if (!format)
    {
        BLAMMO(ERROR, "Format string is NULL");
        return -1;
    }

    // Need to copy the arg list due to the nature of this potentially
    // 2-pass approach to sizing the string.  Since this is a "vprint"
    // style function, it is not allowed to call va_start or va_end on
    // the original va_list.  We'll use the copy to determine size, and
    // if a second pass is necessary: use the original
    va_copy(args_copy, args);

    // First pass will be successful if the current size is large enough.
    // It is OK (and should be done) to call va_end() on the copy.
    nchars = vsnprintf((char *) priv->data, priv->size, format, args_copy);
    va_end (args_copy);

    // Return early if error occurred
    if (nchars < 0)
    {
        BLAMMO(ERROR, "vsnprintf(%p, %zu, %s, va_list) returned %d",
            priv->data, priv->size, format, nchars);
        return nchars;
    }

    // If vsnprintf returns larger than (OR EQUAL TO) existing size, then another
    // pass is necessary.  Either way, we'll resize to either shrink down or
    // grow to fit.  If the two are the same then resize checks for that.
    // FIX: vsnprintf() apparently returns length including null terminator.
    // This leads to all sorts of weird behavior when appending and trimming.
    // This is why a redo is necessary when nchars == size.
    redo = (nchars >= priv->size);
    bytes->resize(bytes, (size_t) nchars);

    if (redo)
    {
        // Second pass will pick up the full formatted buffer
        nchars = vsnprintf((char *) priv->data, priv->size + 1, format, args);
    }

    return nchars;
}

//------------------------------------------------------------------------|
static ssize_t bytes_print(bytes_t * bytes, const char * format, ...)
{
    va_list args;
    ssize_t nchars = 0;

    va_start (args, format);
    nchars = bytes->vprint(bytes, format, args);
    va_end (args);

    return nchars;
}

//------------------------------------------------------------------------|
static void bytes_assign(bytes_t * bytes, const void * data, size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Always size the memory as requested.  There is always an
    // extra hidden null byte at the end for as-string use cases.
    bytes->resize(bytes, size);

    // buffer was already terminated in resize.  Assign data if
    // any was provided.  USE CAUTION that payload >= size.
    if (data)
    {
        memcpy(priv->data, data, size);
    }
}

//------------------------------------------------------------------------|
static void bytes_append(bytes_t * bytes, const void * data, size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    size_t prev_size = priv->size;

    // Simply add data to end, resizing up.  Note this is functionally
    // the same as insert() at the end.
    bytes->resize(bytes, priv->size + size);

    // buffer was already terminated in resize
    memcpy(priv->data + prev_size, data, size);
}

//------------------------------------------------------------------------|
static ssize_t bytes_read_at(struct bytes_t * bytes,
                             void * data,
                             size_t count,
                             size_t offset)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Both count and offset are unsigned, so the only case we have to
    // check is the upper bound.  The lower bound may or may not be in
    // range, but if the upper bound is, then the lower one will be also.
    if (offset + count > priv->size)
    {
        BLAMMO(ERROR, "offset %u + count %u is out of bounds for size %u",
                      offset, count, priv->size);
        return -1;
    }

    memcpy(data, priv->data + offset, count);
    return (ssize_t) count;
}

//------------------------------------------------------------------------|
static ssize_t bytes_write_at(struct bytes_t * bytes,
                              const void * data,
                              size_t count,
                              size_t offset)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Both count and offset are unsigned, so the only case we have to
    // check is the upper bound.  The lower bound may or may not be in
    // range, but if the upper bound is, then the lower one will be also.
    if (offset + count > priv->size)
    {
        BLAMMO(ERROR, "offset %u + count %u is out of bounds for size %u",
                      offset, count, priv->size);
        return -1;
    }

    memcpy(priv->data + offset, data, count);
    return (ssize_t) count;
}

//------------------------------------------------------------------------|
static size_t bytes_trim_left(bytes_t * bytes, const char * whitespace)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    size_t index = 0;

    // search from the left end forwards and do the same
    // thing as trim_right, only now a memmov will be required
    while (index < priv->size && strchr(whitespace, priv->data[index]))
    {
        index++;
    }

    memmove(priv->data, priv->data + index, priv->size - index);
    bytes->resize(bytes, priv->size - index);
    return priv->size;
}

//------------------------------------------------------------------------|
static size_t bytes_trim_right(bytes_t * bytes, const char * whitespace)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    size_t index = priv->size - 1;

    // search from right end backwards checking for any whitespace char
    // and stop at the first character where there is no match.
    while (index >= 0 && strchr(whitespace, priv->data[index]))
    {
        index--;
    }

    // +1 for post-loop correction and another for difference
    // between final index and actual size.
    // resize() already accounts for if there is no change
    bytes->resize(bytes, index + 1);
    return priv->size;
}

//------------------------------------------------------------------------|
static size_t bytes_trim(bytes_t * bytes, const char * whitespace)
{
    bytes->trim_right(bytes, whitespace);
    return bytes->trim_left(bytes, whitespace);
}

//------------------------------------------------------------------------|
static ssize_t bytes_find_forward(struct bytes_t * bytes,
                                  size_t start_offset,
                                  const void * data,
                                  size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    ssize_t offset = (ssize_t) start_offset;

    // Return on first match
    while(offset + size <= priv->size)
    {
        if (!memcmp(priv->data + offset, data, size))
        {
            return offset;
        }

        offset++;
    }

    // No match found
    return -1;
}

//------------------------------------------------------------------------|
static ssize_t bytes_find_reverse(struct bytes_t * bytes,
                                  size_t start_offset,
                                  const void * data,
                                  size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    if (start_offset > priv->size)
    {
        BLAMMO(WARNING, "start_offset %zu is larger than size %zu",
                        start_offset, priv->size);
        start_offset = priv->size;
    }

    ssize_t offset = start_offset - size;

    // Return on first match
    while(offset >= 0)
    {
        if (!memcmp(priv->data + offset, data, size))
        {
            return offset;
        }

        offset--;
    }

    // No match found
    return -1;
}

//------------------------------------------------------------------------|
static inline void bytes_fill(bytes_t * bytes, const char c)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Size might be zero, in which case data is NULL
    if (priv->data)
    {
        memset(priv->data, c, priv->size);
        priv->data[priv->size] = 0;
    }
}

//------------------------------------------------------------------------|
static void * bytes_copy(const void * bytes_ptr)
{
    // This should be an exact copy of the bytes object
    // except for any differences in the state of the report string.
    // The caller of copy() assumes responsibility for destroying the copy
    bytes_t * bytes = (bytes_t *) bytes_ptr;
    bytes_t * copy = bytes->create(bytes->data(bytes),
                                   bytes->size(bytes));

    return (void *) copy;
}

//------------------------------------------------------------------------|
// Private helper function for dynamically sizing token array
static void bytes_resize_tokens(bytes_t * bytes,
                                size_t numtokens,
                                size_t maxtokens)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    priv->tokens = (char **) realloc(priv->tokens,
                                     sizeof(char *) * maxtokens);
    if (!priv->tokens)
    {
        BLAMMO(FATAL, "realloc(%zu) failed", sizeof(char *) * maxtokens);
        return;
    }

    priv->maxtokens = maxtokens;
    memzero(&priv->tokens[numtokens],
            sizeof(char *) * (maxtokens - numtokens));

    BLAMMO(DEBUG, "resized maxtokens: %zu", priv->maxtokens);
}

//------------------------------------------------------------------------|
static char ** bytes_tokenize(bytes_t * bytes,
                              const char * delim,
                              const char * ignore,
                              size_t * numtokens)
{
    // Do not attempt to tokenize empty bytes
    if (bytes->empty(bytes))
    {
        BLAMMO(VERBOSE, "Not tokenizing empty bytes!");
        return NULL;
    }

    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // The theoretical maximum number of tokens in a string is
    // probably something like 1/2 strlen: if every single token was 1
    // char delimited by 1 space.  It certainly cannot be higher than
    // strlen itself.  Assuming the typical keyword is probably 3 or 4
    // chars long, a reasonable guess is 1/4 strlen.  Add some extra
    // extra padding onto that, say 2 elements.  Then the plan is to
    // double this size whenever we run out of room.
    size_t maxtokens = 2 + (priv->size >> 2);
    bytes_resize_tokens(bytes, 0, maxtokens);

    // Proceed with tokenization
    char * saveptr = NULL;
    char * ptr = strtok_r((char *) bytes->cstr(bytes), delim, &saveptr);

    *numtokens = 0;
    while (ptr != NULL)
    {
        BLAMMO(DEBUG, "ptr: %s", ptr);

        if (!strncmp(ptr, ignore, strlen(ignore)))
        {
            BLAMMO(DEBUG, "ignoring: %s", ptr);
            break;
        }

        priv->tokens[(*numtokens)++] = ptr;
        ptr = strtok_r(NULL, delim, &saveptr);

        // Resize up if necessary, zeroing new memory
        if (*numtokens >= priv->maxtokens)
        {
            maxtokens <<= 1;
            bytes_resize_tokens(bytes, *numtokens, maxtokens);
        }
    }

    return priv->tokens;
}

//------------------------------------------------------------------------|
static char ** bytes_marktokens(bytes_t * bytes,
                                const char * delim,
                                const char * ignore,
                                size_t * numtokens)
{
    // Do not attempt to tokenize empty bytes
    if (bytes->empty(bytes))
    {
        BLAMMO(VERBOSE, "Ignoring empty bytes!");
        return NULL;
    }

    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    size_t maxtokens = 2 + (priv->size >> 2);
    bytes_resize_tokens(bytes, 0, maxtokens);

    bool within_delim = true;
    char * ptr = (char *) priv->data;

    *numtokens = 0;
    while (*ptr != 0)
    {
        if (strchr(delim, *ptr))
        {
            if (!within_delim)
            {
                // transition to whitespace at the end of a token
                BLAMMO(VERBOSE, "end of token at \'%s\'", ptr);
            }

            within_delim = true;
        }
        else
        {
            if (within_delim)
            {
                // transition to token at the end of whitespace
                BLAMMO(VERBOSE, "beginning of token at \'%s\'", ptr);
                priv->tokens[(*numtokens)++] = ptr;
            }

            within_delim = false;
        }

        // Resize up if necessary, zeroing new memory
        if (*numtokens >= priv->maxtokens)
        {
            maxtokens <<= 1;
            bytes_resize_tokens(bytes, *numtokens, maxtokens);
        }

        ptr++;
    }

    BLAMMO(VERBOSE, "number of markers: %u", *numtokens);

    return priv->tokens;
}

//------------------------------------------------------------------------|
// Private helper function for bytes_next_token().  This is essentially
// a super-strcspn() that accounts for encapsulated tokens such as quoted
// strings and parenthetical expressions.  The encapsulation charaters are
// included in the spanned token.
static size_t bytes_encaps_span(const char * str, const char * encaps)
{
    // Nothing to check here if end of string or if not encapsulated.
    if (!*str || (*str != encaps[0]))
    {
        return 0;
    }

    // The *str must necessarily be encaps[0] to get to this point,
    // so advance past it and start at nest level 1.
    char * ptr = (char *) str + 1;
    int nest = 1;

    // Done either at end-of-string or nest level gets back to zero
    while (*ptr && nest)
    {
        // Have to check for end-encaps (encaps[1]) first to cover
        // special case of quotes, where end char == begin char.
        if (*ptr == encaps[1])
        {
            nest--;
        }
        else if (*ptr == encaps[0])
        {
            nest++;
        }

        ptr++;
    }

    // Check for unterminated token.  I.E. missing parenthesis, or
    // no end-quote, for example.
    if (nest)
    {
        BLAMMO(WARNING, "Expected \'%c\' at nest level %d", encaps[1], nest);
    }

    return (size_t) (ptr - str);
}

//------------------------------------------------------------------------|
// Private helper function for tokenizer().  This is essentially a super-
// glorified strtok_r() except that it handles encapsulated tokens
// and optionally does or does not insert null terminators, as requested.
static char * bytes_next_token(bytes_t * bytes,
                               bool split,
                               const char ** encaps,
                               const char * delim,
                               const char * ignore,
                               char ** saveptr)
{
    char * end;
    char * str = NULL;
    size_t span = 0;
    int i = 0;

    // Set the current pointer within the buffer
    if (bytes)
    {
        str = (char *) bytes->cstr(bytes);
    }
    else
    {
        str = *saveptr;
    }

    // Check if we're done early, ignore inline comments
    if ((*str == '\0') ||
       (ignore && !strncmp(str, ignore, strlen(ignore))))
    {
        *saveptr = str;
        return NULL;
    }

    // Scan for leading delimiters, move up to token beginning
    str += strspn(str, delim);
    if ((*str == '\0') ||
       (ignore && !strncmp(str, ignore, strlen(ignore))))
    {
        *saveptr = str;
        return NULL;
    }

    // Check for encapsulated tokens.  Regular delimiters are
    // allowed to occur within these, such as quoted strings
    // or parenthetical expressions.  First match wins: I.E.
    // consider "(parens-inside)" versus ("quotes-inside")
    while (encaps && encaps[i])
    {
        span = bytes_encaps_span(str, encaps[i]);
        if (span > 0)
        {
            end = str + span;
            break;
        }

        i++;
    }

    // If span is zero at this point then no encapsulated token,
    // so do things as regular-ol' strtok_r() would in that case.
    if (span == 0)
    {
        // Find where the token ends
        end = str + strcspn(str, delim);
        if (*end == '\0')
        {
            *saveptr = end;
            return str;
        }
    }

    // Terminate the token (if requested) and make *saveptr point past it.
    if (split) { *end = '\0'; }
    *saveptr = end + 1;
    return str;
}

//------------------------------------------------------------------------|
static char ** bytes_tokenizer(bytes_t * bytes,
                               bool split,
                               const char ** encaps,
                               const char * delim,
                               const char * ignore,
                               size_t * numtokens)
{
    // Do not attempt to tokenize empty bytes
    if (bytes->empty(bytes))
    {
        BLAMMO(VERBOSE, "Not tokenizing empty bytes!");
        return NULL;
    }

    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // The theoretical maximum number of tokens in a string is
    // probably something like 1/2 strlen: if every single token was 1
    // char delimited by 1 space.  It certainly cannot be higher than
    // strlen itself.  Assuming the typical keyword is probably 3 or 4
    // chars long, a reasonable guess is 1/4 strlen.  Add some extra
    // extra padding onto that, say 2 elements.  Then the plan is to
    // double this size whenever we run out of room.
    size_t maxtokens = 2 + (priv->size >> 2);
    bytes_resize_tokens(bytes, 0, maxtokens);

    // Proceed with tokenization
    char * saveptr = NULL;
    char * ptr = bytes_next_token(bytes, split, encaps, delim, ignore, &saveptr);

    *numtokens = 0;
    while (ptr != NULL)
    {
        BLAMMO(DEBUG, "ptr: %s", ptr);


        priv->tokens[(*numtokens)++] = ptr;
        ptr = bytes_next_token(NULL, split, encaps, delim, ignore, &saveptr);

        // Resize up if necessary, zeroing new memory
        if (*numtokens >= priv->maxtokens)
        {
            maxtokens <<= 1;
            bytes_resize_tokens(bytes, *numtokens, maxtokens);
        }
    }

    return priv->tokens;
}

//------------------------------------------------------------------------|
static ssize_t bytes_offset(bytes_t * bytes, void * ptr)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    uint8_t * bptr = (uint8_t *) ptr;

    if (bptr < priv->data)
    {
        BLAMMO(ERROR, "Pointer is before data in memory");
        return -1;
    }
    else if (bptr > (priv->data + priv->size))
    {
        BLAMMO(ERROR, "Pointer is after data in memory");
        return -2;
    }

    return (ssize_t) (bptr - priv->data);
}

//------------------------------------------------------------------------|
static ssize_t bytes_remove(bytes_t * bytes, size_t begin, size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Check boundaries on all offsets
    if (begin > (priv->size - 1))
    {
        BLAMMO(ERROR, "begin %zu is after data final offset %zu",
                      begin, priv->size - 1);
        return -1;
    }
    else if ((begin + size) > priv->size)
    {
        BLAMMO(ERROR, "begin + size %zu is after data size %zu",
                      begin + size, priv->size);
        return -2;
    }

    // Fair game, just move the data down and truncate
    memmove(priv->data + begin,
            priv->data + begin + size,
            priv->size - begin - size);

    bytes->resize(bytes, priv->size - size);

    return (ssize_t) priv->size;
}

//------------------------------------------------------------------------|
static ssize_t bytes_insert(bytes_t * bytes,
                            size_t offset,
                            const void * data,
                            size_t size)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;

    // Check boundaries on offsets.  do not allow sparse inserts
    if (offset > priv->size)
    {
        BLAMMO(ERROR, "offset %zu is after data size %zu",
                      offset, priv->size);
        return -1;
    }

    // Size up to hold new data
    size_t oldsize = priv->size;
    bytes->resize(bytes, priv->size + size);

    // Move higher data up
    memmove(priv->data + offset + size,
            priv->data + offset,
            oldsize - offset);

    // Copy in new data
    memcpy(priv->data + offset, data, size);

    return (ssize_t) priv->size;
}

//------------------------------------------------------------------------|
// hexstr char buffer is expected to be minimum size 2
static inline void hexdigit(char * hexstr, uint8_t byte)
{
    uint8_t nibble = byte >> 4;
    hexstr[0] = nibble < 0x0A ? nibble + 0x30 : nibble + 0x37;
    nibble = byte & 0x0F;
    hexstr[1] = nibble < 0x0A ? nibble + 0x30 : nibble + 0x37;
}

//------------------------------------------------------------------------|
// hexstr char buffer is expected to be minimum size 16.
// This routine is hard-coded to represent the address with a minimum
// of 4 hex digits (minbytes is 2).  Values larger than that will add
// significant figures to the left, whereas values less that 0xFFFF will
// have leading zeroes.
static inline size_t hexaddr(char * hexaddr, size_t addr)
{
    const size_t minbytes = 2;
    size_t remain = sizeof(size_t);
    size_t mask = (size_t) 0x00FF << ((remain * 8) - 8);
    size_t posn = 0;

    // Fast-forward to either where significant bits are, or
    // else stop where we still have minimum bytes left.
    while (((addr & mask) == 0) && (remain > minbytes))
    {
        mask >>= 8;
        remain--;
    }

    // start converting address data into hex representation
    while (remain > 0)
    {
        hexdigit(hexaddr + posn, (addr >> ((remain * 8) - 8)) & 0xFF);
        posn += 2;
        remain--;
    }

    // Add some spacing and terminate
    hexaddr[posn++] = ' ';
    hexaddr[posn++] = ' ';
    hexaddr[posn] = '\0';
    return posn;
}

//------------------------------------------------------------------------|
// TODO: merge this with the hexdump in utils
static const char * const bytes_hexdump(bytes_t * bytes)
{
    bytes_priv_t * priv = (bytes_priv_t *) bytes->priv;
    char hexoffs[18] = { 0x00 };
    char hexchar[4] = { 0x20, 0x20, 0x20, 0x00 };
    char ascii[17] = { 0x00 };
    size_t i, j;

    // Allocate a working buffer for the hexdump.  The size calculation
    // is an approximation here, and really only serves to ensure that
    // we can allocate enough contiguous memory early.  Consider
    if (NULL == priv->buffer)
    {
        priv->buffer = bytes_create("", 0); //6 + bytes->size(bytes) * 4);
    }

    // Clear working buffer and iterate through all data bytes
    priv->buffer->clear(priv->buffer);
    for (i = 0; i < priv->size; i++)
    {
        // Address prefix
        if ((i % 16) == 0)
        {
            j = hexaddr(hexoffs, i);
            priv->buffer->append(priv->buffer, hexoffs, j);
        }

        // Intentionally avoiding expensive realloc()/vsnprintf() here
        hexdigit(hexchar, priv->data[i]);
        priv->buffer->append(priv->buffer, hexchar, 3);

        // Intentionally avoiding isprint() here
        if ((priv->data[i] >= ' ') && (priv->data[i] <= '~'))
        {
            ascii[i % 16] = priv->data[i];
        }
        else
        {
            ascii[i % 16] = '.';
        }

        if ((i + 1) % 8 == 0 || i + 1 == priv->size)
        {
            priv->buffer->append(priv->buffer, " ", 1);
            if ((i + 1) % 16 == 0)
            {
                priv->buffer->append(priv->buffer, ascii, sizeof(ascii) - 1);
                priv->buffer->append(priv->buffer, "\n", 1);
            }
            else if (i + 1 == priv->size)
            {
                ascii[(i + 1) % 16] = '\0';

                if ((i + 1) % 16 <= 8)
                {
                    priv->buffer->append(priv->buffer, " ", 1);
                }

                for (j = (i + 1) % 16; j < 16; j++)
                {
                    priv->buffer->append(priv->buffer, "   ", 3);
                }

                priv->buffer->append(priv->buffer, ascii, sizeof(ascii) - 1);
                priv->buffer->append(priv->buffer, "\n", 1);
            }
        }
    }

    return priv->buffer->cstr(priv->buffer);
}

//------------------------------------------------------------------------|
const bytes_t bytes_pub = {
    &bytes_create,
    &bytes_print_create,
    &bytes_destroy,
    &bytes_compare,
    &bytes_diff_byte,
    &bytes_data,
    &bytes_cstr,
    &bytes_size,
    &bytes_empty,
    &bytes_clear,
    &bytes_resize,
    &bytes_vprint,
    &bytes_print,
    &bytes_assign,
    &bytes_append,
    &bytes_read_at,
    &bytes_write_at,
    &bytes_trim_left,
    &bytes_trim_right,
    &bytes_trim,
    &bytes_find_forward,
    &bytes_find_reverse,
    &bytes_fill,
    &bytes_copy,
    &bytes_tokenize,
    &bytes_marktokens,
    &bytes_tokenizer,
    &bytes_offset,
    &bytes_remove,
    &bytes_insert,
    &bytes_hexdump,
    NULL
};
