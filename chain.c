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
//#include "librmf.h"
//#include "bstr.h"
#include "chain.h"

//------------------------------------------------------------------------|
// NOTE: normally private/static use only.  don't use unless you really
// know what you're doing.  list should handle it's own locking unless
// it's part of a special object that inherits list properties.
int chain_lock (_chain *chain)
{
  int error = ERROR_NONE;

  // reject invalid list objects
  //if ((chain == NULL) || (chain->node == NULL)) {
  if (chain == NULL) { return (ERROR_NULL); }

#ifndef _VLIST_LOCK_DISABLE
  // POSIX threads mutex locking
  error = pthread_mutex_lock (&chain->lock);
#ifdef _VLIST_LOCK_DEBUG
  //_ulist_message (ULIST_MSG_DEBUG, "_ulist_lock () ulist->lock: %d", ulist->lock);
  _ulist_message (ULIST_MSG_DEBUG, "_ulist_lock () error_code: %d", error_code);
  if (ulist->lock == 0) { return (error_code); }
#endif
#endif

  chain->level ++;
  return (error);
}

//------------------------------------------------------------------------|
// NOTE: normally private/static use only.  don't use unless you really
// know what you're doing.  list should handle it's own locking unless
// it's part of a special object that inherits list properties.
int chain_unlock (_chain *chain)
{
  int error = ERROR_NONE;

  // reject invalid list objects
  //if ((chain == NULL) || (chain->node == NULL)) {
  if (chain == NULL) { return (ERROR_NULL); }

#ifndef _VLIST_LOCK_DISABLE
  // POSIX threads mutex locking
  error = pthread_mutex_unlock (&chain->lock);
#endif

  chain->level --;
  return (error);
}

//------------------------------------------------------------------------|
// call this after declaration. NULL name should be ok
int chain_open (_chain *chain, const char *name)
{
  int error = ERROR_NONE;

#ifndef _VLIST_LOCK_DISABLE
  // UPDATE: 7/20/05 initialize mutex flag & get first lock
  pthread_mutexattr_t attr;
  error |= pthread_mutexattr_init (&attr);
  error |= pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE_NP);
  error |= pthread_mutex_init (&chain->lock, &attr);
  error |= pthread_mutexattr_destroy (&attr);
  if (error != ERROR_NONE) { return (error); }
#endif

  // get first lock during open
  chain->level = (size_t) 0;
  error |= chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // initialize components
  error |= chain_init (chain);
  //if (error != ERROR_NONE) { return (error); }
  
  // set list name
  // UPDATE: 8/28/04 caller may use NULL
  error |= _bstr_open (&chain->name);
  error |= _bstr_printf (&chain->name, name);
  
  // allocate initial node of this list
  // and link the origin node to itself...
  // mark origin node for later reset () calls
  chain->node = (_link) malloc (sizeof (struct __link));
  chain->node->next = chain->node;
  chain->node->prev = chain->node;
  chain->orig = chain->node; 

  // NOTE: caller may pass a NULL function pointer if they intend to
  // use a static data type or handle a dynamic data type externally
  // set list node data destructor function...
  // initialize contents of origin node...
  // assign node data size...
  chain->node->data = NULL;
  chain->node->size = (size_t) 0;
  chain->node->type = 0x00000000L;
  chain->node->vnclose = NULL;

  chain_unlock (chain);
  return (error);
}  

//------------------------------------------------------------------------|
// call this last to clear and completely deallocate list
int chain_close (_chain *chain)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // close all nodes except origin - clear all data.
  // remove final node & close list name
  error |= _bstr_close (&chain->name);
  error |= chain_clear (chain);
  free (chain->node);

  // re-initialize static components
  error |= chain_init (chain);

  chain_unlock (chain);
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
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // initialize some static components
  chain->node    = NULL;
  chain->orig    = NULL;
  chain->length  = (size_t) 0;

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// reset an active list back to it's state as if just after open while
// preserving the origin node
int chain_clear (_chain *chain)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // reset list to origin node
  // UPDATE: 1/1/03 move back one to last node
  // this is to avoid deleting the origin node!!!
  error |= chain_reset (chain);
  error |= chain_move (chain, -1);
  
  // delete nodes until last remaining (origin)
  while (chain->node != chain->node->next) {
    chain_del (chain);
  }

  // free final node's data contents
  error |= chain_undata (chain);

  // reset list length
  chain->length = (unsigned long) 0;

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// insert a node after current node and advance to it, alternatively
// adding data to it.  data closure function pointer may be specified
// on a per-node basis or else NULL is fine for data or vnclose
int chain_ins (_chain *chain, void *data, size_t size, _vpfunc1 vnclose)
{
  int error = chain_lock (chain);
  _link node = NULL;
  if (error != ERROR_NONE) { return (error); }

  // only add another node if length is non-zero
  // because length can be zero while origin node exists
  if (chain->length > 0) {
    // create a new node
    node = (_link) malloc (sizeof (struct __link));

    // link new node in between current and next node
    chain->node->next->prev = node;
    node->next = chain->node->next;
    node->prev = chain->node;
    chain->node->next = node;

    // move forward to the new node
    // and initialize new node's contents
    error |= chain_move (chain, (long) 1);
    chain->node->data = NULL;
    chain->node->size = (size_t) 0;
    chain->node->type = 0x00000000L;
    chain->node->vnclose = NULL;
  }

  // UPDATE: add data / alloc mem with every insert
  // increment length either way
  error |= chain_data (chain, data, size, vnclose);
  chain->length ++;

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// delete current node and revert back to the previous node as current
int chain_del (_chain *chain)
{
  int error = chain_lock (chain);
  _link node = NULL;
  if (error != ERROR_NONE) { return (error); }

  // free node's data contents
  error |= chain_undata (chain);

  // only delete node if length is greater than 1
  if (chain->length > 1) {
    // check if we're about to delete the origin node
    if (chain->node == chain->orig) {
      // appropriate thing to do may be to re-assign
      // origin node to the next one before deleting it
      // printf ("DEBUG: WARNING: DELETING ORIGIN NODE\n");
      chain->orig = chain->node->next;
    }

    // remember previous node
    node = chain->node->prev;

    // unlink current node
    chain->node->prev->next = chain->node->next;
    chain->node->next->prev = chain->node->prev;

    // free the current node
    free (chain->node);

    // make current node old previous node
    chain->node = node;
  }

  // decrement list length either way
  chain->length --;

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// rotate list forward or backward by signed index nodes
int chain_move (_chain *chain, long index)
{
  int error = chain_lock (chain);
  long count = 0;
  if (error != ERROR_NONE) { return (error); }

  if (index > 0) {
    // move forward by 'index' nodes
    for (count = 0; count < index; count ++) {
      // increment by one node
      chain->node = chain->node->next;
    }
  } else {
    // move backwards by 'index' nodes
    for (count = 0; count > index; count --) {
      // decrement by one node
      chain->node = chain->node->prev;
    }
  }

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// set current node to origin node
int chain_reset (_chain *chain)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // set current node to origin node
  chain->node = chain->orig;

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// add data to current node.  no deep-copy is performed.  data closure
// function pointer may be specified for garbage collection or NULL
int chain_data (_chain *chain, void *data, size_t size, _vpfunc1 vnclose)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // free existing data package if assigned
  error |= chain_undata (chain);

  // allocate memory for current node's data contents
  // and one-level copy caller's data to the node
  // NOTE: this does NOT handle deep-copy of pointers
  // assign data size and destructor
  if (size == (size_t) 0) {
    chain->node->data = NULL;
  } else {
    chain->node->data = (void *) malloc (size);
    if (chain->node->data == NULL) { return (ERROR_ALLOC); }
  }

  chain->node->size = size;
  //chain->node->type = 0x00000000L;
  chain->node->vnclose = vnclose;
  
  // UPDATE: allow NULL data source -- zero-out node data
  // UPDATE: dest data pointer might be NULL!
  if (chain->node->data != NULL) {
    if (data == NULL) {
      memset (chain->node->data, 0, chain->node->size);
    } else {
      memcpy (chain->node->data, data, chain->node->size);
    }
  }

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// erase all data from current node
int chain_undata (_chain *chain)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // check node data destructor and call if not null
  if ((chain->node != NULL)
    && (chain->node->data != NULL)
    && (chain->node->vnclose != NULL)) {
    error |= chain->node->vnclose ((void *) chain->node->data);
  }

  // check if memory has been allocated for node data.
  // if so, free memory from node's data & reset data pointer
  if (chain->node->data != NULL) {
    free (chain->node->data);
    chain->node->data = NULL;
    chain->node->size = (size_t) 0;
    //chain->node->type = 0x00000000L;
    chain->node->vnclose = NULL;
  }

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// sort list using specified node comparator function pointer
int chain_sort (_chain *chain, _vpfunc2 compare)
{
  int error = chain_lock (chain);
#ifdef _VLIST_HEAPSORT_METHOD
  // heapsort method:
  int insize, nmerges, psize, qsize, i;
  _link p, q, e, tail, oldhead;
#else
  // ALTERNATE APPROACH:
  // NOTE: build an array of pointers to list nodes
  // then just use ANSI qsort on that array with linkcmp
  // then relink the list in the sorted order
  unsigned long *indices = NULL;
  unsigned long count = (unsigned long) 0;
#endif
  if (error != ERROR_NONE) { return (error); }

  // return immediately if unsortable.  be careful to unlock list
  if ((chain->length < 2) || (compare == NULL)) {
    chain_unlock (chain);
    return (_VLIST_ERROR_ARGS);
  }

#ifdef _VLIST_HEAPSORT_METHOD
  // quicksort is a well-known highly-efficient sorting algorithm that is included in
  // the ANSI standard libraries as qsort ().  unfortunately it relies on using an
  // array of elements.  linked lists are a different animal, however.  the heapsort
  // algorithm, as it turns out, is a good choice for lists.  this implementation is
  // an adaption of an adaption from some web page and i have no idea how it works
  insize = 1;
  while (TRUE) {
    p = chain->node;
    oldhead = chain->node;  // only used for circular linkage
    chain->node = NULL;
    tail = NULL;
    nmerges = 0;            // count number of merges we do in this pass

    // if there exists a merge to be done
    while (p != NULL) {
      nmerges ++;
      // step `insize' places along from p
      q = p;
      psize = 0;
      for (i = 0; i < insize; i ++) {
        psize ++;
        if (q->next == oldhead) {
          q = NULL;
        } else {
          q = q->next;
        }  

        if (q == NULL) { break; }
      }

      // if q hasn't fallen off end, we have two lists to merge
      // now we have two lists; merge them
      qsize = insize;
      while ((psize > 0) || ((qsize > 0) && (q != NULL))) {
        // decide whether next element of merge comes from p or q
        if (psize == 0) {
          // NOTE: THIS BLOCK IS THE SAME AS >>>>>
          // p is empty; e must come from q.
          e = q;
          q = q->next;
          qsize --;
          if (q == oldhead) { q = NULL; }
        } else if (((qsize == 0) || (q == NULL)) || ((*compare) (p, q) < 0)) {
          // q is empty; e must come from p.
          e = p;
          p = p->next;
          psize --;
          if (p == oldhead) { p = NULL; }
        } else {
          // >>>>> THIS BLOCK (but don't dare change it or it breaks!)
          // p is empty; e must come from q.
          e = q;
          q = q->next;
          qsize --;
          if (q == oldhead) { q = NULL; }
        }        
        
        // add the next element to the merged list
        if (tail != NULL) {
          tail->next = e;
        } else {
          chain->node = e;
        }

        // Maintain reverse pointers in a doubly linked list.
        e->prev = tail;
        tail = e;
      }

      // now p has stepped `insize' places along, and q has too
      p = q;
    }

    tail->next = chain->node;
    chain->node->prev = tail;

    // if we have done only one merge, we're finished.
    // allow for nmerges == 0, the empty list case
    if (nmerges <= 1) {
      // origin node isn't the origin anymore since we've sorted,
      // so it needs to be reset
      chain->orig = chain->node;
      break;
    }

    // otherwise repeat, merging lists twice the size
    insize *= 2;
  }

#else
  //for (count = 0; count < chain->length; count ++) { }
#endif

  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// this function expects chain to be a fully opened list with at least
// one node and part should be an opened, but empty list which returns
// with the nodes from the beginning index and up to, but not including
// the ending index.  the chain as passed in is modified in-place and
// contains whatever is left over.  NOTE: UNTESTED!!
int chain_part (_chain *chain, _chain *part, long begin, long end)
{
  int error = chain_lock (chain);
  _link node = NULL;
  if (error != ERROR_NONE) { return (error); }

  // also lock partition list, but be careful to unlock chain if error
  error |= chain_lock (part);
  if (error != ERROR_NONE) {
    chain_unlock (chain);
    return (error);
  }

  // remove whatever partition list contains up till now including origin
  error |= chain_clear (part);
  free (part->node);

  // assign some key attributes
  part->length = (unsigned long) end - begin;
  part->level = chain->level;
  
  // move from start up to the 'begin' index
  // this becomes the origin node of the partition
  error |= chain_reset (chain);
  error |= chain_move (chain, begin);
  part->node = chain->node;
  part->orig = chain->node;

  // move up to one-after the final node in partition
  // and remember final node there
  chain_move (chain, part->length);
  node = part->node->prev;

  // cut partition from main list & patch up.
  // list is then shorter by the length of the partition
  chain->node->prev->next = part->node;
  part->node->prev = chain->node->prev;
  node->next = chain->node;
  chain->node->prev = node;
  chain->length -= part->length;

  chain_unlock (part);
  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// use method similar to _mtrx_fsave
// NOTE: BIG ASSUMPTION HERE: this will only work for lists of one type!
int chain_fsave (_chain *chain, _file *file, _vpfunc2 link_fsave)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // unimplemented
  
  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
// use method similar to _mtrx_fload
// NOTE: BIG ASSUMPTION HERE: this will only work for lists of one type!
int chain_fload (_chain *chain, _file *file, _vpfunc2 link_fload)
{
  int error = chain_lock (chain);
  if (error != ERROR_NONE) { return (error); }

  // unimplemented
  
  chain_unlock (chain);
  return (error);
}

//------------------------------------------------------------------------|
int chain_vnclose (void *chain) { return (chain_close (chain)); }
