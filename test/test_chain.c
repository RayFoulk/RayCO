#include "blammo.h"
#include "chain.h"
#include "mut.h"
#include "fixture.h"

#include <string.h>
#include <limits.h>

TESTSUITE_BEGIN

    // because these aren't always used, some warning eaters:
    (void) fixture_reset;
    (void) fixture_report;
    (void) fixture_payload;

TEST_BEGIN("create")
    chain_t * mychain = chain_create(NULL);
    CHECK(mychain != NULL);
    CHECK(mychain->length == 0);
    chain_destroy(mychain);
TEST_END

TEST_BEGIN("insert (heap primitive)")
    int i;
    chain_t * mychain = chain_create(free);
    CHECK(mychain != NULL);
    CHECK(mychain->length == 0);

    for (i = 1; i <= 3; i++)
    {
        // add the nth link: first will be the origin
        // all others will be distinct from the origin
        chain_insert(mychain, malloc(sizeof(int)));
        CHECK(mychain->link != NULL);
        CHECK(mychain->orig != NULL);
        CHECK(mychain->length == i);

        if (i == 1)
        {
            CHECK(mychain->link == mychain->orig);
        }
        else
        {
            CHECK(mychain->link != mychain->orig);
        }

        // add and set some simple data
        CHECK(mychain->link->data != NULL);
        *(int *)mychain->link->data = i;
        CHECK(*(int *)mychain->link->data == i);
    }

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("insert (pointer value / static primitive)")
    size_t i;
    chain_t * mychain = chain_create(NULL);
    CHECK(mychain != NULL);
    CHECK(mychain->length == 0);

    for (i = 1; i <= 3; i++)
    {
        // add the nth link: first will be the origin
        // all others will be distinct from the origin
        chain_insert(mychain, (void *) i);
        CHECK(mychain->link != NULL);
        CHECK(mychain->orig != NULL);
        CHECK(mychain->length == i);

        if (i == 1)
        {
            CHECK(mychain->link == mychain->orig);
        }
        else
        {
            CHECK(mychain->link != mychain->orig);
        }

        // add and set some simple data
        CHECK(mychain->link->data != NULL);
        CHECK(mychain->link->data == (void *) i);
    }

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("reset")
    chain_t * mychain = chain_create(NULL);
    chain_insert(mychain, (void *) 1);
    chain_insert(mychain, (void *) 2);
    chain_insert(mychain, (void *) 3);

    // reset back to origin
    CHECK(mychain->link != mychain->orig);
    chain_reset(mychain);
    CHECK(mychain->link == mychain->orig);
    CHECK(mychain->link->data != NULL);
    CHECK(mychain->link->data == (void *) 1);
    CHECK(mychain->length == 3);

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("seek (forward/rewind)")
    chain_t * mychain = chain_create(NULL);
    chain_insert(mychain, (void *) 1);
    chain_insert(mychain, (void *) 2);
    chain_insert(mychain, (void *) 3);
    chain_reset(mychain);

    // go forward two links
    chain_forward(mychain, 2);
    CHECK(mychain->link != mychain->orig);
    CHECK(mychain->link->data == (void *) 3);

    // rewind one link
    chain_rewind(mychain, 1);
    CHECK(mychain->link != mychain->orig);
    CHECK(mychain->link->data == (void *) 2);

    // going forward two links should be back at origin
    // because of circular property of chains
    chain_forward(mychain, 2);
    CHECK(mychain->link == mychain->orig);
    CHECK(mychain->link->data == (void *) 1);

    // rewind two and should be at index 2
    chain_rewind(mychain, 2);
    CHECK(mychain->link != mychain->orig);
    CHECK(mychain->link->data == (void *) 2);

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("delete")
    chain_t * mychain = chain_create(NULL);
    chain_insert(mychain, (void *) 1);
    chain_insert(mychain, (void *) 2);
    chain_insert(mychain, (void *) 3);
    chain_reset(mychain);
    chain_forward(mychain, 1);

    // delete this link, leaving only 1 & 3
    // should land on 1 just because it is prev
    chain_delete(mychain);
    CHECK(mychain->link->data == (void *) 1);
    CHECK(mychain->length == 2);

    // go forward 1, we should be at 3
    chain_forward(mychain, 1);
    CHECK(mychain->link != mychain->orig);
    CHECK(mychain->link->data == (void *) 3);

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("clear")
    chain_t * mychain = chain_create(NULL);
    chain_insert(mychain, (void *) 1);
    chain_insert(mychain, (void *) 2);
    chain_insert(mychain, (void *) 3);

    // clear all links and data back to original state.
    // NOTE: This will fail after origin node refactor
    chain_clear(mychain);
    CHECK(mychain->length == 0);

#ifdef USE_REFACTORED_ORIG_ALLOC
    CHECK(mychain->link == NULL);
    CHECK(mychain->orig == NULL);
#else
    CHECK(mychain->link != NULL);
    CHECK(mychain->orig != NULL);
    CHECK(mychain->link == mychain->orig);
#endif

    // able to add more nodes after clear
    chain_insert(mychain, (void *) 4);
    chain_insert(mychain, (void *) 5);
    chain_insert(mychain, (void *) 6);
    CHECK(mychain->link != mychain->orig);
    CHECK(mychain->link->data != NULL);
    CHECK(mychain->link->data == (void *) 6);
    CHECK(mychain->length == 3);

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("trim")
    chain_t * mychain = chain_create(free);
    CHECK(mychain != NULL);
    CHECK(mychain->length == 0);

    // create a chain with sparse data
    int i;
    for (i = 0; i < 102; i++)
    {
        chain_insert(mychain, NULL);
        if (i % 3 == 0)
        {
            mychain->link->data = malloc(sizeof(int));
            *(int *)mychain->link->data = i;
        }
    }
    CHECK(mychain->length == 102);

    // now trim out nodes without data
    chain_trim(mychain);
    CHECK(mychain->length == 34);

    // verify sane indexing
    chain_forward(mychain, 33);
    CHECK(*(int *)mychain->link->data == 99);

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("sort")
    int i = 0;
    const size_t ids[] =        { 11, 77, 97, 22, 88, 99, 33, 55, 44, 66 };
    const size_t ids_sorted[] = { 11, 22, 33, 44, 55, 66, 77, 88, 97, 99 };
    chain_t * mychain = chain_create(payload_destroy);

    fixture_reset();
    //fixture_report();

    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        chain_insert(mychain, payload_create(ids[i]));
    }

    chain_sort(mychain, payload_compare);
    
    payload_t * p = NULL;
    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        p = (payload_t *) mychain->link->data;
        //payload_report(p, i);
        CHECK(p->id == ids_sorted[i]);
        CHECK(p->is_created == true);
        CHECK(p->is_destroyed == false);
        chain_forward(mychain, 1);
    }

    chain_destroy(mychain);
TEST_END

TEST_BEGIN("destroy")
    int i = 0;
    payload_t * p = NULL;
    chain_t * mychain = chain_create(payload_destroy);

    fixture_reset();

    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        chain_insert(mychain, payload_create(100 - i));
    }

    //fixture_report();
    chain_destroy(mychain);
    //fixture_report();
    
    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        p = (payload_t *) fixture_payload(i);
        CHECK(p->id == (100 - i));
        CHECK(p->is_created == true);
        CHECK(p->is_destroyed == true);
    }

TEST_END

TEST_BEGIN("copy")
    int i = 0;
    payload_t * p = NULL;
    chain_t * mychain = chain_create(payload_destroy);

    fixture_reset();

    for (i = 0; i < FIXTURE_PAYLOADS / 2; i++)
    { 
        chain_insert(mychain, payload_create(i * 2));
    }
    
    payload_t * optr, * cptr;
    chain_t * mycopy = chain_copy(mychain, payload_copy);
    //fixture_report();
    CHECK(mycopy != NULL)
    CHECK(mycopy != mychain)

    chain_reset(mychain);
    chain_reset(mycopy);
    for (i = 0; i < FIXTURE_PAYLOADS / 2; i++)
    {
        CHECK(mychain->link != mycopy->link);
        optr = (payload_t *) mychain->link->data;
        cptr = (payload_t *) mycopy->link->data;
        CHECK(optr != NULL);
        CHECK(cptr != NULL);
        CHECK(optr != cptr);
        CHECK(optr->id == cptr->id);
        CHECK(optr->is_created == cptr->is_created);
        CHECK(optr->is_destroyed == cptr->is_destroyed);

        //payload_report(i, optr);
        //payload_report(i, cptr);

        chain_forward(mychain, 1);
        chain_forward(mycopy, 1);
    }

    chain_destroy(mychain);
    chain_destroy(mycopy);

    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        p = (payload_t *) fixture_payload(i);
        CHECK(p->is_created == true);
        CHECK(p->is_destroyed == true);
    }

TEST_END

TEST_BEGIN("segment")
    size_t i;
    chain_t * mychain = chain_create(NULL);
    CHECK(mychain != NULL);
    CHECK(mychain->length == 0);

    for (i = 1; i <= 7; i++)
    {
        chain_insert(mychain, (void *) i);
        CHECK(mychain->link != NULL);
        CHECK(mychain->length == i);
        CHECK(mychain->link->data != NULL);
        CHECK(mychain->link->data == (void *) i);
    }

    chain_t * segment = chain_segment(mychain, 4, 7);
    CHECK(segment->length == 3);
    CHECK(mychain->length == 4);
    CHECK(mychain->orig != segment->orig);

    chain_reset(mychain);
    chain_reset(segment);
    for (i = 1; i <= 5; i++)
    {
        CHECK(mychain->link->data != NULL);
        CHECK(mychain->link->data == (void *)
            ((i - 1) % mychain->length + 1));

        CHECK(segment->link->data != NULL);
        CHECK(segment->link->data == (void *)
            ((i - 1) % segment->length + 5));

        chain_forward(mychain, 1);
        chain_forward(segment, 1);
    }

    chain_destroy(mychain);
    chain_destroy(segment);
TEST_END

TEST_BEGIN("splice")
    size_t i;
    chain_t * achain = chain_create(NULL);
    chain_t * bchain = chain_create(NULL);
    CHECK(achain != NULL);
    CHECK(bchain != NULL);
    CHECK(achain->length == 0);
    CHECK(bchain->length == 0);

    for (i = 1; i <= 4; i++)
    {
        chain_insert(achain, (void *) i);
        chain_insert(bchain, (void *) (i + 4));
        CHECK(achain->link != NULL);
        CHECK(achain->length == i);
        CHECK(achain->link->data != NULL);
        CHECK(achain->link->data == (void *) i);
    }

    achain = chain_splice(achain, bchain);
    CHECK(achain->length == 8);

    chain_reset(achain);
    for (i = 1; i <= 8; i++)
    {
        CHECK(achain->link != NULL);
        CHECK(achain->link->data != NULL);
        CHECK(achain->link->data == (void *) i);
        chain_forward(achain, 1);
    }

    chain_destroy(achain);
TEST_END

TESTSUITE_END
