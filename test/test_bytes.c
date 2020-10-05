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

TEST_BEGIN("assign")
    bytes_t * bytes = bytes_create(NULL, 0);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);

    // buffer is intentionally oversized
    const char * astr = "qwertyuiop asdfghjkl";
    const char * bstr = "qwertyuiop asdfghjkl";
    bytes->assign(bytes, astr, 32);
    CHECK(strcmp(bytes->cstr(bytes), bstr) == 0);
    CHECK(bytes->size(bytes) == 32);

    bytes->destroy(bytes);

TEST_END

TEST_BEGIN("insert (pointer value / static primitive)")
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

TESTSUITE_END

