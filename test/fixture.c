//------------------------------------------------------------------------|
// Copyright (c) 2018-2020 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

#include "fixture.h"
#include "utils.h"
#include "blammo.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

// The test fixture
static fixture_t fixture;

//------------------------------------------------------------------------|
payload_one_t * payload_one_create(size_t id)
{
    if (fixture.one_begin >= FIXTURE_PAYLOADS_PER_TYPE)
    {
        // Simulate malloc fail
        return NULL;
    }

    payload_one_t * payload = &fixture.payload_one[fixture.one_begin++];
    payload->type = 1;
    payload->id = id;
    payload->is_created = true;

    // glimpse of future refactoring.
    payload->destroy = payload_one_destroy;

    BLAMMO(DEBUG, "created payload_one_t %p", payload);
    return payload;
}

void payload_one_destroy(void * ptr)
{
    payload_one_t * payload = (payload_one_t *) ptr;
    payload->is_destroyed = true;

    if (!payload->copy_of)
    {
        // created by create
        if (fixture.one_begin > 0)
        {
            fixture.one_begin--;
        }
    }
    else
    {
        // created by copy
        if (fixture.one_end < FIXTURE_PAYLOADS_PER_TYPE)
        {
            fixture.one_end++;
        }
    }
}

int payload_one_compare(const void * a, const void * b)
{
    payload_one_t * ap = (payload_one_t *) *(void **) a;
    payload_one_t * bp = (payload_one_t *) *(void **) b;

    if (!ap || !bp)
    {
        return INT_MIN;
    }

    BLAMMO(DEBUG, "comparing ptr %p to %p, payload_one_t %p to %p", a, b, ap, bp);
    return (int) ap->id - (int) bp->id;
}

void * payload_one_copy(const void * p)
{
    payload_one_t * orig = (payload_one_t *) p;
    payload_one_t * copy = NULL;

    if (!orig || fixture.one_end == 0)
    {
        return copy;
    }

    copy = &fixture.payload_one[fixture.one_end--];
    // WARNING: Stack copies may not be 'destroyed' properly

    memcpy(copy, orig, sizeof(payload_one_t));
    copy->copy_of = orig;

    BLAMMO(DEBUG, "copied payload_one_t %p to %p", p, copy);
    return copy;
}

void payload_one_report(payload_one_t * p, int i)
{
    if (!p)
    {
        BLAMMO(DEBUG, "NULL payload!\n");
        return;
    }

    BLAMMO(DEBUG, "payload: %d  type: %d  id: %zu\n"
                  "    created: %s  destroyed: %s\n"
                  "    self: %p  copy_of: %p\n",
                  i,
                  p->type,
                  p->id,
                  p->is_created ? "true" : "false",
                  p->is_destroyed ? "true" : "false",
                  p, p->copy_of);
}

//------------------------------------------------------------------------|
payload_two_t * payload_two_create(const char * name)
{
    if (fixture.two_begin >= FIXTURE_PAYLOADS_PER_TYPE)
    {
        // Simulate malloc fail
        return NULL;
    }

    payload_two_t * payload = &fixture.payload_two[fixture.two_begin++];
    payload->type = 2;
    strcpy(payload->name, name);
    payload->is_created = true;

    // glimpse of future refactoring.
    payload->destroy = payload_two_destroy;

    return payload;
}

void payload_two_destroy(void * ptr)
{
    payload_two_t * payload = (payload_two_t *) ptr;
    payload->is_destroyed = true;

    if (!payload->copy_of)
    {
        // created by create
        if (fixture.two_begin > 0)
        {
            fixture.two_begin--;
        }
    }
    else
    {
        // created by copy
        if (fixture.two_end < FIXTURE_PAYLOADS_PER_TYPE)
        {
            fixture.two_end++;
        }
    }
}

void * payload_two_copy(const void * p)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return NULL;
}

void payload_two_report(payload_two_t * p, int i)
{
    if (!p)
    {
        BLAMMO(DEBUG, "NULL payload!\n");
        return;
    }

    BLAMMO(DEBUG, "payload: %d  type: %d  name: %s\n"
                  "    created: %s  destroyed: %s\n"
                  "    self: %p  copy_of: %p\n",
                  i,
                  p->type,
                  p->name,
                  p->is_created ? "true" : "false",
                  p->is_destroyed ? "true" : "false",
                  p, p->copy_of);
}

//------------------------------------------------------------------------|
void fixture_reset()
{
    memzero(fixture.payload_one, FIXTURE_PAYLOADS_PER_TYPE * sizeof(payload_one_t));
    fixture.one_begin = 0;
    fixture.one_end = FIXTURE_PAYLOADS_PER_TYPE - 1;

    memzero(fixture.payload_two, FIXTURE_PAYLOADS_PER_TYPE * sizeof(payload_two_t));
    fixture.two_begin = 0;
    fixture.two_end = FIXTURE_PAYLOADS_PER_TYPE - 1;
}

void fixture_report()
{
    int i;

    BLAMMO(DEBUG, "payload_one_t array:");
    for (i = 0; i < FIXTURE_PAYLOADS_PER_TYPE; i++)
    {
        payload_one_report(&fixture.payload_one[i], i);
    }

    BLAMMO(DEBUG, "payload_two_t array:");
    for (i = 0; i < FIXTURE_PAYLOADS_PER_TYPE; i++)
    {
        payload_two_report(&fixture.payload_two[i], i);
    }

}

payload_one_t * fixture_payload_one(int i)
{
    return &fixture.payload_one[i];
}

payload_two_t * fixture_payload_two(int i)
{
    return &fixture.payload_two[i];
}
