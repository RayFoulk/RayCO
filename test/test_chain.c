#include "blammo.h"
#include "chain.h"
#include "mut.h"

// basic chain operations
// * chain_create
// chain_destroy
// * chain_clear
// * chain_insert
// * chain_delete
// * chain_forward
// * chain_rewind
// * chain_reset

// advanced chain operations
// * chain_trim
// chain_sort
// chain_copy
// chain_segment
// chain_splice

// Test fixture payload.  Don't really actually
// use heap for this, because we want to test
// conditions and inspect state throughout.
// This is more of a 'mock' dynamic payload
typedef struct
{
    unsigned long id;
    bool is_created;
    bool is_destroyed;
}
payload_t;

// statically allocate test payloads on
// program stack for analysis.
#define MAX_PAYLOADS 10
static size_t payload_index = 0;
static payload_t payloads[MAX_PAYLOADS];

// test fixture functions, simulating a
// factory / object interface
static payload_t * payload_create(unsigned long id)
{
    if (payload_index >= MAX_PAYLOADS)
    {
        // Simulate malloc fail
        return NULL;
    }
    
    payload_t * payload = &payloads[payload_index];
    payload_index++;
    
    payload->id = id;
    payload->is_created = true;
    return payload;
}                                 

static void payload_destroy(payload_t * payload)
{
    payload->is_destroyed = false;
    
    if (payload_index > 0)
    {
        payload_index--;
    }
}




TESTSUITE_BEGIN

    TEST_BEGIN("basic chain functions")
        // create a simple chain
        chain_t * mychain = chain_create(NULL);
        CHECK(mychain != NULL);
        CHECK(mychain->length == 0);

        // add the first link
        chain_insert(mychain);
        CHECK(mychain->link != NULL);
        CHECK(mychain->orig != NULL);
        CHECK(mychain->link == mychain->orig); 
        CHECK(mychain->length == 1);

        // add and set some simple data
        mychain->link->data = malloc(sizeof(int));
        CHECK(mychain->link->data != NULL);
        *(int *)mychain->link->data = 1;
        CHECK(*(int *)mychain->link->data == 1);

        // add another link
        chain_insert(mychain);
        CHECK(mychain->link != NULL);
        CHECK(mychain->orig != NULL);
        CHECK(mychain->link != mychain->orig); 
        CHECK(mychain->length == 2);

        // add and set some more data
        mychain->link->data = malloc(sizeof(int));
        CHECK(mychain->link->data != NULL);
        *(int *)mychain->link->data = 2;
        CHECK(*(int *)mychain->link->data == 2);

        // add a third link
        chain_insert(mychain);
        CHECK(mychain->link != NULL);
        CHECK(mychain->orig != NULL);
        CHECK(mychain->link != mychain->orig); 
        CHECK(mychain->length == 3);

        // add and set data
        mychain->link->data = malloc(sizeof(int));
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
            chain_insert(mychain);
            mychain->link->data = malloc(32);
        }
        CHECK(mychain->length == 99);

        chain_destroy(mychain);
        // cannot check anything, but local pointer
        // is invalid.  could solve with ** arg

    TEST_END

    TEST_BEGIN("advanced chain functions")
        chain_t * mychain = chain_create(NULL);
        CHECK(mychain != NULL);
        CHECK(mychain->length == 0);

        // create a chain with sparse data
        int i;
        for (i = 0; i < 102; i++)
        {
            chain_insert(mychain);
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
        
        memset(payloads, 0, MAX_PAYLOADS *
            sizeof(payload_t));
        
                        
                                
        chain_destroy(mychain);
    TEST_END

TESTSUITE_END
