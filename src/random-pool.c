// random-pool.c
//
// Random number pool implementation
//

#include <math.h>
#include <stdlib.h>

#include "random-pool.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

/* -------------------------------------------------------------------------- */

void randpool_init(rand_pool_t *rp, int pool_size)
{
    rp->pool      = malloc(pool_size * sizeof(unsigned int));
    rp->size      = pool_size;
    rp->index     = pool_size;
    rp->bit_index = 0;
}

void randpool_cleanup(rand_pool_t *rp)
{
    if (rp->pool != NULL)
    {
        free(rp->pool);
        rp->pool = NULL;
    }
    rp->size      = 0;
    rp->index     = 0;
    rp->bit_index = 0;
}

static void randpool_refill(rand_pool_t *rp)
{
    arc4random_buf(rp->pool, rp->size * sizeof(unsigned int));
    rp->index     = 0;
    rp->bit_index = 0;
}

// Get a specified number of bits from the pool
unsigned int randpool_get(rand_pool_t *rp, int nbits)
{
    unsigned int result;
    unsigned int bits_available;
    unsigned int bits_to_extract;
    unsigned int mask;
    unsigned int remaining_bits;
    unsigned int more_bits;

    if (nbits <= 0 || nbits > 32) {
        assert(0);
        return 0;
    }

    result = 0;

    // If we need more bits than available in current uint32_t, refill
    if (rp->index >= rp->size)
        randpool_refill(rp);

    // Extract bits from current uint32_t
    bits_available = 32 - rp->bit_index;
    bits_to_extract = (nbits > bits_available) ? bits_available : nbits;

    // Extract the bits
    mask = (1U << bits_to_extract) - 1;
    result = (rp->pool[rp->index] >> rp->bit_index) & mask;

    // Update bit index
    rp->bit_index += bits_to_extract;

    // If we used all bits in current uint32_t, advance to next
    if (rp->bit_index >= 32) {
        rp->bit_index = 0;
        rp->index++;
    }

    // If we need more bits and there aren't enough in current uint32_t, get more
    if (nbits > bits_to_extract) {
        remaining_bits = nbits - bits_to_extract;
        more_bits = randpool_get(rp, remaining_bits);
        result |= (more_bits << bits_to_extract);
    }

    return result;
}

/* -------------------------------------------------------------------------- */

static rand_pool_t globalrandpool;

unsigned int globalrandpool_get(int nbits)
{
    return randpool_get(&globalrandpool, nbits);
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
