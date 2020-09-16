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

#include "chain.h"
#include "blammo.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

//------------------------------------------------------------------------|
static void chain_clear(chain_t * chain)
{
    // delete links until none left
    while (chain->link != NULL)
    {
        chain->delete(chain);
    }

    chain->orig = NULL;
    chain->length = 0;
}

//------------------------------------------------------------------------|
static void chain_insert(chain_t * chain, void * data)
{
    link_t * link = NULL;

    // create a new link
    link = (link_t *) malloc(sizeof(link_t));
    if (!link)
    {
        BLAMMO(ERROR, "malloc(sizeof(link_t)) failed\n");
        return;
    }

    // check if linking in the origin
    if (chain->link == NULL)
    {
        chain->link = link;
        chain->orig = link;
        chain->link->next = link;
        chain->link->prev = link;
    }
    else
    {
        // link new link in between current and next link
        chain->link->next->prev = link;
        link->next = chain->link->next;
        link->prev = chain->link;
        chain->link->next = link;
    }

    // move forward to the new link
    // NOTE: Harmless but unnecessary
    // call/return for origin node.
    // and initialize new link's contents
    chain->spin(chain, 1);
    chain->link->data = data;
    chain->length ++;
}

//------------------------------------------------------------------------|
static void chain_delete(chain_t * chain)
{
    link_t * link = NULL;

    // free link's data contents
    if (NULL != chain->link->data)
    {
        if (NULL != chain->data_destroy)
        {
            chain->data_destroy(chain->link->data);
        }

        chain->link->data = NULL;
    }

    // check if we're about to delete the origin link
    // if so, redefine the new origin as the next link
    if (chain->link == chain->orig)
    {
        chain->orig = chain->link->next;
    }

    // remember previous link
    link = chain->link->prev;

    // unlink current link
    chain->link->prev->next = chain->link->next;
    chain->link->next->prev = chain->link->prev;

    // free the current link
    free(chain->link);

    // make current link old previous link
    chain->link = chain->length > 1 ? link : NULL;

    // the chain is effectively one link shorter either way
    chain->length --;
}

//------------------------------------------------------------------------|
static void chain_reset(chain_t * chain)
{
    chain->link = chain->orig;
}

//------------------------------------------------------------------------|
static bool chain_spin(chain_t * chain, int64_t index)
{
    while (index > 0)
    {
        chain->link = chain->link->next;
        index--;
    }

    while (index < 0)
    {
        chain->link = chain->link->prev;
        index++;
    }

    return (chain->link != chain->orig);
}

//------------------------------------------------------------------------|
static void chain_trim(chain_t * chain)
{
    if (chain->length > 0)
    {
        chain->reset(chain);

        do
        {
            if (!chain->link->data)
            {
                chain->link->data = NULL;
                chain->delete(chain);
                chain->reset(chain);
            }

            chain->spin(chain, 1);
        }
        while (chain->link != chain->orig);
    }
}

//------------------------------------------------------------------------|
static void chain_sort(chain_t * chain, data_compare_f data_compare)
{
    // cannot sort lists of length 0 or 1
    if ((chain->length < 2) || (data_compare == NULL))
    {
        return;
    }

    // No need to rearrange links, just the data
    // pointers within each link.
    void ** data_ptrs = (void **) malloc(sizeof(void *) * chain->length);
    if (!data_ptrs)
    {
        BLAMMO(ERROR, "malloc(sizeof(void *) * %zu) failed\n", chain->length);
        return;
    }

    // fill in the link pointer array from the chain
    size_t index = 0;
    chain->reset(chain);
    do
    {
        data_ptrs[index++] = chain->link->data;
        chain->spin(chain, 1);
    }
    while (chain->link != chain->orig);

    // call quicksort on the array of data pointers
    qsort(data_ptrs, chain->length, sizeof(void *), data_compare);

    // now directly re-arrange all of the data pointers
    // the chain reset may not technically be necessary
    // because of the exit condition of the previous loop
    chain->reset(chain);
    for (index = 0; index < chain->length; index++)
    {
        chain->link->data = data_ptrs[index];
        chain->spin(chain, 1);
    }

    free(data_ptrs);
    data_ptrs = NULL;
}

//------------------------------------------------------------------------|
// Not static because also exposed via the header, so that it can be
// included as a payload in other chains.
void chain_destroy(void * chain_ptr)
{
    chain_t * chain = (chain_t *) chain_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!chain)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy\n");
        return;
    }

    // remove all links - note there is no data dtor here, so the user is
    // expected to have previously called free() on every payload.
    chain->clear(chain);

    // zero out the remaining empty chain
    memset(chain, 0, sizeof(chain_t));

    // destroy the chain
    free(chain);
    chain = NULL;
}

//------------------------------------------------------------------------|
chain_t * chain_create(data_destroy_f data_destroy)
{
    chain_t * chain = (chain_t *) malloc(sizeof(chain_t));
    if (!chain)
    {
        BLAMMO(ERROR, "malloc(sizeof(chain_t)) failed\n");
        return NULL;
    }

    // initialize components
    memset(chain, 0, sizeof(chain_t));

    chain->create = chain_create;
    chain->destroy = chain_destroy;
    chain->clear = chain_clear;
    chain->insert = chain_insert;
    chain->delete = chain_delete;
    chain->reset = chain_reset;
    chain->spin = chain_spin;
    chain->trim = chain_trim;
    chain->sort = chain_sort;

    chain->data_destroy = data_destroy;

    return chain;
}

//------------------------------------------------------------------------|
// create a complete copy of a chain.  note this requires the caller to
// define a link-copy function
chain_t * chain_copy(chain_t * chain, data_copy_f data_copy)
{
    chain_t * copy = chain_create(chain->data_destroy);
    void * data = NULL;

    if (!copy)
    {
        BLAMMO(ERROR, "chain_create() copy failed\n");
        return NULL;
    }

    if (chain->length > 0)
    {
        chain->reset(chain);
        do
        {
        	data = data_copy ? data_copy(chain->link->data) : chain->link->data;
            chain->insert(copy, data);
            chain->spin(chain, 1);
        }
        while (chain->link != chain->orig);
    }

    return copy;
}

//------------------------------------------------------------------------|
// This breaks a chain into two segments: The segment specified by the
// 'begin' and 'end' indexes into the chain is returned,  and the remainder
// segment is repaired and left as the original chain object (minus the
// separated segment).
chain_t * chain_segment(chain_t * chain, size_t begin, size_t end)
{
    link_t * link = NULL;
    chain_t * segment = chain_create(chain->data_destroy);

    if (!segment)
    {
        BLAMMO(ERROR, "chain_create() segment failed\n");
        return NULL;
    }

    // We do not need the origin link in the new segment
    // NOTE: This would not be necessary if we allowed truly empty
    // chains.
    free(segment->link);

    // set new segment length
    segment->length = end - begin;

    // set the original chain to the first link in what will become the
    // origin of the cut segment.
    chain->reset(chain);
    chain->spin(chain, begin);
    segment->link = chain->link;
    segment->orig = chain->link;

    // set chain position to one-after the final link of the segment
    // cache this link's prev because we'll need it to fix chain linkage
    chain->spin(chain, segment->length);
    link = segment->link->prev;

    // separate the segment and fix up the now shorter chain
    chain->link->prev->next = segment->link;
    segment->link->prev = chain->link->prev;
    link->next = chain->link;
    chain->link->prev = link;
    chain->length -= segment->length;

    return segment;
}

//------------------------------------------------------------------------|
// join together two chain segments and return the larger result.  the head
// chain is modified and the tail chain is destroyed
chain_t * chain_splice(chain_t * head, chain_t * tail)
{
    link_t * link = NULL;

    // can't join empty lists, but it is allowable for one or the other
    // to be empty.  this is a cheap workaround by adding empty links
    // to avoid breakage, but will in some cases result in empty links,
    // this implementation needs to be improved (TODO).

    // FIXME: BROKEN HACK

    if (head->length == 0)
    {
    	head->insert(head, NULL);
    }

    if (tail->length == 0)
    {
    	tail->insert(tail, NULL);
    }

    // reset both chains to origin link
    head->reset(head);
    tail->reset(tail);

    // link the tail segment to the end of the head segment
    link = tail->link->prev;
    head->link->prev->next = tail->link;
    tail->link->prev = head->link->prev;
    head->link->prev = link;
    link->next = head->link;

    // the tail container is no longer in a valid state: blow it away.
    // memory management becomes the responsibility of the head chain
    head->length += tail->length;

    memset(tail, 0, sizeof(chain_t));
    free(tail);

    return head;
}

