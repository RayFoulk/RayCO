//------------------------------------------------------------------------|
// Copyright (c) 2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

// Python's Dictionary Methods (for reference)
// Method  Description
// clear() Removes all the elements from the dictionary
// copy()  Returns a copy of the dictionary
// fromkeys()  Returns a dictionary with the specified keys and value
// get()   Returns the value of the specified key
// items() Returns a list containing a tuple for each key value pair
// keys()  Returns a list containing the dictionary's keys
// pop()   Removes the element with the specified key
// popitem()   Removes the last inserted key-value pair
// setdefault()    Returns the value of the specified key. If the key does not exist: insert the key, with the specified value
// update()    Updates the dictionary with the specified key-value pairs
// values()    Returns a list of all the values in the dictionary

//------------------------------------------------------------------------|
typedef struct collect_t
{
    struct collect_t * (*create)();

    void (*destroy)(void * collect);


}
collect_t;

//------------------------------------------------------------------------|
extern const collect_t collect_pub;
