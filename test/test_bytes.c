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
#include "bytes.h"
#include "mut.h"
//#include "fixture.h"

#include <string.h>
#include <limits.h>

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO(INFO, "bytes tests...");

TEST_BEGIN("create")
    bytes_t * bytes = bytes_create("hello", 12);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);
    CHECK(!bytes->empty(bytes));
    CHECK(bytes->size(bytes) == 12);
    bytes->destroy(bytes);

    bytes = bytes_create(NULL, 0);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);
    CHECK(bytes->empty(bytes));
    CHECK(bytes->size(bytes) == 0);
    bytes->destroy(bytes);

TEST_END

TEST_BEGIN("append")
    bytes_t * bytes = bytes_create("abc", 3);
    bytes_t * suffix = bytes_create("defg", 4);

    CHECK(bytes->size(bytes) == 3);
    CHECK(suffix->size(suffix) == 4);

    bytes->append(bytes, suffix->data(suffix), suffix->size(suffix));

    CHECK(bytes->size(bytes) == 7);
    CHECK(suffix->size(suffix) == 4);
    //BLAMMO(INFO, "bytes->cstr() is %s", bytes->cstr(bytes));
    CHECK(strcmp(bytes->cstr(bytes), "abcdefg") == 0);

    bytes->destroy(bytes);
    suffix->destroy(suffix);

TEST_END

TEST_BEGIN("assign")
    bytes_t * bytes = bytes_create(NULL, 0);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);

    // buffer is intentionally oversized
    const char * astr = "one two three";
    const char * bstr = "one two three";
    const uint8_t data[] = { 'o', 'n', 'e', ' ',
                             't', 'w', 'o', ' ',
                             't', 'h', 'r', 'e',
                             'e',  0,   0,   0,
                              0,   0,   0,   0  };

    bytes->assign(bytes, astr, 20);
    BLAMMO(INFO, "hexdump:\n%s", bytes->hexdump(bytes));

    CHECK(strcmp(bytes->cstr(bytes), bstr) == 0);
    CHECK(bytes->size(bytes) == 20);

    CHECK(memcmp(bytes->data(bytes), data, 20) == 0);
    bytes->destroy(bytes);

TEST_END

TEST_BEGIN("reset")
TEST_END

TEST_BEGIN("seek (forward/rewind)")
TEST_END

TEST_BEGIN("remove")
TEST_END

TEST_BEGIN("clear")
TEST_END

TEST_BEGIN("trim")
TEST_END

TEST_BEGIN("destroy")
TEST_END

TEST_BEGIN("copy")
TEST_END

TEST_BEGIN("split")
TEST_END

TEST_BEGIN("join")
TEST_END

TEST_BEGIN("hexdump")
	bytes_t * bytes = bytes_create("", 37);
	CHECK(bytes != NULL);
	CHECK(bytes->priv != NULL);

	int i;
	for (i = 0; i < 37; i++)
	{

	}

	bytes->destroy(bytes);
TEST_END

TESTSUITE_END

