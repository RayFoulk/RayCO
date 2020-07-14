#include "blammo.h"
#include "chain.h"
#include "mut.h"

// basic chain operations
// * chain_create
// chain_destroy
// chain_clear
// * chain_insert
// chain_delete
// chain_forward
// chain_rewind
// * chain_reset

// advanced chain operations
// chain_trim
// chain_sort
// chain_copy
// chain_segment
// chain_splice


TESTSUITE_BEGIN

    TEST_BEGIN("basic chain operations")
        // create a simple chain
        chain_t * mychain = chain_create(NULL);
        CHECK(mychain != NULL);
        CHECK(mychain->length == 0);

        // add a link and set some simple data
        chain_insert(mychain);
        CHECK(mychain->link != NULL);
        CHECK(mychain->orig != NULL);
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
        CHECK(mychain->length == 2);

        // add and set some more data
        mychain->link->data = malloc(sizeof(int));
        CHECK(mychain->link->data != NULL);
        *(int *)mychain->link->data = 2;
        CHECK(*(int *)mychain->link->data == 2);

        // reset back to origin
        CHECK(mychain->link != mychain->orig);
        chain_reset(mychain);
        CHECK(mychain->link == mychain->orig);
        CHECK(mychain->link->data != NULL);
        CHECK(*(int *)mychain->link->data == 1);
       
        

        chain_destroy(mychain);
        //CHECK(mychain->orig == NULL);

    TEST_END

    TEST_BEGIN("advanced chain operations")
        chain_t * mychain = chain_create(NULL);
        CHECK(mychain != NULL);
        CHECK(mychain->length == 0);
        chain_destroy(mychain);
    TEST_END

TESTSUITE_END
