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
    BLAMMO_LEVEL(DEBUG);
    BLAMMO_FILE("test_collect.log");
    BLAMMO(INFO, "collect tests...");

    // because these aren't always used, some warning eaters:
    (void) fixture_reset;
    (void) fixture_report;
    (void) fixture_payload_one;

TEST_BEGIN("create")
    collect_t * collect = collect_pub.create();
    CHECK(collect != NULL);
    CHECK(collect->priv != NULL);
    CHECK(collect->empty(collect));
    CHECK(collect->length(collect) == 0);
    collect->destroy(collect);
TEST_END

TEST_BEGIN("empty, clear")
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

TEST_BEGIN("length, set (scalar)")
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

TEST_BEGIN("get (scalar), set (scalar)")
    collect_t * collect = collect_pub.create();
    collect->set(collect, "one", (void *) 1, NULL, NULL);
    collect->set(collect, "two", (void *) 2, NULL, NULL);
    collect->set(collect, "three", (void *) 3, NULL, NULL);

    CHECK(collect->get(collect, "two") == (void *) 2);

    collect->destroy(collect);
TEST_END

TEST_BEGIN("set & get (heterogeneous heap objects)")

    fixture_reset();

    collect_t * collect = collect_pub.create();
    collect->set(collect, "one", payload_one_create(1),
                                 payload_one_copy,
                                 payload_one_destroy);

    collect->set(collect, "two", payload_two_create("bravo"),
                                 payload_two_copy,
                                 payload_two_destroy);

    collect->set(collect, "three", payload_one_create(3),
                                   payload_one_copy,
                                   payload_one_destroy);

    collect->set(collect, "four", payload_two_create("delta"),
                                  payload_two_copy,
                                  payload_two_destroy);
    CHECK(collect->length(collect) == 4);

    // Dummy payload used to identify the real payload type
    typedef struct
    {
        int type;
    }
    dummy_t;

    CHECK(collect->get(collect, "two") != NULL);
    dummy_t * dummy = collect->get(collect, "two");
    CHECK(dummy->type == 2);
    payload_two_t * p2 = (payload_two_t *) dummy;
    CHECK(strcmp(p2->name, "bravo") == 0);

    CHECK(collect->get(collect, "three") != NULL);
    dummy = collect->get(collect, "three");
    CHECK(dummy->type == 1);
    payload_one_t * p1 = (payload_one_t *) dummy;
    CHECK(p1->id == 3);

    collect->destroy(collect);

    //fixture_report();

TEST_END

TEST_BEGIN("set overwrite/update")
BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("copy")
    BLAMMO(ERROR, "FIXME: TEST NOT IMPLEMENTED");
//    fixture_reset();
//    fixture_report();
TEST_END

TEST_BEGIN("remove, specific item")

    fixture_reset();

    const char * keys[] = {
        "one",
        "two",
        "three",
        "four",
        "five",
        NULL
    };

    collect_t * collect = collect_pub.create();
    int i = 0;

    for (i = 0; i < 5; i++)
    {
        collect->set(collect, keys[i], payload_one_create(i),
                                       payload_one_copy,
                                       payload_one_destroy);
    }

    CHECK(collect->get(collect, "three") != NULL);
    collect->remove(collect, "three");
    CHECK(collect->get(collect, "three") == NULL);
    CHECK(collect->length(collect) == 4);
    //fixture_report();

    payload_one_t * p1 = fixture_payload_one(2);
    CHECK(p1->is_destroyed == true);
    collect->destroy(collect);

TEST_END

TEST_BEGIN("remove, nonexisting item")
    BLAMMO(ERROR, "NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("first")

    const char * keys[] = {
        "one",
        "two",
        "three",
        NULL
    };

    collect_t * collect = collect_pub.create();
    size_t i = 0;

    char * key = NULL;
    void * object = NULL;
    void * iterator = collect->first(collect, &key, &object);
    CHECK(key == NULL);
    CHECK(object == NULL);
    CHECK(iterator == NULL);

    while (keys[i])
    {
        collect->set(collect, keys[i], (void *) (i + 1), NULL, NULL);
        i++;
    }

    iterator = collect->first(collect, &key, &object);
    CHECK(strcmp(key, "three") == 0);
    CHECK(object == (void *) 3);
    CHECK(iterator != NULL);

    collect->destroy(collect);
TEST_END

TEST_BEGIN("next")

    const char * keys[] = {
        "one",
        "two",
        "three",
        NULL
    };

    collect_t * collect = collect_pub.create();
    size_t i = 0;

    while (keys[i])
    {
        collect->set(collect, keys[i], (void *) (i + 1), NULL, NULL);
        i++;
    }

    char * key = NULL;
    void * object = NULL;
    void * iterator = collect->first(collect, &key, &object);

    i = 2;
    while (iterator)
    {
        BLAMMO(DEBUG, "iterator: %p  key: %s  object: %p",
                iterator, key, object);

        CHECK(strcmp(key, keys[i]) == 0);
        CHECK(object == (void *) (i + 1));
        i--;

        iterator = collect->next(iterator, &key, &object);
    }

    collect->destroy(collect);
TEST_END

TEST_BEGIN("keys & objects")
    collect_t * collect = collect_pub.create();

    collect->set(collect, "one", (void *) 1, NULL, NULL);
    collect->set(collect, "two", (void *) 2, NULL, NULL);
    collect->set(collect, "three", (void *) 3, NULL, NULL);

    const char ** keys = collect->keys(collect);
    CHECK(strcmp("three", keys[0]) == 0);
    CHECK(strcmp("two", keys[1]) == 0);
    CHECK(strcmp("one", keys[2]) == 0);

    void ** objects = collect->objects(collect);
    CHECK((void *) 3 == objects[0]);
    CHECK((void *) 2 == objects[1]);
    CHECK((void *) 1 == objects[2]);

    collect->destroy(collect);

TEST_END

TESTSUITE_END
