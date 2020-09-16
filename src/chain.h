//------------------------------------------------------------------------|
// Copyright (c) 2018-2020 by Raymond M. Foulk IV
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
typedef int (*data_compare_f) (const void *, const void *);
typedef void * (*data_copy_f) (const void *);
typedef void (*data_destroy_f) (void *);

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

typedef struct chain_t
{
	// Empties the chain: Removes all links and destroys their data payloads
    void (*clear)(struct chain_t * chain);

    // Insert a new link after the current link, spin forward to it,
    // and assign data to the new link.  Data is assumed to be of the uniform
    // type that can be destroyed by data_destroy_f data_destroy.
    void (*insert)(struct chain_t * chain, void * data);


	////////////////////////////////////////////////////
    // TODO convert this to PIMPL void * data
    link_t * link;                // current link in chain
    link_t * orig;                // origin link in chain
    size_t length;                // list length

    // TODO: pass this into delete() and destroy() rather than storing
    // it here.  this either belongs as an object method of link_t or
    // as a chain_t method argument
    data_destroy_f data_destroy;  // link destroyer function

    // TODO: move all functions to here as object method function pointers
    // that get populated in the factory function.
}
chain_t;

//------------------------------------------------------------------------|
// Factory function that creates a Chain
chain_t * chain_create(data_destroy_f data_destroy);

// Destructor function for a Chain
void chain_destroy(void * chain);

// TODO: Make these static / object methods
//void chain_insert(chain_t * chain, void * data); // insert new link after & go to it
void chain_delete(chain_t * chain);   // delete current link & go back
void chain_reset(chain_t * chain);    // reset position back to origin link
bool chain_spin(chain_t * chain, int64_t index);
void chain_trim(chain_t * chain);     // delete links with NULL data payload
void chain_sort(chain_t * chain, data_compare_f data_compare);
chain_t * chain_copy(chain_t * chain, data_copy_f data_copy);
chain_t * chain_segment(chain_t * chain, size_t begin, size_t end);
chain_t * chain_splice(chain_t * head, chain_t * tail);
