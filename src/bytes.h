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

    // Public bytes destructor function
    void (*destroy)(void * bytes);

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
    ssize_t (*format)(struct bytes_t * bytes, const char * format, ...);

    // Assign data directly to buffer, replacing any existing data,
    // and sizing the buffer as necessary.  strncpy equivalent.
    // TODO: how to handle whether the string needs to be un-escaped or not?
    // TODO: manual escape/un-escape calls for this
    void (*assign)(struct bytes_t * bytes, const void * data, size_t size);

    // Append data to end of buffer, growing as necessary.
    // strncat equivalent style arguments
    void (*append)(struct bytes_t * bytes, const void * data, size_t size);

    // Analogous to pread(), this will read arbitrary data from the string
    // at an offset.  Return value is number of bytes read or negative
    // if an error occurred.
    ssize_t (*read)(struct bytes_t * bytes, void * data, size_t count, size_t offset);

    // Analogous to pwrite(), this will write arbitrary data to the string
    // at an offset.  Return value is number of bytes read or negative
    // if an error occurred.  The size of the bytes object is unaltered.
    ssize_t (*write)(struct bytes_t * bytes, const void * data, size_t count, size_t offset);

    // TODO: Notional Functions
    /*
    bool (*fill)(struct bytes_t * bytes, const char c);
    void (*shrink)(struct bytes_t * bytes);
    size_t (*rtrim)(struct bytes_t * bytes);
    size_t (*ltrim)(struct bytes_t * bytes);
    const char * (*hexdump)(struct bytes_t * bytes)
    */

    // TODO: Stubbed Functions
    size_t (*trim)(struct bytes_t * bytes);
    struct bytes_t * (*copy)(struct bytes_t * bytes);
    struct bytes_t * (*split)(struct bytes_t * bytes, size_t begin, size_t end);
    bool (*join)(struct bytes_t * head, struct bytes_t * tail);

    // debug, serialization, etc... reorganize later
    const char * const (*hexdump)(struct bytes_t * bytes);

    // Private data
    void * priv;
}
bytes_t;

//------------------------------------------------------------------------|
// Public 'bytes' interface
extern const bytes_t bytes_pub;
