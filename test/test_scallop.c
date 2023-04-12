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
#include "console.h"
#include "scallop.h"
#include "mut.h"

#include <string.h>
#include <limits.h>
#include <stdbool.h>

static bool dummy_registration_func(void * scallop)
{
    BLAMMO(INFO, "got called!");
    return true;
}

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_scallop.log");
    BLAMMO(INFO, "scallop tests...");

TEST_BEGIN("test create/destroy")
    console_t * console = console_pub.create(stdin, stdout, "test-history.txt");
    CHECK(console != NULL);

    scallop_t * scallop = scallop_pub.create(console,
                                             dummy_registration_func,
                                             "TEST");
    CHECK(scallop != NULL);

    scallop->destroy(scallop);

TEST_END

TEST_BEGIN("test register/unregister")
    CHECK(true);
TEST_END

TEST_BEGIN("test destroy")
    CHECK(true);
TEST_END

TESTSUITE_END

