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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
typedef struct bytes_t
{
    // Factory function that creates a 'bytes' object.
    struct bytes_t * (*create)(const void * data, size_t size);

    // Bytes destructor function
    void (*destroy)(void * bytes);

    // Comparator that intentionally has the same signature
    // as qsort()'s 'compar' argument, for that purpose.
    int (*compare)(const void * bytes, const void * other);

    // Get the data as a byte array pointer
    const uint8_t * (*data)(struct bytes_t * bytes);

    // Get the data as a C string
    const char * (*cstr)(struct bytes_t * bytes);

    // Get the bytes's current length
    size_t (*size)(struct bytes_t * bytes);

    // Returns true if the bytes is empty and false otherwise
    bool (*empty)(struct bytes_t * bytes);

    // Effectively brings the bytes back to factory condition.
    void (*clear)(struct bytes_t * bytes);

    // Resize the buffer, keeping existing data intact
    void (*resize)(struct bytes_t * bytes, size_t size);

    // Printf-style string formatter
    ssize_t (*print)(struct bytes_t * bytes, const char * format, ...);
    // TODO: vprint() and then use it in console->reprint()/print()

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
    // or else all occuring throughout the whole sequence.  negative
    // return indicates not found.  length of match is same as substring
    ssize_t (*find_left)(struct bytes_t * bytes,
                         const void * data,
                         size_t size);
    ssize_t (*find_right)(struct bytes_t * bytes,
                          const void * data,
                          size_t size);

    // Fill the buffer completely with a given character
    void (*fill)(struct bytes_t * bytes, const char c);

    // Create a copy of the given bytes object.
    // Caller is responsible for destroying the copy.
    struct bytes_t * (*copy)(struct bytes_t * bytes);

    // Split byte array into tokenized string list by delimiters.
    // Caller is responsible for destroying returned list.
    // The bytes array is altered in-place, having null bytes inserted.
//    chain_t * (*split)(struct bytes_t * bytes,
//                       const char * delim,
//                       const char * ignore);
    int * (*split)(struct bytes_t * bytes,
                   char ** tokens,
                   const char * delim,
                   const char * ignore);

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
