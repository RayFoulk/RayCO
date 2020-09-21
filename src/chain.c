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
// All links data types are assumed to be homogeneous.
// Heterogeneous payloads are not considered in this implementation.
typedef struct link_t
{
    // Pointer to next link
    struct link_t * next;

    // Pointer to previous link
    struct link_t * prev;

    // Pointer to link's contents,
    void * data;
}
link_t;

// chain private implementation data
typedef struct
{
    // Current link in the chain
    link_t * link;

    // The 'origin' link of the chain
    link_t * orig;

    // The chain length, number of links
    size_t length;

    // The link data destructor function for all links.
    // This can be NULL for static or unmanaged data
    data_destroy_f data_destroy;
}
chain_priv_t;

//------------------------------------------------------------------------|
static void * chain_data(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    if (NULL == priv->link)
    {
        return NULL;
    }

    return priv->link->data;
}

//------------------------------------------------------------------------|
static inline size_t chain_length(chain_t * chain)
{
    return ((chain_priv_t *) chain->priv)->length;
}

//------------------------------------------------------------------------|
static inline bool chain_empty(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    return (NULL == priv->link);    
}

//------------------------------------------------------------------------|
static inline bool chain_orig(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    return (priv->orig == priv->link);    
}

//------------------------------------------------------------------------|
static void chain_clear(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    // remove links until none left
    while (NULL != priv->link)
    {
        chain->remove(chain);
    }

    priv->orig = NULL;
    priv->length = 0;
}

//------------------------------------------------------------------------|
static void chain_insert(chain_t * chain, void * data)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    link_t * link = (link_t *) malloc(sizeof(link_t));

    if (!link)
    {
        BLAMMO(ERROR, "malloc(sizeof(link_t)) failed\n");
        return;
    }

    // check if linking in the origin
    if (!priv->link)
    {
        priv->link = link;
        priv->orig = link;
        priv->link->next = link;
        priv->link->prev = link;
    }
    else
    {
        // link new link in between current and next link
        priv->link->next->prev = link;
        link->next = priv->link->next;
        link->prev = priv->link;
        priv->link->next = link;
    }

    // move forward to the new link
    // NOTE: Harmless but unnecessary
    // call/return for origin node.
    // and initialize new link's contents
    chain->spin(chain, 1);
    priv->link->data = data;
    chain->length ++;
}

//------------------------------------------------------------------------|
static void chain_remove(chain_t * chain)
{
    link_t * link = NULL;
    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    // free and zero link's data contents
    if (priv->link->data)
    {
        if (priv->data_destroy)
        {
            priv->data_destroy(priv->link->data);
        }

        priv->link->data = NULL;
    }

    // check if we're about to remove the origin link
    // if so, redefine the new origin as the next link
    if (priv->link == priv->orig)
    {
        priv->orig = priv->link->next;
    }

    // remember previous link
    link = priv->link->prev;

    // unlink current link
    priv->link->prev->next = priv->link->next;
    priv->link->next->prev = priv->link->prev;

    // free the current link
    free(priv->link);

    // make current link old previous link
    priv->link = priv->length > 1 ? link : NULL;

    // the chain is effectively one link shorter either way
    priv->length --;
}

//------------------------------------------------------------------------|
static inline void chain_reset(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    priv->link = priv->orig;
}

//------------------------------------------------------------------------|
static bool chain_spin(chain_t * chain, int64_t index)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    while (index > 0)
    {
        priv->link = priv->link->next;
        index--;
    }

    while (index < 0)
    {
        priv->link = priv->link->prev;
        index++;
    }

    return (priv->link != priv->orig);
}

//------------------------------------------------------------------------|
static void chain_trim(chain_t * chain)
{
    if (chain->length > 0)
    {
        chain_priv_t * priv = (chain_priv_t *) chain->priv;
        chain->reset(chain);

        do
        {
            if (!priv->link->data)
            {
                chain->remove(chain);
                chain->reset(chain);
            }

            // TODO: use bool return from spin
            chain->spin(chain, 1);
        }
        while (priv->link != priv->orig);
    }
}

//------------------------------------------------------------------------|
static void chain_sort(chain_t * chain, data_compare_f data_compare)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    // cannot sort lists of length 0 or 1
    if ((priv->length < 2) || (data_compare == NULL))
    {
        return;
    }

    // No need to rearrange links, just the data
    // pointers within each link.
    void ** data_ptrs = (void **) malloc(sizeof(void *) * priv->length);
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
        data_ptrs[index++] = priv->link->data;
        chain->spin(chain, 1);
        // TODO: use bool return from spin
    }
    while (priv->link != priv->orig);

    // call quicksort on the array of data pointers
    qsort(data_ptrs, priv->length, sizeof(void *), data_compare);

    // now directly re-arrange all of the data pointers
    // the chain reset may not technically be necessary
    // because of the exit condition of the previous loop
    chain->reset(chain);
    for (index = 0; index < priv->length; index++)
    {
        priv->link->data = data_ptrs[index];
        chain->spin(chain, 1);
    }

    free(data_ptrs);
    data_ptrs = NULL;
}

//------------------------------------------------------------------------|
static chain_t * chain_copy(chain_t * chain, data_copy_f data_copy)
{
    void * data = NULL;
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    chain_t * copy = chain_create(priv->data_destroy);

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
            data = data_copy ? data_copy(priv->link->data) : priv->link->data;
            chain->insert(copy, data);
            chain->spin(chain, 1);
        }

        // TODO: use bool return from spin
        while (priv->link != priv->orig);
    }

    return copy;
}

//------------------------------------------------------------------------|
static chain_t * chain_split(chain_t * chain, size_t begin, size_t end)
{
    link_t * link = NULL;
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    chain_t * segment = chain_create(priv->data_destroy);

    if (!segment)
    {
        BLAMMO(ERROR, "chain_create() segment failed\n");
        return NULL;
    }

    // advance the chain to the link to cut
    chain->reset(chain);
    chain->spin(chain, begin);

    // set the original chain to the first link in what will become the
    // origin of the cut segment.  set new segment length
    chain_priv_t * segment_priv = (chain_priv_t *) segment->priv;
    segment_priv->link = priv->link;
    segment_priv->orig = priv->link;
    segment_priv->length = end - begin;

    // set chain position to one-after the final link of the segment
    // cache this link's prev because we'll need it to fix chain linkage
    chain->spin(chain, segment_priv->length);
    link = segment_priv->link->prev;

    // separate the segment and fix up the now shorter chain
    priv->link->prev->next = segment_priv->link;
    segment_priv->link->prev = priv->link->prev;
    link->next = priv->link;
    priv->link->prev = link;
    priv->length -= segment_priv->length;

    return segment;
}

//------------------------------------------------------------------------|
static chain_t * chain_join(chain_t * head, chain_t * tail)
{
    link_t * link = NULL;
    chain_priv_t * head_priv = (chain_priv_t *) head->priv;
    chain_priv_t * tail_priv = (chain_priv_t *) tail->priv;
    
    // can't join empty lists, but it is allowable for one or the other
    // to be empty.  this is a cheap workaround by adding empty links
    // to avoid breakage, but will in some cases result in empty links,
    // this implementation needs to be improved (TODO).

    // FIXME: BROKEN HACK

    if (head_priv->length == 0)
    {
        head->insert(head, NULL);
    }

    if (tail_priv->length == 0)
    {
        tail->insert(tail, NULL);
    }

    // reset both chains to origin link
    head->reset(head);
    tail->reset(tail);

    // link the tail segment to the end of the head segment
    link = tail_priv->link->prev;
    head_priv->link->prev->next = tail_priv->link;
    tail_priv->link->prev = head_priv->link->prev;
    head_priv->link->prev = link;
    link->next = head_priv->link;

    // TODO: Refactor to simply take ownership of links
    // leaving the tail empty but valid

    // the tail container is no longer in a valid state: destroy it
    // memory management becomes the responsibility of the head chain
    head_priv->length += tail_priv->length;

    // this is essentially destroy() without clear()
    memset(tail->priv, 0, sizeof(chain_priv_t));
    free(tail->priv);

    memset(tail, 0, sizeof(chain_t));
    free(tail);

    return head;
}

//------------------------------------------------------------------------|
// Not static because also exposed via the header, so that it can be
// included as a payload in other chains.
void chain_destroy(void * chain_ptr)
{
    chain_t * chain = (chain_t *) chain_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!chain || !chain->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy\n");
        return;
    }

    // remove all links and destroy their data
    chain->clear(chain);

    // zero out and destroy the private data
    memset(chain->priv, 0, sizeof(chain_priv_t));
    free(chain->priv);

    // zero out and destroy the public interface
    memset(chain, 0, sizeof(chain_t));
    free(chain);
}

//------------------------------------------------------------------------|
chain_t * chain_create(data_destroy_f data_destroy)
{
    // Allocate and initialize public interface
    chain_t * chain = (chain_t *) malloc(sizeof(chain_t));
    if (!chain)
    {
        BLAMMO(ERROR, "malloc(sizeof(chain_t)) failed\n");
        return NULL;
    }

    memset(chain, 0, sizeof(chain_t));
    chain->create = chain_create;
    chain->destroy = chain_destroy;
    chain->data = chain_data;
    chain->length = chain_length;
    chain->empty = chain_empty;
    chain->orig = chain_orig;
    chain->clear = chain_clear;
    chain->insert = chain_insert;
    chain->remove = chain_remove;
    chain->reset = chain_reset;
    chain->spin = chain_spin;
    chain->trim = chain_trim;
    chain->sort = chain_sort;
    chain->copy = chain_copy;
    chain->split = chain_split;
    chain->join = chain_join;

    // Allocate and initialize private implementation
    chain->priv = malloc(sizeof(chain_priv_t));
    if (!chain->priv)
    {
        BLAMMO(ERROR, "malloc(sizeof(chain_priv_t)) failed\n");
        free(chain);
        return NULL;
    }

    memset(chain->priv, 0, sizeof(chain_priv_t));
    ((chain_priv_t *) chain->priv)->data_destroy = data_destroy;

    return chain;
}
