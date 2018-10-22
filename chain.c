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
#include "chain.h"

//------------------------------------------------------------------------|
// call this after declaration. NULL name should be ok
int chain_open (_chain *chain, const char *name)
{
  int error = 0;

  // initialize components
  error |= chain_init (chain);
  //if (error != 0) { return (error); }
  
  // allocate initial link of this list
  // and link the origin link to itself...
  // mark origin link for later reset () calls
  chain->link = (link_t) malloc (sizeof (struct _link_t));
  chain->link->next = chain->link;
  chain->link->prev = chain->link;
  chain->orig = chain->link;

  // NOTE: caller may pass a NULL function pointer if they intend to
  // use a static data type or handle a dynamic data type externally
  // set list link data destructor function...
  // initialize contents of origin link...
  // assign link data size...
  chain->link->data = NULL;
  chain->link->size = (size_t) 0;
  chain->link->type = 0x00000000L;
  chain->link->vnclose = NULL;


  return (error);
}  

//------------------------------------------------------------------------|
// call this last to clear and completely deallocate list
int chain_close (_chain *chain)
{
  int error = 0;
  if (error != 0) { return (error); }

  // close all links except origin - clear all data.
  // remove final link & close list name
  error |= _bstr_close (&chain->name);
  error |= chain_clear (chain);
  free (chain->link);

  // re-initialize static components
  error |= chain_init (chain);


  chain->level = (size_t) 0;
#ifndef _VLIST_LOCK_DISABLE
  error |= pthread_mutex_destroy (&chain->lock);
#endif
  return (error);
}

//------------------------------------------------------------------------|
// NOTE: normally private/static use only.  don't use unless you really
// know what you're doing.  this callously blasts/initializes the list!
int chain_init (_chain *chain)
{
  int error = 0;
  if (error != 0) { return (error); }

  // initialize some static components
  chain->link    = NULL;
  chain->orig    = NULL;
  chain->length  = (size_t) 0;


  return (error);
}

//------------------------------------------------------------------------|
// reset an active list back to it's state as if just after open while
// preserving the origin link
int chain_clear (_chain *chain)
{
  int error = 0;
  if (error != 0) { return (error); }

  // reset list to origin link
  // UPDATE: 1/1/03 move back one to last link
  // this is to avoid deleting the origin link!!!
  error |= chain_reset (chain);
  error |= chain_move (chain, -1);
  
  // delete links until last remaining (origin)
  while (chain->link != chain->link->next) {
    chain_del (chain);
  }

  // free final link's data contents
  error |= chain_undata (chain);

  // reset list length
  chain->length = (unsigned long) 0;


  return (error);
}

//------------------------------------------------------------------------|
// insert a link after current link and advance to it, alternatively
// adding data to it.  data closure function pointer may be specified
// on a per-link basis or else NULL is fine for data or vnclose
int chain_ins (_chain *chain, void *data, size_t size, _vpfunc1 vnclose)
{
  int error = 0;
  link_t link = NULL;
  if (error != 0) { return (error); }

  // only add another link if length is non-zero
  // because length can be zero while origin link exists
  if (chain->length > 0) {
    // create a new link
    link = (link_t) malloc (sizeof (struct _link_t));

    // link new link in between current and next link
    chain->link->next->prev = link;
    link->next = chain->link->next;
    link->prev = chain->link;
    chain->link->next = link;

    // move forward to the new link
    // and initialize new link's contents
    error |= chain_move (chain, (long) 1);
    chain->link->data = NULL;
    chain->link->size = (size_t) 0;
    chain->link->type = 0x00000000L;
    chain->link->vnclose = NULL;
  }

  // UPDATE: add data / alloc mem with every insert
  // increment length either way
  error |= chain_data (chain, data, size, vnclose);
  chain->length ++;


  return (error);
}

//------------------------------------------------------------------------|
// delete current link and revert back to the previous link as current
int chain_del (_chain *chain)
{
  int error = 0;
  link_t link = NULL;
  if (error != 0) { return (error); }

  // free link's data contents
  error |= chain_undata (chain);

  // only delete link if length is greater than 1
  if (chain->length > 1) {
    // check if we're about to delete the origin link
    if (chain->link == chain->orig) {
      // appropriate thing to do may be to re-assign
      // origin link to the next one before deleting it
      // printf ("DEBUG: WARNING: DELETING ORIGIN link\n");
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

  // decrement list length either way
  chain->length --;


  return (error);
}

//------------------------------------------------------------------------|
// rotate list forward or backward by signed index links
int chain_move (_chain *chain, long index)
{
  int error = 0;
  long count = 0;
  if (error != 0) { return (error); }

  if (index > 0) {
    // move forward by 'index' links
    for (count = 0; count < index; count ++) {
      // increment by one link
      chain->link = chain->link->next;
    }
  } else {
    // move backwards by 'index' links
    for (count = 0; count > index; count --) {
      // decrement by one link
      chain->link = chain->link->prev;
    }
  }


  return (error);
}

//------------------------------------------------------------------------|
// set current link to origin link
int chain_reset (_chain *chain)
{
  int error = 0;
  if (error != 0) { return (error); }

  // set current link to origin link
  chain->link = chain->orig;


  return (error);
}

//------------------------------------------------------------------------|
// add data to current link.  no deep-copy is performed.  data closure
// function pointer may be specified for garbage collection or NULL
int chain_data (_chain *chain, void *data, size_t size, _vpfunc1 vnclose)
{
  int error = 0;
  if (error != 0) { return (error); }

  // free existing data package if assigned
  error |= chain_undata (chain);

  // allocate memory for current link's data contents
  // and one-level copy caller's data to the link
  // NOTE: this does NOT handle deep-copy of pointers
  // assign data size and destructor
  if (size == (size_t) 0) {
    chain->link->data = NULL;
  } else {
    chain->link->data = (void *) malloc (size);
    if (chain->link->data == NULL) { return (ERROR_ALLOC); }
  }

  chain->link->size = size;
  //chain->link->type = 0x00000000L;
  chain->link->vnclose = vnclose;
  
  // UPDATE: allow NULL data source -- zero-out link data
  // UPDATE: dest data pointer might be NULL!
  if (chain->link->data != NULL) {
    if (data == NULL) {
      memset (chain->link->data, 0, chain->link->size);
    } else {
      memcpy (chain->link->data, data, chain->link->size);
    }
  }


  return (error);
}

//------------------------------------------------------------------------|
// erase all data from current link
int chain_undata (_chain *chain)
{
  int error = 0;
  if (error != 0) { return (error); }

  // check link data destructor and call if not null
  if ((chain->link != NULL)
    && (chain->link->data != NULL)
    && (chain->link->vnclose != NULL)) {
    error |= chain->link->vnclose ((void *) chain->link->data);
  }

  // check if memory has been allocated for link data.
  // if so, free memory from link's data & reset data pointer
  if (chain->link->data != NULL) {
    free (chain->link->data);
    chain->link->data = NULL;
    chain->link->size = (size_t) 0;
    //chain->link->type = 0x00000000L;
    chain->link->vnclose = NULL;
  }


  return (error);
}

//------------------------------------------------------------------------|
// sort list using specified link comparator function pointer
int chain_sort (_chain *chain, _vpfunc2 compare)
{
  int error = 0;

  return (error);
}

//------------------------------------------------------------------------|
// this function expects chain to be a fully opened list with at least
// one link and part should be an opened, but empty list which returns
// with the links from the beginning index and up to, but not including
// the ending index.  the chain as passed in is modified in-place and
// contains whatever is left over.  NOTE: UNTESTED!!
int chain_part (_chain *chain, _chain *part, long begin, long end)
{
  int error = 0;
  link_t link = NULL;
  if (error != 0) { return (error); }

  // also lock partition list, but be careful to unlock chain if error
  error |= chain_lock (part);
  if (error != 0) {

    return (error);
  }

  // remove whatever partition list contains up till now including origin
  error |= chain_clear (part);
  free (part->link);

  // assign some key attributes
  part->length = (unsigned long) end - begin;
  part->level = chain->level;
  
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

  chain_unlock (part);
  
  return (error);
}
