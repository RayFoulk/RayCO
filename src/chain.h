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

#include "utils.h"          // generic function signatures

//------------------------------------------------------------------------|
typedef struct chain_t
{
    // Factory function that creates a chain.  A destructor callback may
    // be provided for destroying data payload memory.  This callback must
    // have the same signature as free().  NULL may be passed if the data
    // is static or if the pointer value itself is directly assigned or
    // not to be managed by the chain.  'free' may be passed if the data
    // was allocated by a simple 'malloc' call.
    struct chain_t * (*create)(generic_copy_f data_copy,
                               generic_destroy_f data_destroy);

    // Chain destructor function
    void (*destroy)(void * chain);

    // Access to current link data
    void * (*data)(struct chain_t * chain);

    // Get the chain's current length
    size_t (*length)(struct chain_t * chain);

    // Returns true if the chain is empty and false otherwise
    bool (*empty)(struct chain_t * chain);

    // Returns true if chain is positioned at origin link
    bool (*origin)(struct chain_t * chain);

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
    // that if the chain length gets larger than the maximum possible for
    // offset, that certain links will take multiple calls to reach.
    bool (*spin)(struct chain_t * chain, ssize_t offset);

    // 'Streamlined' iterators: More focused use cases, less generic.
    // Sets position to origin link (I.E. "first") and returns origin data.
    // This call is equivalent to reset(), data()
    void * (*first)(struct chain_t * chain);

    // 'Streamlined' iterators: More focused use cases, less generic.
    // Sets position to origin-1 (I.E. "last") and returns link data.
    // This call is equivalent to reset(), spin(-1), data()
    void * (*last)(struct chain_t * chain);

    // 'Streamlined' iterators: More focused use cases, less generic.
    // Advances to the next link and returns link data.
    // This call is equivalent to spin(1), data() except that it will
    // artificially return NULL when it hits the origin, thus emulating
    // a non-circular list.
    void * (*next)(struct chain_t * chain);

    // 'Streamlined' iterators: More focused use cases, less generic.
    // Reverses to the prev link and returns link data.
    // This call is equivalent to spin(-1), data() except that it will
    // artificially return NULL when it hits origin-1 (last), thus
    // emulating a non-circular list.
    void * (*prev)(struct chain_t * chain);

    // Walk through the chain and remove all links with NULL data payloads.
    // This can be very useful after collecting data, and before processing
    // analyzing, and presenting results.
    size_t (*trim)(struct chain_t * chain);

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
    void (*sort)(struct chain_t * chain, generic_compare_f data_compare);

    // search through all links and return the first one that matches
    // the given reference link (may or may not be part of the chain)
    // according to the criterion applied by the comparison function.
    // returns NULL if no matching link is found in the chain
    void * (*find)(struct chain_t * chain, void * data, generic_compare_f data_compare);

    // Makes a full deep copy of the given chain.  The data_copy function
    // (if not NULL) is called for each link data payload.
    struct chain_t * (*copy)(struct chain_t * chain);

    // This splits a chain into two segments: The segment specified by the
    // 'begin' and 'end' indexes into the chain is returned,  and the remainder
    // segment is repaired and left as the original chain object (minus the
    // separated segment).
    struct chain_t * (*split)(struct chain_t * chain, size_t begin, size_t end);

    // Joins together the _contents_ of two chain segments.  Both chains are
    // modified in place: The 'head' chain accumulates the concatenated links
    // of the 'tail' chain and its own, while the 'tail' chain is emptied back
    // to it's factory state.  Returns true on success or false on failure.
    bool (*join)(struct chain_t * head, struct chain_t * tail);

    // Private data
    void * priv;
}
chain_t;

//------------------------------------------------------------------------|
// Public chain interface
extern const chain_t chain_pub;
