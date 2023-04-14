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

//-----------------------------------------------------------------------------+
#pragma once

//-----------------------------------------------------------------------------+
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
#ifndef MAX
#define MAX(a,b) ({ __typeof__ (a) _a = (a); \
                    __typeof__ (b) _b = (b); \
                    _a > _b ? _a : _b; })
#endif

#ifndef MIN
#define MIN(a,b) ({ __typeof__ (a) _a = (a); \
                    __typeof__ (b) _b = (b); \
                    _a < _b ? _a : _b; })
#endif

//-----------------------------------------------------------------------------+
// Generic comparator function signature.  Matches signature for qsort()'s
// 'compar' function pointer argument.  These are frequently used in objects
// for finding items within a chain or collection, or for sorting.
typedef int (*generic_compare_f)(const void *, const void *);

// Generic object/data copy function signature.  Similar to strdup(), but these
// are expected to return a heap-allocated deep-copy of the passed-in object.
// The type must be known by the implementer.  These are used within other
// implementations of object copy instances (sometimes recursively) to copy
// complex objects.
typedef void * (*generic_copy_f)(const void *);

// Generic object/data destructor signature.  Matches free() so that these can
// be used interchangeably depending on the type of data or object being
// deallocated.  These are used all over the place for garbage collection.
typedef void (*generic_destroy_f)(void *);

//-----------------------------------------------------------------------------+
void hexdump(const void * buf, size_t len, size_t addr);

// Convert a string to boolean.  Things like "true" and "false", '0' and '1',
// "on" and "off" should be supported by this.
bool str_to_bool(const char * str);

// https://www.cryptologie.net/article/419/zeroing-memory-compiler-optimizations-and-memset_s/
