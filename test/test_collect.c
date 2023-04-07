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
#include "collect.h"
#include "mut.h"
#include "fixture.h"

#include <string.h>
#include <limits.h>

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_collect.log");
    BLAMMO(INFO, "collect tests...");

    // because these aren't always used, some warning eaters:
    (void) fixture_reset;
    (void) fixture_report;
    (void) fixture_payload;

TEST_BEGIN("create")
    collect_t * collect = collect_pub.create();
    CHECK(collect != NULL);
    CHECK(collect->priv != NULL);
    CHECK(collect->empty(collect));
    CHECK(collect->length(collect) == 0);
    collect->destroy(collect);
TEST_END

TEST_BEGIN("clear")
    collect_t * collect = collect_pub.create();
    CHECK(collect->empty(collect));

    collect->set(collect, "one", (void *) 1, NULL, NULL);
    CHECK(!collect->empty(collect));

    collect->set(collect, "two", (void *) 2, NULL, NULL);
    CHECK(!collect->empty(collect));

    collect->clear(collect);
    CHECK(collect->empty(collect));

    collect->destroy(collect);
TEST_END

TEST_BEGIN("length")
    collect_t * collect = collect_pub.create();
    CHECK(collect->empty(collect));

    collect->set(collect, "one", (void *) 1, NULL, NULL);
    collect->set(collect, "two", (void *) 2, NULL, NULL);
    collect->set(collect, "three", (void *) 3, NULL, NULL);
    CHECK(collect->length(collect) == 3);

    collect->clear(collect);
    CHECK(collect->length(collect) == 0);

    collect->destroy(collect);
TEST_END

TEST_BEGIN("copy")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("get")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("set")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("remove")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("first")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("next")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("keys")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("objects")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TESTSUITE_END
