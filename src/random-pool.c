// random-pool.c
//
// Random number pool implementation
//

#include <math.h>
#include <stdlib.h>

#include "random-pool.h"

/* -------------------------------------------------------------------------- */

void randpool_init(rand_pool_t *rp, int pool_size)
{
    rp->pool  = malloc(pool_size * sizeof(unsigned int));
    rp->size  = pool_size;
    rp->index = pool_size;
}

void randpool_cleanup(rand_pool_t *rp)
{
    if (rp->pool != NULL)
    {
        free(rp->pool);
        rp->pool = NULL;
    }
    rp->size  = 0;
    rp->index = 0;
}

static void randpool_refill(rand_pool_t *rp)
{
    arc4random_buf(rp->pool, rp->size * sizeof(unsigned int));
    rp->index = 0;
}

unsigned int randpool_get(rand_pool_t *rp)
{
    if (rp->index >= rp->size)
        randpool_refill(rp);

    return rp->pool[rp->index++];
}

/* -------------------------------------------------------------------------- */

static rand_pool_t globalrandpool;

unsigned int globalrandpool_get(void)
{
    return randpool_get(&globalrandpool);
}

void globalrandpool_init(int pool_size)
{
    randpool_init(&globalrandpool, pool_size);
}

void globalrandpool_cleanup(void)
{
    randpool_cleanup(&globalrandpool);
}

// vim:sw=4:sts=4:ts=8:tw=78:
