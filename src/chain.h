//------------------------------------------------------------------------|
// Copyright (c) 2018-2019 by Raymond M. Foulk IV
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
#include <stdlib.h>
#include <stdbool.h>

#define USE_REFACTORED_DATA_SORT
//#define USE_REFACTORED_ORIG_ALLOC

//------------------------------------------------------------------------|
typedef int (*link_compare_f) (const void *, const void *);
typedef void * (*link_copy_f) (void *);
typedef void (*link_destroy_f) (void *);

//------------------------------------------------------------------------|
// NOTE: All links data types are assumed to be homogeneous.  Heterogeneous
// link payloads are not considered in this implementation.
typedef struct link_t
{
    struct link_t * next;    // pointer to next node
    struct link_t * prev;    // pointer to previous node
    void * data;             // pointer to node's contents,
                             // caller is responsible for data size
}
link_t;

typedef struct
{
    link_t * link;                // current link in chain
    link_t * orig;                // origin link in chain
    size_t length;                // list length
    link_destroy_f link_destroy;  // link destroyer function
}
chain_t;

//------------------------------------------------------------------------|
chain_t * chain_create(link_destroy_f link_destroy);
void chain_destroy(void * chain);
void chain_clear(chain_t * chain);    // remove all links (no data dtor!!)
void chain_insert(chain_t * chain, void * data); // insert new link after & go to it
void chain_delete(chain_t * chain);   // delete current link & go back
bool chain_forward(chain_t * chain, size_t index);
bool chain_rewind(chain_t * chain, size_t index);
void chain_trim(chain_t * chain);     // delete links with NULL data payload
void chain_reset(chain_t * chain);    // reset position back to origin link
void chain_sort(chain_t * chain, link_compare_f compare_func);
chain_t * chain_copy(chain_t * chain, link_copy_f copy_func);
chain_t * chain_segment(chain_t * chain, size_t begin, size_t end);
chain_t * chain_splice(chain_t * head, chain_t * tail);
