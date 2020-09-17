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
    // Factory function that creates a chain.  A destructor callback may
    // be provided for destroying data payload memory.  This callback must
    // have the same signature as free().  NULL may be passed if the data
    // is static or if the pointer value itself is directly assigned or
    // not to be managed by the chain.  'free' may be passed if the data
    // was allocated by a simple 'malloc' call.
    struct chain_t * (*create)(data_destroy_f data_destroy);

    // Chain destructor function
    void (*destroy)(void * chain);

    // Empties the chain: Removes all links and destroys their data payloads.
    // Effectively brings the chain back to factory condition.
    void (*clear)(struct chain_t * chain);

    // Insert a new link after the current link, spin forward to it,
    // and assign data to the new link.  Data is assumed to be of the uniform
    // type that can be destroyed by data_destroy_f data_destroy.
    void (*insert)(struct chain_t * chain, void * data);

    // Delete the current link, destroying its data payload, and spin back to
    // the previous link.  All data payloads are assumed to be of the uniform
    // type that can be destroyed by data_destroy_f data_destroy.
    void (*remove)(struct chain_t * chain);

    // Reset the chain position back to the origin link
    void (*reset)(struct chain_t * chain);

    // Seeks the chain to the requested position, either forward (positive
    // offset) or backward (negative offset).  Note there is the possibility
    // that if the chain length gets larger than MAX_
    bool (*spin)(struct chain_t * chain, int64_t offset);

    // Walk through the chain and remove all links with NULL data payloads.
    // This can be very useful after collecting data, and before processing
    // analyzing, and presenting results.
    void (*trim)(struct chain_t * chain);

    // Sort the data payloads within a chains according to the data comparator
    // function provided.  The form of the comparator callback should be:
    //
    //     int compare(const void * a, const void * b)
    //     {
    //         mytype_t * aptr = (mytype_t *) *(void **) a;
    //         mytype_t * bptr = (mytype_t *) *(void **) b;
    //
    // Internally, this uses the libc qsort() function on a dynamic array of
    // data payload pointers for optimum performance.
    void (*sort)(struct chain_t * chain, data_compare_f data_compare);

    // Makes a full deep copy of the given chain.  The data_copy function
    // (if not NULL) is called for each link data payload.
    struct chain_t * (*copy)(struct chain_t * chain, data_copy_f data_copy);

    // This splits a chain into two segments: The segment specified by the
    // 'begin' and 'end' indexes into the chain is returned,  and the remainder
    // segment is repaired and left as the original chain object (minus the
    // separated segment).
    struct chain_t * (*split)(struct chain_t * chain, size_t begin, size_t end);

    // Joins together two chain segments and return the larger result.  The head
    // chain is modified and the tail chain is destroyed
    struct chain_t * (*join)(struct chain_t * head, struct chain_t * tail);


    // TODO: length() and data() as accessor methods

    ////////////////////////////////////////////////////
    // TODO convert this to PIMPL void * data
    link_t * link;                // current link in chain
    link_t * orig;                // origin link in chain
    size_t length;                // list length

    // TODO: pass this into remove() and destroy() rather than storing
    // it here.  this either belongs as an object method of link_t or
    // as a chain_t method argument
    data_destroy_f data_destroy;  // link destroyer function

    // TODO: move all functions to here as object method function pointers
    // that get populated in the factory function.
}
chain_t;

//------------------------------------------------------------------------|
// Public factory function that creates a new chain
chain_t * chain_create(data_destroy_f data_destroy);

// Public destructor function that destroys a chain
void chain_destroy(void * chain);
