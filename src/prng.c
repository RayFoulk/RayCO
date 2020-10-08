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

// Portions of this file were originally from the following sources:
// http://prng.di.unimi.it/xoroshiro1024plusplus.c
// http://prng.di.unimi.it/splitmix64.c
// Those portions are in the public domain, and follow this comment
// up until the next separator bar.

/*  Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include <stdint.h>

/* This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and 
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush, and it can be useful if
   for some reason you absolutely want 64 bits of state. */

static uint64_t xstate; /* The state can be seeded with any value. */

static uint64_t splitmix64_next()
{
    uint64_t z = (xstate += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

/* This is xoshiro256++ 1.0, one of our all-purpose, rock-solid generators.
   It has excellent (sub-ns) speed, a state (256 bits) that is large
   enough for any parallel application, and it passes all tests we are
   aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill state. */

static uint64_t state[4];

static inline uint64_t rotl(const uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

static uint64_t xoshiro256_next(void)
{
    const uint64_t result = rotl(state[0] + state[3], 23) + state[0];
    const uint64_t tmp = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= tmp;
    state[3] = rotl(state[3], 45);

    return result;
}

//------------------------------------------------------------------------|
// Copyright (c) 2018-2020 by Raymond M. Foulk IV (rfoulk@gmail.com)
// Public interface to above PRNG algorithms
#include "prng.h"

//------------------------------------------------------------------------|
void prng_seed(uint64_t seed)
{
    // Initialize 'split mix 64' state
    xstate = seed;

    // Initialize xoshiro256++ state
    state[0] = splitmix64_next();
    state[1] = splitmix64_next();
    state[2] = splitmix64_next();
    state[3] = splitmix64_next();

    // Mix things up a bit to start
    int i;
    for (i = 0; i < 4096; i++)
    {
        xoshiro256_next();
    }
}

//------------------------------------------------------------------------|
uint64_t prng_next()
{
    return xoshiro256_next();
}

//------------------------------------------------------------------------|
void prng_fill(void * data, size_t size)
{

}
