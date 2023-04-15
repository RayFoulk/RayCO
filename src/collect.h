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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
// A collection consists of a set of heterogeneous objects.  For brevity,
// comparator functions are (currently) not supported with this object.
// This may be re-evaluated at a later time if it ever becomes necessary
// to sort() a collection.
#include "utils.h"          // generic function signatures

//------------------------------------------------------------------------|
typedef struct collect_t
{
    // Collection factory function
    // TODO: consider char **, void ** initializers
    struct collect_t * (*create)();

    // Collection destructor function
    void (*destroy)(void * collect);

    // Empty the collection: Removes all items and destroys their data.
    void (*clear)(struct collect_t * collect);

    // Whether the collection is empty or not
    bool (*empty)(struct collect_t * collect);

    // Gets the length of the collection: the number of objects stored.
    size_t (*length)(struct collect_t * collect);

    // Produce a full deep copy of the collection, including individual
    // objects within the collection.
    struct collect_t * (*copy)(struct collect_t * collect);

    // Get an object by keyword.  Returns NULL if it doesn't exist.
    // NOTE: Payload type determining information has been left ENTIRELY
    // up the user.  C has no typeof() or reflection, which presents a
    // challenge, but there are a few solutions.  For example:
    //   1.) Holding off the necessity of doing so for as long as possible,
    //   such as by using type-specific callbacks (or subscriptions).
    //   This is the approach generally used throughout RayCO.
    //   2.) Putting a type identifier/signature at the beginning of each
    //   structure to be used within a collection (or a certain API).
    //   This is the approach taken by the legacy C socket library), and
    //   then just handle the rest with type-casting.
    //   2.b.) With an object that has methods in a public interface,
    //   the first method could be a "get_type()" with a consistent
    //   signature, for example.
    //   3.) Going back to making all payloads appear to be homogeneous,
    //   but then use an internal type identifier with a union payload.
    // Objects may also be made quasi-polymorphic through composition,
    // ownership of parent objects, and then manually overriding
    // certain interface methods.
    void * (*get)(struct collect_t * collect, const char * key);

    // Set an object.  Adds it to the collection if it does not exist,
    // or else destroys and overwrites the previous object instance
    // if it does.
    void (*set)(struct collect_t * collect,
                const char * key,
                void * object,
                generic_copy_f object_copy,
                generic_destroy_f object_destroy);

    // Remove a specific object entry by key.
    // returns true if the object was found and removed,
    // or false if the object was not found
    bool (*remove)(struct collect_t * collect, const char * key);

    // Starting iterator for the collection.  Returns an iterator pointer
    // to be used in subsequent calls to 'next', and sets the
    // 'key' and 'object' pointers to the first key/object in the collection
    void * (*first)(struct collect_t * collect,
                    char ** key, void ** object);

    // Regular forward iterator function for the collection.  Pass in
    // the iterator pointer from first or a previous call to next.
    // key/object are set to the current item in the collection
    void * (*next)(void * iterator,
                   char ** key, void ** object);

    // Returns a linear array of all keys associated with objects, NULL
    // terminated.  Order is top-first, bottom-last.
    const char ** (*keys)(struct collect_t * collect);

    // Returns a linear array of all object pointers, NULL terminated.
    // Order is top-first, bottom-last.
    void ** (*objects)(struct collect_t * collect);

    // Private data
    void * priv;
}
collect_t;

//------------------------------------------------------------------------|
extern const collect_t collect_pub;
