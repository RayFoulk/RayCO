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
    chain_t * chain = chain_create(NULL);
    CHECK(chain != NULL);
    CHECK(chain->priv != NULL);
    CHECK(chain->empty(chain));
    CHECK(chain->orig(chain));
    CHECK(chain->length(chain) == 0);
    chain->destroy(chain);
TEST_END

TEST_BEGIN("insert (heap primitive)")
    int i;
    chain_t * chain = chain_create(free);
    CHECK(chain != NULL);
    CHECK(chain->priv != NULL);
    CHECK(chain->length(chain) == 0);

    for (i = 1; i <= 3; i++)
    {
        // add the nth link: first will be the origin
        // all others will be distinct from the origin
        chain->insert(chain, malloc(sizeof(int)));
        CHECK(!chain->empty(chain));
        CHECK(chain->length(chain) == i);

        if (i == 1)
        {
            CHECK(chain->orig(chain));
        }
        else
        {
            CHECK(!chain->orig(chain));
        }

        // add and set some simple data
        CHECK(chain->data(chain) != NULL);
        *(int *)chain->data(chain) = i;
        CHECK(*(int *)chain->data(chain) == i);
    }

    chain->destroy(chain);
TEST_END

TEST_BEGIN("insert (pointer value / static primitive)")
    size_t i;
    chain_t * chain = chain_create(NULL);
    CHECK(chain != NULL);
    CHECK(chain->length(chain) == 0);

    for (i = 1; i <= 3; i++)
    {
        // add the nth link: first will be the origin
        // all others will be distinct from the origin
        chain->insert(chain, (void *) i);
        CHECK(!chain->empty(chain));
        CHECK(chain->length(chain) == i);

        if (i == 1)
        {
            CHECK(chain->orig(chain));
        }
        else
        {
            CHECK(!chain->orig(chain));
        }

        // add and set some simple data
        CHECK(chain->data(chain) != NULL);
        CHECK(chain->data(chain) == (void *) i);
    }

    chain->destroy(chain);
TEST_END

TEST_BEGIN("reset")
    chain_t * chain = chain_create(NULL);
    chain->insert(chain, (void *) 1);
    chain->insert(chain, (void *) 2);
    chain->insert(chain, (void *) 3);

    // reset back to origin
    CHECK(!chain->orig(chain));
    chain->reset(chain);
    CHECK(chain->orig(chain));
    CHECK(chain->data(chain) != NULL);
    CHECK(chain->data(chain) == (void *) 1);
    CHECK(chain->length(chain) == 3);

    chain->destroy(chain);
TEST_END

TEST_BEGIN("seek (forward/rewind)")
    chain_t * chain = chain_create(NULL);
    chain->insert(chain, (void *) 1);
    chain->insert(chain, (void *) 2);
    chain->insert(chain, (void *) 3);
    chain->reset(chain);

    // go forward two links
    chain->spin(chain, 2);
    CHECK(!chain->orig(chain));
    CHECK(chain->data(chain) == (void *) 3);

    // rewind one link
    chain->spin(chain, -1);
    CHECK(!chain->orig(chain));
    CHECK(chain->data(chain) == (void *) 2);

    // going forward two links should be back at origin
    // because of circular property of chains
    chain->spin(chain, 2);
    CHECK(chain->orig(chain));
    CHECK(chain->data(chain) == (void *) 1);

    // rewind two and should be at index 2
    chain->spin(chain, -2);
    CHECK(!chain->orig(chain));
    CHECK(chain->data(chain) == (void *) 2);

    chain->destroy(chain);
TEST_END

TEST_BEGIN("remove")
    chain_t * chain = chain_create(NULL);
    chain->insert(chain, (void *) 1);
    chain->insert(chain, (void *) 2);
    chain->insert(chain, (void *) 3);
    chain->reset(chain);
    chain->spin(chain, 1);

    // remove this link, leaving only 1 & 3
    // should land on 1 just because it is prev
    chain->remove(chain);
    CHECK(chain->data(chain) == (void *) 1);
    CHECK(chain->length(chain) == 2);

    // go forward 1, we should be at 3
    chain->spin(chain, 1);
    CHECK(!chain->orig(chain));
    CHECK(chain->data(chain) == (void *) 3);

    chain->destroy(chain);
TEST_END

TEST_BEGIN("clear")
    chain_t * chain = chain_create(NULL);
    chain->insert(chain, (void *) 1);
    chain->insert(chain, (void *) 2);
    chain->insert(chain, (void *) 3);

    // clear all links and data back to original state.
    // NOTE: This will fail after origin node refactor
    chain->clear(chain);
    CHECK(chain->length(chain) == 0);
    CHECK(chain->empty(chain));

    // able to add more nodes after clear
    chain->insert(chain, (void *) 4);
    chain->insert(chain, (void *) 5);
    chain->insert(chain, (void *) 6);
    CHECK(!chain->orig(chain));
    CHECK(chain->data(chain) != NULL);
    CHECK(chain->data(chain) == (void *) 6);
    CHECK(chain->length(chain) == 3);

    chain->destroy(chain);
TEST_END

TEST_BEGIN("trim")
    chain_t * chain = chain_create(NULL);
    CHECK(chain != NULL);
    CHECK(chain->length(chain) == 0);

    // trimming an empty chain should not crash
    chain->trim(chain);

    // trim a single NULL origin link should not crash
    chain->insert(chain, NULL);
    chain->trim(chain);

    // create a chain with sparse data
    size_t i;
    for (i = 0; i < 102; i++)
    {
        chain->insert(chain, (i % 3 == 0) ? (void *) i : NULL);
    }
    CHECK(chain->length(chain) == 102);

    // now trim out nodes without data
    chain->trim(chain);
    CHECK(chain->length(chain) == 34);

    // verify sane indexing
    chain->spin(chain, 33);
    CHECK((size_t)chain->data(chain) == 99);

    chain->destroy(chain);
TEST_END

TEST_BEGIN("sort")
    int i = 0;
    const size_t ids[] =        { 11, 77, 97, 22, 88, 99, 33, 55, 44, 66 };
    const size_t ids_sorted[] = { 11, 22, 33, 44, 55, 66, 77, 88, 97, 99 };
    chain_t * chain = chain_create(payload_destroy);

    fixture_reset();
    //fixture_report();

    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        chain->insert(chain, payload_create(ids[i]));
    }

    chain->sort(chain, payload_compare);
    
    payload_t * p = NULL;
    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        p = (payload_t *) chain->data(chain);
        //payload_report(p, i);
        CHECK(p->id == ids_sorted[i]);
        CHECK(p->is_created == true);
        CHECK(p->is_destroyed == false);
        chain->spin(chain, 1);
    }

    chain->destroy(chain);
TEST_END

TEST_BEGIN("destroy")
    int i = 0;
    payload_t * p = NULL;
    chain_t * chain = chain_create(payload_destroy);

    fixture_reset();

    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        chain->insert(chain, payload_create(100 - i));
    }

    //fixture_report();
    chain->destroy(chain);
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
    chain_t * chain = chain_create(payload_destroy);

    fixture_reset();

    for (i = 0; i < FIXTURE_PAYLOADS / 2; i++)
    { 
        chain->insert(chain, payload_create(i * 2));
    }
    
    payload_t * optr, * cptr;
    chain_t * mycopy = chain->copy(chain, payload_copy);
    //fixture_report();
    CHECK(mycopy != NULL)
    CHECK(mycopy != chain)

    chain->reset(chain);
    mycopy->reset(mycopy);
    for (i = 0; i < FIXTURE_PAYLOADS / 2; i++)
    {
        //CHECK(chain->link != mycopy->link);
        optr = (payload_t *) chain->data(chain);
        cptr = (payload_t *) mycopy->data(mycopy);
        CHECK(optr != NULL);
        CHECK(cptr != NULL);
        CHECK(optr != cptr);
        CHECK(optr->id == cptr->id);
        CHECK(optr->is_created == cptr->is_created);
        CHECK(optr->is_destroyed == cptr->is_destroyed);

        //payload_report(i, optr);
        //payload_report(i, cptr);

        chain->spin(chain, 1);
        chain->spin(mycopy, 1);
    }

    chain->destroy(chain);
    mycopy->destroy(mycopy);

    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        p = (payload_t *) fixture_payload(i);
        CHECK(p->is_created == true);
        CHECK(p->is_destroyed == true);
    }

TEST_END

TEST_BEGIN("split")
    size_t i;
    chain_t * chain = chain_create(NULL);
    CHECK(chain != NULL);
    CHECK(chain->length(chain) == 0);

    for (i = 1; i <= 7; i++)
    {
        chain->insert(chain, (void *) i);
        CHECK(!chain->empty(chain));
        CHECK(chain->length(chain) == i);
        CHECK(chain->data(chain) != NULL);
        CHECK(chain->data(chain) == (void *) i);
    }

    chain_t * segment = chain->split(chain, 4, 7);
    CHECK(segment->length(segment) == 3);
    CHECK(chain->length(chain) == 4);
    //CHECK(chain->orig != segment->orig);

    chain->reset(chain);
    segment->reset(segment);
    for (i = 1; i <= 5; i++)
    {
        CHECK(chain->data(chain) != NULL);
        CHECK(chain->data(chain) == (void *)
            ((i - 1) % chain->length(chain) + 1));

        CHECK(segment->data(segment) != NULL);
        CHECK(segment->data(segment) == (void *)
            ((i - 1) % segment->length(segment) + 5));

        chain->spin(chain, 1);
        chain->spin(segment, 1);
    }

    chain->destroy(chain);
    segment->destroy(segment);
TEST_END

TEST_BEGIN("splice")
    size_t i;
    chain_t * achain = chain_create(NULL);
    chain_t * bchain = chain_create(NULL);
    CHECK(achain != NULL);
    CHECK(bchain != NULL);
    CHECK(achain->length(achain) == 0);
    CHECK(bchain->length(bchain) == 0);

    for (i = 1; i <= 4; i++)
    {
        achain->insert(achain, (void *) i);
        CHECK(!achain->empty(achain));
        CHECK(achain->length(achain) == i);
        CHECK(achain->data(achain) != NULL);
        CHECK(achain->data(achain) == (void *) i);
        bchain->insert(bchain, (void *) (i + 4));
        CHECK(!bchain->empty(bchain));
        CHECK(bchain->length(bchain) == i);
        CHECK(bchain->data(bchain) != NULL);
        CHECK(bchain->data(bchain) == (void *) (i + 4));
    }

    achain = achain->join(achain, bchain);
    CHECK(achain->length(achain) == 8);

    achain->reset(achain);
    for (i = 1; i <= 8; i++)
    {
        CHECK(!achain->empty(achain));
        CHECK(achain->data(achain) != NULL);
        CHECK(achain->data(achain) == (void *) i);
        achain->spin(achain, 1);
    }

    achain->destroy(achain);
TEST_END

TESTSUITE_END
