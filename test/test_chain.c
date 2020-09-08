#include "blammo.h"
#include "chain.h"
#include "mut.h"

#include <string.h>
#include <limits.h>

// basic chain operations
// * chain_create
// * chain_destroy
// * chain_clear
// * chain_insert
// * chain_delete
// * chain_forward
// * chain_rewind
// * chain_reset

// advanced chain operations
// * chain_trim
// * chain_sort
// chain_copy
// chain_segment
// chain_splice

// Test fixture payload.  Don't really actually
// use heap for this, because we want to test
// conditions and inspect state throughout.
// This is more of a 'mock' dynamic payload
typedef struct
{
    size_t id;
    bool is_created;
    bool is_destroyed;
}
payload_t;

#define FIXTURE_PAYLOADS 10
typedef struct
{
    // statically allocate test fixture.payloads on
    // program stack for analysis.
    payload_t payloads[FIXTURE_PAYLOADS];
    size_t i, j;
}
fixture_t;

// The test fixture
static fixture_t fixture;

static void fixture_reset()
{
    memset(fixture.payloads, 0, FIXTURE_PAYLOADS * sizeof(payload_t));
    fixture.i = 0;
    fixture.j = FIXTURE_PAYLOADS - 1;
}

static void fixture_report()
{
    int i;
    for (i = 0; i < FIXTURE_PAYLOADS; i++)
    {
        printf("payload %d: id: %zu created: %s destroyed: %s\n",
               i, fixture.payloads[i].id,
               fixture.payloads[i].is_created ? "true" : "false",
               fixture.payloads[i].is_destroyed ? "true" : "false");
    }
}

// test fixture functions, simulating a
// factory / object interface
static payload_t * payload_create(size_t id)
{
    if (fixture.i >= FIXTURE_PAYLOADS)
    {
        // Simulate malloc fail
        return NULL;
    }
    
    payload_t * payload = &fixture.payloads[fixture.i++];
    payload->id = id;
    payload->is_created = true;
    return payload;
}                                 

static void payload_destroy(void * ptr)
{
    payload_t * payload = (payload_t *) ptr;
    payload->is_destroyed = true;
    
    if (fixture.i > 0)
    {
        fixture.i--;
    }
}

static int payload_compare(const void * a, const void * b)
{
    payload_t * ap = (payload_t *) *(void **) a;
    payload_t * bp = (payload_t *) *(void **) b;

    if (!ap || !bp)
    {
        return INT_MIN;
    }

    return (int) ap->id - (int) bp->id;
}

static void * payload_copy(const void * p)
{
    payload_t * orig = (payload_t *) p;
    payload_t * copy = NULL;

    if (!orig || fixture.j == 0)
    {
        return copy;
    }

    copy = &fixture.payloads[fixture.j--];
    // WARNING: Stack copies may not be 'destroyed' properly

    memcpy(copy, orig, sizeof(payload_t));
    return copy;
}

TESTSUITE_BEGIN

TEST_BEGIN("basic chain functions")
	// create a simple chain
	chain_t * mychain = chain_create(free);
	CHECK(mychain != NULL);
	CHECK(mychain->length == 0);

	// add the first link
	chain_insert(mychain, malloc(sizeof(int)));
	CHECK(mychain->link != NULL);
	CHECK(mychain->orig != NULL);
	CHECK(mychain->link == mychain->orig);
	CHECK(mychain->length == 1);

	// add and set some simple data
	CHECK(mychain->link->data != NULL);
	*(int *)mychain->link->data = 1;
	CHECK(*(int *)mychain->link->data == 1);

	// add another link
	chain_insert(mychain, malloc(sizeof(int)));
	CHECK(mychain->link != NULL);
	CHECK(mychain->orig != NULL);
	CHECK(mychain->link != mychain->orig);
	CHECK(mychain->length == 2);

	// add and set some more data
	CHECK(mychain->link->data != NULL);
	*(int *)mychain->link->data = 2;
	CHECK(*(int *)mychain->link->data == 2);

	// add a third link
	chain_insert(mychain, malloc(sizeof(int)));
	CHECK(mychain->link != NULL);
	CHECK(mychain->orig != NULL);
	CHECK(mychain->link != mychain->orig);
	CHECK(mychain->length == 3);

	// add and set data
	CHECK(mychain->link->data != NULL);
	*(int *)mychain->link->data = 3;
	CHECK(*(int *)mychain->link->data == 3);

	// reset back to origin
	CHECK(mychain->link != mychain->orig);
	chain_reset(mychain);
	CHECK(mychain->link == mychain->orig);
	CHECK(mychain->link->data != NULL);
	CHECK(*(int *)mychain->link->data == 1);

	// go forward two links
	chain_forward(mychain, 2);
	CHECK(mychain->link != mychain->orig);
	CHECK(*(int *)mychain->link->data == 3);

	// rewind one link
	chain_rewind(mychain, 1);
	CHECK(mychain->link != mychain->orig);
	CHECK(*(int *)mychain->link->data == 2);

	// go forward two links
	// because of circular property,
	// it should be back at origin
	chain_forward(mychain, 2);
	CHECK(mychain->link == mychain->orig);
	CHECK(*(int *)mychain->link->data == 1);

	// rewind two.  because of circular
	// property, should be at index 2
	chain_rewind(mychain, 2);
	CHECK(mychain->link != mychain->orig);
	CHECK(*(int *)mychain->link->data == 2);

	// delete this link, leaving only 1 & 3
	// should land on 1 just because it is prev
	chain_delete(mychain);
	CHECK(*(int *)mychain->link->data == 1);
	CHECK(mychain->length == 2);

	// go forward 1, we should be at 3
	chain_forward(mychain, 1);
	CHECK(mychain->link != mychain->orig);
	CHECK(*(int *)mychain->link->data == 3);

	// clear all links and data
	// back to original state.
	// NOTE: This will fail after refactor
	chain_clear(mychain);
	CHECK(mychain->length == 0);
	CHECK(mychain->link != NULL);
	CHECK(mychain->orig != NULL);
	CHECK(mychain->link == mychain->orig);

	// allocate a whole bunch after clearing
	int i;
	for (i = 0; i < 99; i++)
	{
		chain_insert(mychain, malloc(32));
	}
	CHECK(mychain->length == 99);

	chain_destroy(mychain);
	// cannot (easily) check heap, but local pointer
	// is invalid.  could solve with ** arg
    // however, destroy must have same signature
    // as free()

TEST_END

TEST_BEGIN("advanced chain functions")
    ///////////////////////////////
    // test: trim
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

    ///////////////////////////////
    // test: sort
	// now test the sort function
	mychain = chain_create(payload_destroy);
	// start fresh, reset mock fixture
    fixture_reset();
	//fixture_report();

	const size_t ids[] =        { 11, 77, 97, 22, 88, 99, 33, 55, 44, 66 };
	const size_t ids_sorted[] = { 11, 22, 33, 44, 55, 66, 77, 88, 97, 99 };
	for (i = 0; i < FIXTURE_PAYLOADS; i++)
	{
		chain_insert(mychain, payload_create(ids[i]));
	}

	chain_sort(mychain, payload_compare);
	
    payload_t * p = NULL;
	for (i = 0; i < FIXTURE_PAYLOADS; i++)
	{
        p = (payload_t *) mychain->link->data;
        printf("payload %d: id: %zu\n", i, p->id);
        CHECK(p->id == ids_sorted[i]);
	    CHECK(p->is_created == true);
	    CHECK(p->is_destroyed == false);
	    
		chain_forward(mychain, 1);
	}

    ///////////////////////////////
    // test: destroy
	//fixture_report();
	chain_destroy(mychain);
	//fixture_report();
    (void) fixture_report;
    
	for (i = 0; i < FIXTURE_PAYLOADS; i++)
	{
        p = (payload_t *) &fixture.payloads[i];
	    CHECK(p->is_created == true);
	    CHECK(p->is_destroyed == true);
	}

    ///////////////////////////////
    // test: copy
    
    // TODO: Break this down into a test per function
    fixture_reset();


TEST_END

TESTSUITE_END
