#include "blammo.h"
#include "chain.h"
#include "mut.h"


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

        

        chain_destroy(mychain);
        //CHECK(mychain->orig == NULL);

    TEST_END
TESTSUITE_END
