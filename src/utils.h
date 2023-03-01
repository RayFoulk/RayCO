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
void hexdump(const void * buf, size_t len, size_t addr);

// Pass in caller-managed pointer array and its size.  THe string to be
// tokenized MUST be mutable otherwise this will segfault.  Use strdup()
// beforehand if you have to.  Uses strtok_r() internally.  Returns the
// number of tokens and populates the 'tokens' array with pointers.
int splitstr(char ** tokens, size_t max_tokens,
             char * str, const char * delim);

// Like splitstr only do NOT alter original string.  This is useful for
// counting would-be tokens, and for marking where they begin.  Note that
// because NULL terminators are not inserted, the tokens are not terminated.
// One of the use cases prevents us from simply using strdup() followed by
// splitstr(), because then the pointers would be to the copy rather than
// the original unaltered string.
int markstr(char ** markers, size_t max_markers,
            const char * str, const char * delim);


