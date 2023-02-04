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
#include <stdbool.h>

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
    CHECK(true);

TEST_END

TEST_BEGIN("test splitstr")

    // The string to be parsed MUST be mutable
    // and even just declaring 'char *' causes the
    // compiler to put it in read-only constants
    const char * const_str = "mary had\t\ta  little\tlamb   ";
    const int max_tokens = 5;
    char * tokens[max_tokens];
    size_t i = 0;
    char * str = strdup(const_str);

    i = splitstr(tokens, max_tokens, str, " \t");

    CHECK(i == 5);
    CHECK(strcmp(tokens[0], "mary") == 0)
    CHECK(strcmp(tokens[1], "had") == 0)
    CHECK(strcmp(tokens[2], "a") == 0)
    CHECK(strcmp(tokens[3], "little") == 0)
    CHECK(strcmp(tokens[4], "lamb") == 0)

    free(str);

TEST_END

TEST_BEGIN("test markstr")

    const char * const_str = "mary had\t\ta  little\tlamb   ";
    const int max_markers = 5;
    char * markers[max_markers];
    size_t nmark = 0;

    nmark = markstr(markers, max_markers, const_str, " \t");
    CHECK(nmark == 5);
    CHECK(strncmp(markers[0], "mary", 4) == 0)
    CHECK(strncmp(markers[1], "had", 3) == 0)
    CHECK(strncmp(markers[2], "a", 1) == 0)
    CHECK(strncmp(markers[3], "little", 6) == 0)
    CHECK(strncmp(markers[4], "lamb", 4) == 0)


    memset(markers, 0, sizeof(markers));
    const char * another = " mary had a \t  little lamb";

    nmark = markstr(markers, max_markers, another, " \t");
    CHECK(nmark == 5);
    CHECK(strncmp(markers[0], "mary", 4) == 0)
    CHECK(strncmp(markers[1], "had", 3) == 0)
    CHECK(strncmp(markers[2], "a", 1) == 0)
    CHECK(strncmp(markers[3], "little", 6) == 0)
    CHECK(strncmp(markers[4], "lamb", 4) == 0)

    memset(markers, 0, sizeof(markers));
    const char * yet_another = " <crash> <happy>";

    nmark = markstr(markers, max_markers, yet_another, " ");
    CHECK(nmark == 2);

TEST_END

TESTSUITE_END

