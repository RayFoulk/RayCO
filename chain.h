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

typedef struct __link {
  struct __link *next;				// address of next node
  struct __link *prev;				// address of previous node
  void *data;					// pointer to node's contents
  size_t size;					// node data size
  long type;					// data type (IMPLEMENTATION-SPECIFIC)
  _vpfunc1 vnclose;				// node data destructor
  //pthread_t tid;
} *_link;

typedef struct {
  _bstr name;					// name of this list
  _link node;					// current node
  _link orig;					// origin node for reset
  unsigned long length;				// list length
  pthread_mutex_t lock;				// pthread mutex lock
  size_t level;					// lock depth
} _chain;

//------------------------------------------------------------------------|
int chain_lock (_chain *);			// use carefully!
int chain_unlock (_chain *);			// use carefully!
int chain_open (_chain *, const char *);	// initialize list instance
int chain_close (_chain *);			// completely deallocate list
int chain_init (_chain *);			// initilize static components
int chain_clear (_chain *);			// revert state to after 'open'
int chain_ins (_chain *, void *, size_t, _vpfunc1); // add node after current
int chain_del (_chain *);			// delete current node
int chain_move (_chain *, long);		// rewind/forward a list
int chain_reset (_chain *);			// reset to origin node
int chain_data (_chain *, void *, size_t, _vpfunc1); // add data to node
int chain_undata (_chain *);			// clear data from current node
int chain_sort (_chain *, _vpfunc2);		// sort using comparator
int chain_part (_chain *, _chain *, long, long); // partition list into 2
int chain_fsave (_chain *, _file *, _vpfunc2); // save to file
int chain_fload (_chain *, _file *, _vpfunc2); // load from file
int chain_vnclose (void *);

#endif
