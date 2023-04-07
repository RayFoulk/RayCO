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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "collect.h"
#include "bytes.h"
#include "blammo.h"

//------------------------------------------------------------------------|
// Item container for heterogeneous key/object pair payload
typedef struct collect_object_t
{
    // Pointer to the next container in the collection
    // NULL indicates the end of the collection
    struct collect_object_t * next;

    // Dictionary-style keyword associated with this object
    bytes_t * key;

    // Pointer to the object managed by this collection
    void * object;

    // Object deep-copy function
    object_data_copy_f object_copy;

    // Object destructor function
    object_data_destroy_f object_destroy;
}
collect_item_t;

// Collection private implementation data
typedef struct
{
    // Staring object in the collection.  From a 'stack' perspective,
    // this is the top, although the Collection API doesn't really
    // treat is as such per se.
    collect_item_t * first;

    // Speed optimization more than anything else.  Could also
    // iteratively count all objects upon request, but that consumes
    // cycles by Order-N, whereas tracking the length on each addition
    // or removal of an object to have it on-hand when asked is Order-1.
    size_t length;

    // Some dynamically-sized pointer arrays for array-style iteration.
    char ** keys;
    void ** objects;
}
collect_priv_t;

//------------------------------------------------------------------------|
// Private helper function for get(), set(), and remove()
static collect_item_t * collect_item_find(collect_t * collect,
                                          const char * key,
                                          collect_item_t ** prev)
{
    collect_priv_t * priv = (collect_priv_t *) collect->priv;
    collect_item_t * item = priv->first;
    bytes_t * keybytes = bytes_pub.create(key, strlen(key));

    // Caller may of may not care about previous node
    // Initialize the return pointer if prev is requested
    if (prev) { *prev = NULL; }

    while (item)
    {
        if (item->key->compare(item->key, keybytes) == 0)
        {
            keybytes->destroy(keybytes);
            return item;
        }

        if (prev) { *prev = item; }
        item = item->next;
    }

    keybytes->destroy(keybytes);
    return NULL;
}

//------------------------------------------------------------------------|
// Private helper function for clear(), remove()
static void collect_item_remove(collect_t * collect,
                                collect_item_t * item,
                                collect_item_t * prev)
{
    collect_priv_t * priv = (collect_priv_t *) collect->priv;

    if (!item)
    {
        BLAMMO(DEBUG, "no item to remove!");
        return;
    }

    // Destroy heap objects in container
    if (item->object && item->object_destroy)
    {
        item->object_destroy(item->object);
    }

    item->key->destroy(item->key);

    // Unlink the item from the collection
    if (item == priv->first)
    {
        priv->first = item->next;
    }

    if (prev)
    {
        prev->next = item->next;
    }

    // Wipe memory and delete the container
    memset(item, 0, sizeof(collect_item_t));
    free(item);

    // Size is now one fewer
    priv->length--;
}

//------------------------------------------------------------------------|
static collect_t * collect_create()
{
    // Allocate and initialize public interface
    collect_t * collect = (collect_t *) malloc(sizeof(collect_t));
    if (!collect)
    {
        BLAMMO(FATAL, "malloc(sizeof(collect_t)) failed");
        return NULL;
    }

    // bulk copy all function pointers and init opaque ptr
    memcpy(collect, &collect_pub, sizeof(collect_t));

    // Allocate and initialize private implementation
    collect->priv = malloc(sizeof(collect_priv_t));
    if (!collect->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(collect_priv_t)) failed");
        free(collect);
        return NULL;
    }

    memset(collect->priv, 0, sizeof(collect_priv_t));

    return collect;
}

//------------------------------------------------------------------------|
static void collect_destroy(void * collect_ptr)
{
    collect_t * collect = (collect_t *) collect_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!collect || !collect->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    // remove all objects and destroy their data
    collect->clear(collect);

    // zero out and destroy the private data
    memset(collect->priv, 0, sizeof(collect_priv_t));
    free(collect->priv);

    // zero out and destroy the public interface
    memset(collect, 0, sizeof(collect_t));
    free(collect);
}

//------------------------------------------------------------------------|
static void collect_clear(collect_t * collect)
{
    collect_priv_t * priv = (collect_priv_t *) collect->priv;

    if (priv->objects)
    {
        free(priv->objects);
        priv->objects = NULL;
    }

    if (priv->keys)
    {
        free(priv->keys);
        priv->keys = NULL;
    }

    // Just keep popping off the top until the stack is empty
    while(priv->first)
    {
        collect_item_remove(collect, priv->first, NULL);
    }
}

//------------------------------------------------------------------------|
static inline bool collect_empty(collect_t * collect)
{
    return (NULL == ((collect_priv_t *) collect->priv)->first);
}

//------------------------------------------------------------------------|
static inline size_t collect_length(collect_t * collect)
{
    collect_priv_t * priv = (collect_priv_t *) collect->priv;
    return priv->length;
}

//------------------------------------------------------------------------|
static collect_t * collect_copy(collect_t * collect)
{
    //collect_priv_t * priv = (collect_priv_t *) collect->priv;

    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

//------------------------------------------------------------------------|
static inline void * collect_get(collect_t * collect, const char * key)
{
    collect_item_t * item = collect_item_find(collect, key, NULL);
    return item ? item->object : NULL;
}

//------------------------------------------------------------------------|
static void collect_set(collect_t * collect,
                        const char * key,
                        void * object,
                        object_data_copy_f object_copy,
                        object_data_destroy_f object_destroy)
{
    collect_priv_t * priv = (collect_priv_t *) collect->priv;

    // Search through collection and try to find object with the given key.
    // The whole container is needed, not just the object.
    collect_item_t * item = collect_item_find(collect, key, NULL);
    if (item)
    {
        // If found, destroy the object (if allocated/valid).
        // To make way for the new replacement object.
        if (item->object && item->object_destroy)
        {
            item->object_destroy(item->object);
        }

        // Object and function pointers are left in an assigned non-null
        // state here, but it is now OK to overwrite them, as will be done.
    }
    else
    {
        // If not found, create a new container to hold the object
        item = (collect_item_t *) malloc(sizeof(collect_item_t));
        if (!item)
        {
            BLAMMO(FATAL, "malloc(sizeof(collect_item_t)) failed!");
            return;
        }

        // The object assignment portions below cannot fail,
        // so go ahead and link in the new container
        item->next = priv->first;
        priv->first = item;

        // New key is the one provided, and length is incremented
        // to reflect the new item.
        item->key = bytes_pub.create(key, strlen(key));
        priv->length++;
    }

    // Put the provided object in the container,
    // potentially also overwriting stale pointers.
    item->object = object;
    item->object_copy = object_copy;
    item->object_destroy = object_destroy;
}

//------------------------------------------------------------------------|
static bool collect_remove(collect_t * collect, const char * key)
{
    //collect_priv_t * priv = (collect_priv_t *) collect->priv;
    collect_item_t * prev = NULL;
    collect_item_t * item = collect_item_find(collect, key, &prev);

    // If item was not found, we're done
    if (!item)
    {
        return false;
    }

    collect_item_remove(collect, item, prev);

    return true;
}

//------------------------------------------------------------------------|
static void * collect_first(collect_t * collect,
                            char ** key, void ** object)
{
    collect_priv_t * priv = (collect_priv_t *) collect->priv;

    if (priv->first)
    {
        *key = (char *) priv->first->key->cstr(priv->first->key);
        *object = priv->first->object;
    }
    else
    {
        *key = NULL;
        *object = NULL;
    }

    return (void *) priv->first;
}

//------------------------------------------------------------------------|
static void * collect_next(void * iterator,
                           char ** key, void ** object)
{
    collect_item_t * item = (collect_item_t *) iterator;

    if (item)
    {
        *key = (char *) item->key->cstr(item->key);
        *object = item->object;
    }
    else
    {
        *key = NULL;
        *object = NULL;
    }

    return (void *) item->next;
}

//------------------------------------------------------------------------|
static const char ** collect_keys(collect_t * collect)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

//------------------------------------------------------------------------|
static void ** collect_objects(collect_t * collect)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

//------------------------------------------------------------------------|
const collect_t collect_pub = {
    &collect_create,
    &collect_destroy,
    &collect_clear,
    &collect_empty,
    &collect_length,
    &collect_copy,
    &collect_get,
    &collect_set,
    &collect_remove,
    &collect_first,
    &collect_next,
    &collect_keys,
    &collect_objects,
    NULL
};

















