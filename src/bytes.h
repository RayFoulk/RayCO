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

#pragma once

#include <sys/types.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
typedef struct bytes_t
{
    // Factory function that creates a 'bytes' object.
    struct bytes_t * (*create)(const void * data, size_t size);

    // Alternative printf-style factory function
    struct bytes_t * (*print_create)(const char * format, ...);

    // Bytes destructor function
    void (*destroy)(void * bytes);

    // Comparator that intentionally has the same signature
    // as qsort()'s 'compar' argument, for that purpose.
    int (*compare)(const void * bytes, const void * other);

    // Comparator that finds the index of the first byte that is
    // different between the two buffers, searching from the beginning
    // (offset 0) up to the end of the shortest byte buffer.
    // Returns the index of the first different byte or negative
    // value if the buffers are identical (size and content)
    ssize_t (*diff_byte)(const struct bytes_t * bytes,
                         const struct bytes_t * other);

    // Get the data as a byte array pointer
    const uint8_t * (*data)(const struct bytes_t * bytes);

    // Get the data as a C string
    const char * (*cstr)(const struct bytes_t * bytes);

    // Get the bytes's current length
    size_t (*size)(const struct bytes_t * bytes);

    // Returns true if the bytes is empty and false otherwise
    bool (*empty)(const struct bytes_t * bytes);

    // Effectively brings the bytes back to factory condition.
    void (*clear)(struct bytes_t * bytes);

    // Resize the buffer, keeping existing data intact
    void (*resize)(struct bytes_t * bytes, size_t size);

    // vprintf()-style string formatter.  resizes as necessary.
    // Does not call va_start or va_end!!!
    ssize_t (*vprint)(struct bytes_t * bytes, const char * format, va_list args);

    // printf-style string formatter.  resizes as necessary
    ssize_t (*print)(struct bytes_t * bytes, const char * format, ...);

    // Assign data directly to buffer, replacing any existing data,
    // and sizing the buffer as necessary.  strncpy/memcpy analog.
    void (*assign)(struct bytes_t * bytes,
                   const void * data,
                   size_t size);

    // Append raw data to end of buffer, growing as necessary.
    void (*append)(struct bytes_t * bytes,
                   const void * data,
                   size_t size);

    // Analogous to pread(), this will read arbitrary data from the string
    // at an offset.  Return value is number of bytes read or negative
    // if an error occurred.
    ssize_t (*read_at)(struct bytes_t * bytes,
                       void * data,
                       size_t count,
                       size_t offset);

    // Analogous to pwrite(), this will write arbitrary data to the string
    // at an offset.  Return value is number of bytes read or negative
    // if an error occurred.  The size of the bytes object is unaltered.
    ssize_t (*write_at)(struct bytes_t * bytes,
                        const void * data,
                        size_t count,
                        size_t offset);

    // Trim whitespace as defined by caller from the right end, left
    // end, and both ends of the byte buffer.  Returns new size.
    size_t (*trim_left)(struct bytes_t * bytes, const char * whitespace);
    size_t (*trim_right)(struct bytes_t * bytes, const char * whitespace);
    size_t (*trim)(struct bytes_t * bytes, const char * whitespace);

    // Find instance(s) of subsequence, searching from left or right,
    // negative return indicates not found.  length of match is same as substring
    // TODO: consider find_all by re-using token array
    ssize_t (*find_forward)(struct bytes_t * bytes,
                            size_t start_offset,
                            const void * data,
                            size_t size);

    ssize_t (*find_reverse)(struct bytes_t * bytes,
                            size_t start_offset,
                            const void * data,
                            size_t size);

    // Fill the buffer completely with a given character
    void (*fill)(struct bytes_t * bytes, const char c);

    // Create a copy of the given bytes object.
    // Caller is responsible for destroying the copy.
    void * (*copy)(const void * bytes);

    // Split byte array into tokenized string array by delimiters.
    // The bytes array is altered in-place, having null bytes inserted.
    // The first token that begins with 'ignore' truncates the list.
    // NULL may be passed for ignore to get all tokens.
//    char ** (*tokenize)(struct bytes_t * bytes,
//                        const char * delim,
//                        const char * ignore,
//                        size_t * numtokens);

    // Get token markers without inserting null terminators or
    // altering the backing data buffer in any way.
//    char ** (*marktokens)(struct bytes_t * bytes,
//                          const char * delim,
//                          const char * ignore,
//                          size_t * numtokens);

    // Refactored tokenizer that takes into account quoted strings
    // and parenthetical expressions.  Can be operated to either
    // insert null terminators or not, as needed.  Quotes and
    // Parenthesis are generalized here as "Encapsulation Pairs"
    // consisting of a "Begin" and "End" character,
    // like so: { "\"\"", "()", NULL }.  The encaps array is expected
    // to be NULL pointer terminated.  Quotes are really a special case
    // of an encapsulation pair where the beginning and end characters
    // are the same.  The 'split' flag indicates whether to insert
    // null terminators or not.
    char ** (*tokenizer)(struct bytes_t * bytes,
                         bool split,
                         const char ** encaps,
                         const char * delim,
                         const char * ignore,
                         size_t * numtokens);

    // Given an absolute pointer into the data, get the relative offset
    // Returns negative value if there is an error
    ssize_t (*offset)(struct bytes_t * bytes, void * ptr);

    // Remove an arbitrary chunk of data, moving higher data
    // down to fill the gap, and resizing down.  Returns new
    // size or else negative if error occurs.
    ssize_t (*remove)(struct bytes_t * bytes, size_t begin, size_t size);

    // Insert new data at an arbitrary offset.  Sparseness is not
    // supported.  This will move higher data up, and resize up.
    //  Returns new size or else negative if error occurs.
    ssize_t (*insert)(struct bytes_t * bytes,
            size_t offset,
            const void * data,
            size_t size);

    // debug, serialization, etc... reorganize later
    const char * const (*hexdump)(struct bytes_t * bytes);

    // Private data
    void * priv;
}
bytes_t;

//------------------------------------------------------------------------|
// Public 'bytes' interface
extern const bytes_t bytes_pub;

//------------------------------------------------------------------------|
//    ssize_t (*find_all)(struct bytes_t * bytes,
//                        const void * data,
//                        size_t size,
//                        size_t * offsets,
//                        size_t * max_offsets);
// Else consider returning a list.
// TODO: Notional Functions
// (*fill_cyclic) // for VR purposes
// TODO: how to handle whether the string needs to be un-escaped or not?
// TODO: manual escape/un-escape calls for this
//(*escape/encode) (*unescape/decode)
//find/insert/replace/remove
//void (*shrink)(struct bytes_t * bytes); ???
