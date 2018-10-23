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

#pragma once

//------------------------------------------------------------------------|

typedef struct link_t
{
  struct link_t *next;    // pointer to next node or null if end
  struct link_t *prev;    // pointer to previous node or null if beginning
  void *data;             // pointer to node's contents,
                          // implementer is responsible for size
}
link_t;

typedef struct
{
  link_t * link;          // current link in chain
  link_t * orig;          // origin link in chain
  unsigned long length;   // list length
  _vpfunc1 vnclose;       // node data destructor
}
chain_t;

//------------------------------------------------------------------------|
int chain_open (chain_t *, const char *);	// initialize list instance
int chain_close (chain_t *);			// completely deallocate list

int chain_vnclose (void *);

int chain_init (chain_t *);			// initilize static components
int chain_clear (chain_t *);			// revert state to after 'open'

int chain_ins (chain_t *, void *, size_t, _vpfunc1); // add node after current
int chain_del (chain_t *);			// delete current node

int chain_move (chain_t *, long);		// rewind/forward a list
int chain_reset (chain_t *);			// reset to origin node

int chain_data (chain_t *, void *, size_t, _vpfunc1); // add data to node
int chain_undata (chain_t *);			// clear data from current node

int chain_sort (chain_t *, _vpfunc2);		// sort using comparator
int chain_part (chain_t *, chain_t *, long, long); // partition list into 2


#endif
