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

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
#define FIXTURE_PAYLOADS 10

// Test fixture payload.  Don't really actually
// use heap for this, because we want to test
// conditions and inspect state throughout.
// This is more of a 'mock' dynamic payload
typedef struct payload_t
{
    size_t id;
    bool is_created;
    bool is_destroyed;
    struct payload_t * copy_of;

    void (*destroy) (void *);
}
payload_t;

typedef struct
{
    // statically allocate test fixture.payloads on
    // program stack for analysis.
    payload_t payloads[FIXTURE_PAYLOADS];
    size_t i, j;
}
fixture_t;

//------------------------------------------------------------------------|
// These simulated payload functions emulate a heap-allocated object
// interface, but on the program stack which makes analysis easier.
payload_t * payload_create(size_t id);
void payload_destroy(void * ptr);
int payload_compare(const void * a, const void * b);
void * payload_copy(const void * p);
void payload_report(payload_t * p, int i);

//------------------------------------------------------------------------|
void fixture_reset();
void fixture_report();
payload_t * fixture_payload(int i);

