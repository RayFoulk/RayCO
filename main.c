#include "blammo.h"
#include "chain.h"

int main(int argc, char *argv[])
{
    chain_t * mychain = chain_create(NULL);


    chain_destroy(mychain);


    return 0;
}
