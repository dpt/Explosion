// random-pool.h
//
// Random number pool for efficient random value generation
//

#ifndef RANDOM_POOL_H
#define RANDOM_POOL_H

/* -------------------------------------------------------------------------- */

typedef struct
{
    unsigned int *pool;
    int           size;
    int           index;
    unsigned int  bit_index;  // bit position within current uint32_t
} rand_pool_t;

/* -------------------------------------------------------------------------- */

void randpool_init(rand_pool_t *rp, int pool_size);
void randpool_cleanup(rand_pool_t *rp);
unsigned int randpool_get(rand_pool_t *rp, int nbits);

/* -------------------------------------------------------------------------- */

void globalrandpool_init(int pool_size);
void globalrandpool_cleanup(void);
unsigned int globalrandpool_get(int nbits);

/* -------------------------------------------------------------------------- */

#endif // RANDOM_POOL_H

// vim:sw=4:sts=4:ts=8:tw=78:
