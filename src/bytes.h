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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
typedef struct bytes_t
{
    // Factory function that creates a 'bytes' object.
    struct bytes_t * (*create)(const char * str, size_t size);

    // bytes destructor function
    void (*destroy)(void * bytes);

    uint8_t * (*data)(struct bytes_t * bytes);
    char * (*cstr)(struct bytes_t * bytes);

    // Get the bytes's current length
    size_t (*length)(struct bytes_t * bytes);

    // Returns true if the bytes is empty and false otherwise
    bool (*empty)(struct bytes_t * bytes);

    bool (*fill)(struct bytes_t * bytes, const char c);

    // Effectively brings the bytes back to factory condition.
    void (*clear)(struct bytes_t * bytes);

    void (*append)(struct bytes_t * bytes, void * data);

    void (*resize)(struct bytes_t * bytes, size_t size);

    void (*shrink)(struct bytes_t * bytes);

    bool (*format)(struct bytes_t * bytes, const char * format, ...);

    size_t (*rtrim)(struct bytes_t * bytes);
    size_t (*ltrim)(struct bytes_t * bytes);

    struct bytes_t * (*copy)(struct bytes_t * bytes);

    struct bytes_t * (*split)(struct bytes_t * bytes, size_t begin, size_t end);

    bool (*join)(struct bytes_t * head, struct bytes_t * tail);

    // Private data
    void * priv;
}
bytes_t;

//------------------------------------------------------------------------|
// Public factory function that creates a new bytes object
bytes_t * bytes_create(const char * str, size_t size);

// Public destructor function that destroys a bytes object
void bytes_destroy(void * bytes);
