//------------------------------------------------------------------------|
// Copyright (c) 2018 by Raymond M. Foulk IV
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

//------------------------------------------------------------------------|
#include <string.h>
#include <stdlib.h>

#include "chain.h"

//------------------------------------------------------------------------|
// factory-style creation of chain-link list
chain_t * chain_create()
{
    chain_t * chain = (chain_t *) malloc(sizeof(chain_t));
    if (!chain)
    {
        // TODO: replace these witha vararg macro
        printf("%s: ERROR: malloc(sizeof(chain_t)) failed\n", __FUNCTION__);
        return NULL;
    }

    // initialize components
    memset(chain, 0, sizeof(chain_t));

    // allocate initial link of this list
    // this will be the origin link.
    chain->link = (link_t *) malloc(sizeof(link_t));
    if (!chain->link)
    {
        printf("%s: ERROR: malloc(sizeof(link_t)) failed\n", __FUNCTION__);
        free(chain);
        chain = NULL;
        return NULL;
    }

    // mark origin link for later reset () calls
    // and link the origin link to itself...
    chain->link->next = chain->link;
    chain->link->prev = chain->link;
    chain->orig = chain->link;
    chain->link->data = NULL;

    return chain;
}

//------------------------------------------------------------------------|
void chain_destroy(chain_t * chain)
{
    // guard against accidental double-destroy or early-destroy
    if (!chain || !chain->orig)
    {
        return;
    }

    // remove all links - note there is no data dtor here, so the user is
    // expected to have previously called free() on every payload.
    chain_clear(chain);

    // free the origin link, which is the only one left
    free (chain->link);

    // zero out the remaining empty chain
    memset(chain, 0, sizeof(chain_t));

    // destroy the chain
    free(chain);
    chain = NULL;
}

//------------------------------------------------------------------------|
void chain_clear(chain_t * chain)
{

    chain_reset(chain);
    chain_rewind(chain, 1);

    // delete links until last remaining (origin)
    while (chain->link != chain->link->next)
    {
        chain_delete (chain);
    }

    // The last link may still contain data.
    // May need to add a dtor function to the chain factory
    // and main data strucure to fix potential memory leak.
    // valgrind this later to be sure
    if (NULL != chain->link->data)
    {
        free(chain->link->data) ;
        chain->link->data = NULL;
    }

    chain->length = 0;
}

//------------------------------------------------------------------------|
// insert a link after current link and advance to it
void chain_insert(chain_t * chain)
{
    link_t * link = NULL;

    // only add another link if length is non-zero
    // because length can be zero while origin link exists
    if (chain->length > 0)
    {
        // create a new link
        link = (link_t *) malloc(sizeof(link_t));
        if (!link)
        {
            printf("%s: ERROR: malloc(sizeof(link_t)) failed\n", __FUNCTION__);
            return;
        }

        // link new link in between current and next link
        chain->link->next->prev = link;
        link->next = chain->link->next;
        link->prev = chain->link;
        chain->link->next = link;

        // move forward to the new link
        // and initialize new link's contents
        chain_forward(chain, 1);
        chain->link->data = NULL;
    }

    chain->length ++;
}

//------------------------------------------------------------------------|
// delete current link and revert back to the previous link as current
void chain_delete(chain_t * chain)
{
    link_t * link = NULL;

    // free link's data contents
    // FIXME: this implementation assumes the chain user has allocated
    // something using malloc.  this should be fixed using a dtor function
    if (NULL != chain->link->data)
    {
       free(chain->link->data) ;
       chain->link->data = NULL;
    }

    // only delete link if length is greater than 1
    if (chain->length > 1)
    {
        // check if we're about to delete the origin link
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
        free (chain->link);

        // make current link old previous link
        chain->link = link;
    }

    // the chain is effectively one link shorter either way
    chain->length --;
}

//------------------------------------------------------------------------|
// Move forward through the chain by a positive number of links
// Return false if ended on the origin node
bool chain_forward(chain_t * chain, size_t index)
{
    while (index > 0)
    {
        chain->link = chain->link->next;
        index--;
    }

    return (chain->link != chain->orig);
}

//------------------------------------------------------------------------|
// Move backward through the chain by a positive number of links
// Return false if ended on the origin node
bool chain_rewind(chain_t * chain, size_t index)
{
    while (index > 0)
    {
        chain->link = chain->link->prev;
        index--;
    }

    return (chain->link != chain->orig);
}

//------------------------------------------------------------------------|
void chain_trim(chain_t * chain)
{
    if (chain->length > 0)
    {
        chain_reset(chain);

        do
        {
            if (!chain->link->data)
            {
                chain->link->data = NULL;
                chain_delete(chain);
                chain_reset(chain);
            }

            chain_forward(chain, 1);
        }
        while (chain->link != chain->orig);
    }
}

//------------------------------------------------------------------------|
// set current link to origin link
void chain_reset(chain_t * chain)
{
    chain->link = chain->orig;
}


//------------------------------------------------------------------------|
// sort list using specified link comparator function pointer
int chain_sort (chain_t *chain, link_compare_func_t compare)
{
    int error = 0;

    // TODO: use qsort on a dynamically allocated array of link pointers
    return 0;
}

//------------------------------------------------------------------------|
// this function expects chain to be a fully opened list with at least
// one link and part should be an opened, but empty list which returns
// with the links from the beginning index and up to, but not including
// the ending index.  the chain as passed in is modified in-place and
// contains whatever is left over.  NOTE: UNTESTED!!
int chain_part (chain_t *chain, chain_t *part, long begin, long end)
{
    int error = 0;
    link_t * link = NULL;

    // remove whatever partition list contains up till now including origin
    error |= chain_clear (part);
    free (part->link);

    // assign some key attributes
    part->length = (unsigned long) end - begin;

    // move from start up to the 'begin' index
    // this becomes the origin link of the partition
    error |= chain_reset (chain);
    error |= chain_move (chain, begin);
    part->link = chain->link;
    part->orig = chain->link;

    // move up to one-after the final link in partition
    // and remember final link there
    chain_move (chain, part->length);
    link = part->link->prev;

    // cut partition from main list & patch up.
    // list is then shorter by the length of the partition
    chain->link->prev->next = part->link;
    part->link->prev = chain->link->prev;
    link->next = chain->link;
    chain->link->prev = link;
    chain->length -= part->length;


    return (error);
}
