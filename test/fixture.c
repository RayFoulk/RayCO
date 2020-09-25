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

#include "blammo.h"
#include "fixture.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

// The test fixture
static fixture_t fixture;

//------------------------------------------------------------------------|
payload_t * payload_create(size_t id)
{
    if (fixture.i >= FIXTURE_PAYLOADS)
    {
        // Simulate malloc fail
        return NULL;
    }

    payload_t * payload = &fixture.payloads[fixture.i++];
    payload->id = id;
    payload->is_created = true;

    // glimpse of future refactoring.
    payload->destroy = payload_destroy;

    return payload;
}

void payload_destroy(void * ptr)
{
    payload_t * payload = (payload_t *) ptr;
    payload->is_destroyed = true;

    if (!payload->copy_of)
    {
        // created by create
        if (fixture.i > 0)
        {
            fixture.i--;
        }
    }
    else
    {
        // created by copy
        if (fixture.j < FIXTURE_PAYLOADS)
        {
            fixture.j++;
        }
    }
}

int payload_compare(const void * a, const void * b)
{
    payload_t * ap = (payload_t *) *(void **) a;
    payload_t * bp = (payload_t *) *(void **) b;

    if (!ap || !bp)
    {
        return INT_MIN;
    }

    return (int) ap->id - (int) bp->id;
}

void * payload_copy(const void * p)
{
    payload_t * orig = (payload_t *) p;
    payload_t * copy = NULL;

    if (!orig || fixture.j == 0)
    {
        return copy;
    }

    copy = &fixture.payloads[fixture.j--];
    // WARNING: Stack copies may not be 'destroyed' properly

    memcpy(copy, orig, sizeof(payload_t));
    copy->copy_of = orig;
    return copy;
}

void payload_report(payload_t * p, int i)
{
    if (!p)
    {
        BLAMMO(DEBUG, "NULL payload!\n");
        return;
    }

    BLAMMO(DEBUG, "payload: %d  id: %zu  created: %s  destroyed: %s\n"
                  "    self: %p  copy_of: %p\n", i, p->id,
                  p->is_created ? "true" : "false",
                  p->is_destroyed ? "true" : "false",
                  p, p->copy_of);
}

//------------------------------------------------------------------------|
void fixture_reset()
{
    memset(fixture.payloads, 0, FIXTURE_PAYLOADS * sizeof(payload_t));
    fixture.i = 0;
    fixture.j = FIXTURE_PAYLOADS - 1;
}

void fixture_report()
{
    int i;
    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        payload_report(&fixture.payloads[i], i);
    }
}

payload_t * fixture_payload(int i)
{
    return &fixture.payloads[i];
}
