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
#define FIXTURE_PAYLOADS_PER_TYPE       10
#define FIXTURE_PAYLOAD_BUFFER_SIZE     64

// A simple test fixture payload.  This doesn't use the heap, because the
// goal is to be able to test conditions and inspect state throughout
// creation/descruction.  This is essentially a 'mock' dynamic payload.
typedef struct payload_one_t
{
    int type;

    size_t id;
    bool is_created;
    bool is_destroyed;
    struct payload_one_t * copy_of;

    void (*destroy) (void *);
}
payload_one_t;

//------------------------------------------------------------------------|
// These simulated payload functions emulate a heap-allocated object
// interface, but on the program stack which makes analysis easier.
payload_one_t * payload_one_create(size_t id);
void payload_one_destroy(void * ptr);
int payload_one_compare(const void * a, const void * b);
void * payload_one_copy(const void * p);
void payload_one_report(payload_one_t * p, int i);

//------------------------------------------------------------------------|
// A second type of payload, for testing objects that handle
// heterogeneous payload types.
typedef struct payload_two_t
{
    int type;

    char name[FIXTURE_PAYLOAD_BUFFER_SIZE];
    bool is_created;
    bool is_destroyed;
    struct payload_two_t * copy_of;

    void (*destroy) (void *);
}
payload_two_t;

//------------------------------------------------------------------------|
payload_two_t * payload_two_create(const char * name);
void payload_two_destroy(void * ptr);
void * payload_two_copy(const void * p);
void payload_two_report(payload_two_t * p, int i);

//------------------------------------------------------------------------|
// The test fixture that holds data & state of payloads being tested.
typedef struct
{
    // statically allocate test fixture.payloads on
    // program stack for later runtime post-mortem analysis.
    payload_one_t payload_one[FIXTURE_PAYLOADS_PER_TYPE];
    size_t one_begin, one_end;

    payload_two_t payload_two[FIXTURE_PAYLOADS_PER_TYPE];
    size_t two_begin, two_end;
}
fixture_t;

//------------------------------------------------------------------------|
void fixture_reset();
void fixture_report();
payload_one_t * fixture_payload_one(int i);
payload_two_t * fixture_payload_two(int i);

