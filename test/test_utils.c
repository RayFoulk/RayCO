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

#include "blammo.h"
#include "utils.h"
#include "prng.h"
#include "mut.h"

#include <string.h>
#include <limits.h>

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_utils.log");
    BLAMMO(INFO, "utils tests...");

TEST_BEGIN("test hexdump")
    uint8_t data[64] = { 0 };

    prng_seed(999);
    prng_fill(data, 64);
    hexdump(data, 64, 0);

TEST_END

TEST_BEGIN("test splitstr")
    char * str = "mary had a  little\tlamb";
    const int max_tokens = 5;
    char * tokens[max_tokens];
    int i = 0;

    CHECK(splitstr(tokens, str, " \t"));

    // TODO: replace with individual checks
    for (i = 0; i < max_tokens; i++)
    {
        BLAMMO(DEBUG, "token[%d] is %s", i, tokens[i]);
    }


TEST_END

TESTSUITE_END

