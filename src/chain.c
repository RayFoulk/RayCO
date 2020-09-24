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
    if (chain->empty(chain))
    {
        return NULL;
    }

    return ((chain_priv_t *) chain->priv)->link->data;
}

//------------------------------------------------------------------------|
static inline size_t chain_length(chain_t * chain)
{
    return ((chain_priv_t *) chain->priv)->length;
}

//------------------------------------------------------------------------|
static inline bool chain_empty(chain_t * chain)
{
    return (NULL == ((chain_priv_t *) chain->priv)->link);
}

//------------------------------------------------------------------------|
static inline bool chain_origin(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    return (priv->orig == priv->link);    
}

//------------------------------------------------------------------------|
static void chain_clear(chain_t * chain)
{
    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    // remove links until none left
    while (!chain->empty(chain))
    {
        chain->remove(chain);
    }

    // TODO: Likely this is redundant
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
    if (chain->empty(chain))
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
    priv->length ++;
}

//------------------------------------------------------------------------|
static void chain_remove(chain_t * chain)
{
    if (chain->empty(chain))
    {
        return;
    }

    chain_priv_t * priv = (chain_priv_t *) chain->priv;

    // free and zero link's data contents
    if (NULL != priv->link->data)
    {
        if (NULL != priv->data_destroy)
        {
            priv->data_destroy(priv->link->data);
        }

        priv->link->data = NULL;
    }

    // check if we're about to remove the origin link
    // if so, designate the next link as the origin
    if (chain->origin(chain))
    {
        priv->orig = priv->link->next;
    }

    // remember previous link.  This will be the final
    // chain position after link deletion
    link_t * link = priv->link->prev;

    // unlink current link
    priv->link->prev->next = priv->link->next;
    priv->link->next->prev = priv->link->prev;

    // free the current link
    free(priv->link);

    // make current link old previous link
    if (priv->length > 1)
    {
        priv->link = link;
    }
    else
    {
        // The origin link itself was just removed.
        priv->link = NULL;
        priv->orig = NULL;
    }

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
    if (chain->empty(chain))
    {
        return false;
    }

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
static size_t chain_trim(chain_t * chain)
{
    if (chain->empty(chain))
    {
        return 0;
    }

    size_t trimmed = 0;
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    chain->reset(chain);

    do
    {
        if (NULL == priv->link->data)
        {
            chain->remove(chain);
            trimmed++;
        }
        else
        {
            // it's more natural to spin backwards because
            // of the way remove reverts to previous, and
            // also may redesignate origin as next.
            chain->spin(chain, -1);
        }
    }
    while (!chain->origin(chain));

    return trimmed;
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
        BLAMMO(ERROR, "malloc(sizeof(void *) * %zu) failed\n", priv->length);
        return;
    }

    // fill in the link pointer array from the chain
    size_t index = 0;
    chain->reset(chain);
    do
    {
        data_ptrs[index++] = priv->link->data;
    }
    while (chain->spin(chain, 1));

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

    if (priv->length > 0)
    {
        chain->reset(chain);
        do
        {
            data = data_copy ? data_copy(priv->link->data) : priv->link->data;
            chain->insert(copy, data);
        }
        while (chain->spin(chain, 1));
    }

    return copy;
}

//------------------------------------------------------------------------|
static chain_t * chain_split(chain_t * chain, size_t begin, size_t end)
{
    link_t * link = NULL;
    chain_priv_t * priv = (chain_priv_t *) chain->priv;
    chain_t * seg = chain_create(priv->data_destroy);

    if (!seg)
    {
        BLAMMO(ERROR, "chain_create() seg failed\n");
        return NULL;
    }

    // advance the chain to the link to cut
    chain->reset(chain);
    chain->spin(chain, begin);

    // set the original chain to the first link in what will become the
    // origin of the cut segment.  set new segment length
    chain_priv_t * seg_priv = (chain_priv_t *) seg->priv;
    seg_priv->link = priv->link;
    seg_priv->orig = priv->link;
    seg_priv->length = end - begin;

    // set chain position to one-after the final link of the seg
    // cache this link's prev because we'll need it to fix chain linkage
    chain->spin(chain, seg_priv->length);
    link = seg_priv->link->prev;

    // separate the seg and fix up the now shorter chain
    priv->link->prev->next = seg_priv->link;
    seg_priv->link->prev = priv->link->prev;
    link->next = priv->link;
    priv->link->prev = link;
    priv->length -= seg_priv->length;

    return seg;
}

//------------------------------------------------------------------------|
static bool chain_join(chain_t * head, chain_t * tail)
{
    link_t * link = NULL;
    chain_priv_t * head_priv = (chain_priv_t *) head->priv;
    chain_priv_t * tail_priv = (chain_priv_t *) tail->priv;
    
    // Cannot join chains of dissimilar data types
    if (head_priv->data_destroy != tail_priv->data_destroy)
    {
        BLAMMO(ERROR, "chain_join() cannot join chains with dissimilar "
            "data destructors %p and %p\n",
            head_priv->data_destroy,
            tail_priv->data_destroy);
        return false;
    }

    // One or the other chain may be empty.  If the achain is empty, then
    // simply take all the contents from the bchain as the final result.
    // if the bchain itself is also empty, this still validly returns
    // an empty result.
    if (head->empty(head))
    {
        // memcpy() would work too, but unnecessarily clobber destructor
        head_priv->link = tail_priv->link;
        head_priv->orig = tail_priv->orig;
        head_priv->length = tail_priv->length;
        tail_priv->link = NULL;
        tail_priv->orig = NULL;
        tail_priv->length = 0;
        return true;
    }

    // Here we know the head is not empty, but if the tail is empty,
    // then we're effectively done.  No operation is necessary!
    if (tail->empty(tail))
    {
        return true;
    }

    // reset both chains to origin link
    head->reset(head);
    tail->reset(tail);

    // link the tail seg to the end of the head segment
    link = tail_priv->link->prev;
    head_priv->link->prev->next = tail_priv->link;
    tail_priv->link->prev = head_priv->link->prev;
    head_priv->link->prev = link;
    link->next = head_priv->link;

    // the tail container is now empty, and the head chain has assumed
    // ownership of all it's links.
    head_priv->length += tail_priv->length;
    tail_priv->length = 0;

    return true;
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
static const chain_t chain_calls = {
    &chain_create,
    &chain_destroy,
    &chain_data,
    &chain_length,
    &chain_empty,
    &chain_origin,
    &chain_clear,
    &chain_insert,
    &chain_remove,
    &chain_reset,
    &chain_spin,
    &chain_trim,
    &chain_sort,
    &chain_copy,
    &chain_split,
    &chain_join,
    NULL
};

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

    // bulk copy all function pointers and init opaque ptr
    memcpy(chain, &chain_calls, sizeof(chain_t));

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
