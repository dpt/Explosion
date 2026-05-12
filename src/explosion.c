// explosion.c
//
// Particle effects
//

// TODO
// - Particle spin
// - Explosion types - add/improve
// - Cascades (particles randomly trigger more bursts)
// - Emitters (emit particles just by existing)
//

#include <math.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "explosion.h"

#include <assert.h>

/* -------------------------------------------------------------------------- */

// Random number pool
//

#define RAND_POOL_SIZE (1024 * 4)

typedef struct
{
    unsigned int pool[RAND_POOL_SIZE];
    int index;
} rand_pool_t;

static rand_pool_t g_rand_pool =
{
    .index = RAND_POOL_SIZE
};

// Refill the random pool from system entropy
static void refill_rand_pool(void)
{
    arc4random_buf(g_rand_pool.pool, sizeof(g_rand_pool.pool));
    g_rand_pool.index = 0;
}

// Get next random number from pool, refilling if needed
static unsigned int poolrand(void)
{
    if (g_rand_pool.index >= RAND_POOL_SIZE)
        refill_rand_pool();

    return g_rand_pool.pool[g_rand_pool.index++];
}

/* -------------------------------------------------------------------------- */

// Return a random value in the specified range
static int randrange(int min, int max)
{
    return min + (poolrand() % (max + 1 - min));
}

// Return a random value in the specified float range
static float randrangef(float min, float max)
{
    return min + ((float)poolrand() / (float)UINT32_MAX) * (max - min);
}

// Return a random angle
static float randangle(float angle, float range)
{
    return (angle + fmodf(poolrand(), range) - range / 2.0f) * (float) M_PI / 180.0f;
}

// Return a random speed
static float randspeed(unsigned int speed)
{
    return 0.5f + (poolrand() % speed) * 0.02f;
}

/* -------------------------------------------------------------------------- */

// Initialize particle system
void init_particle_system(particle_system_t      *ps,
                          const particle_style_t *styles,
                          int                     nstyles)
{
    int total;
    int i;
    int s;
    int cum;
    int target;

    ps->styles  = styles;
    ps->nstyles = nstyles;

    // Total probabilities
    total = 0;
    for (i = 0; i < nstyles; i++)
        total += styles[i].probability;

    // Build a probabilty table for O(1) selection
    s = 0;
    cum = styles[s].probability;
    target = cum * CHANCE_BINS / total;
    for (i = 0; i < CHANCE_BINS; i++)
    {
        if (i >= target)
        {
            cum += styles[++s].probability;
            target = cum * CHANCE_BINS / total;
        }
        ps->chance[i] = s;
    }

    // Initialize all particles to inactive
    for (i = 0; i < MAX_PARTICLES; i++)
        ps->particles[i].style = 0;

    ps->active_count = 0;
}

// Set default fire particle style
void set_default_style(particle_style_t *style,
                       float             frame_ms,
                       SDL_Color        *palette)
{
    style->min_life   = 30 * frame_ms;
    style->max_life   = 90 * frame_ms;
    style->vel_scale  = 1.0f;
    style->emit_angle = 0.0f; // point right
    style->emit_range = 360.0f;  // full circle
    style->emit_speed = 100;
    style->min_size   = 1;
    style->max_size   = 3;
    style->max_delay  = frame_ms; // milliseconds
    style->gravity    = GRAVITY * PHYSICS_FPS * PHYSICS_FPS; // pixels/second/second
    style->size_decay = powf(0.999f, PHYSICS_FPS); // per-second decay factor
    style->palette    = palette;
}

// Create a new particle
static void create_particle(particle_system_t *ps,
                            int                style,
                            int                cx,
                            int                cy)
{
    int                     i;
    particle_t             *p;
    const particle_style_t *s;
    float                   angle;
    float                   speed;

    if (ps->active_count >= MAX_PARTICLES)
        return;

    // Find first inactive particle
    for (i = 0; i < MAX_PARTICLES; i++)
        if (ps->particles[i].style == 0)
            break;
    if (i == MAX_PARTICLES)
        return;

    p = &ps->particles[i];
    s = &ps->styles[style];
    p->style = style + 1;

    // Set initial position at explosion centre
    p->x = cx;
    p->y = cy;

    angle = randangle(s->emit_angle, s->emit_range);
    speed = randspeed(s->emit_speed);

    // Set velocity based on angle and speed (convert to pixels/second)
    p->vx = cosf(angle) * speed * s->vel_scale * PHYSICS_FPS;
    p->vy = sinf(angle) * speed * s->vel_scale * PHYSICS_FPS;

    // Random lifetime between min and max (in milliseconds)
    p->max_life = randrange(s->min_life, s->max_life);

    // Random size
    p->size = randrange(s->min_size, s->max_size);

    // Set created_time to a future time for delayed start (in milliseconds)
    float delay_ms = randrangef(0.0f, s->max_delay);
    p->created_time = SDL_GetTicks() + (Uint32)delay_ms;

    ps->active_count++;
}

// Create explosion effect with additional explosions
void create_explosion(particle_system_t *ps,
                      int                cx,
                      int                cy,
                      int                particle_count,
                      int                force_style,
                      int                create_additional)
{
    int i;
    int style;

    for (i = 0; i < particle_count; i++)
    {
        // Choose a style
        style = (force_style >= 0) ? force_style : ps->chance[poolrand() % CHANCE_BINS];
        create_particle(ps, style, cx, cy);
    }

    // Create additional explosions if requested
    if (create_additional && ps->active_count > 0)
    {
        // Create 1-3 additional smaller explosions
        int additional_count = 1 + (poolrand() % 3);
        for (i = 0; i < additional_count; i++)
        {
            // Create explosion at random position near main explosion
            float x = cx + (poolrand() % 100) - 50;
            float y = cy + (poolrand() % 100) - 50;

            // Create temporary system for additional explosion
            create_explosion(ps, x, y, 30, -1, 0); // No further additional explosions
        }
    }
}

// Update particle system
void update_particles(particle_system_t *ps, float dt)
{
    Uint32 current_time = SDL_GetTicks();

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const particle_style_t *s;

        particle_t *p = &ps->particles[i];

        if (p->style == 0)
            continue; // inactive

        // Check if particle hasn't started yet (delay)
        if (current_time < p->created_time)
            continue;

        s = &ps->styles[p->style - 1];

        // Update position based on velocity and delta time
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        // Update size decay exponentially based on delta time
        p->size *= powf(s->size_decay, dt);

        // Apply gravity based on delta time
        p->vy += s->gravity * dt;

        // Calculate elapsed time since particle activation (in milliseconds)
        Uint32 age = current_time - p->created_time;

        // Check if particle should die
        if (age >= p->max_life || p->size <= 0.1f || (unsigned int) p->x >= WIDTH || (unsigned int) p->y >= HEIGHT)
        {
            p->style = 0;
            ps->active_count--;
        }
    }
}

// Draw a rectangle centred on (x,y)
static void rect(SDL_Renderer* renderer, int x, int y, int size)
{
    SDL_FRect rect;

    rect.x = x - size / 2.0f;
    rect.y = y - size / 2.0f;
    rect.w = size;
    rect.h = size;

    SDL_RenderFillRect(renderer, &rect);
}

// Render particles to SDL surface
void render_particles(particle_system_t *ps, SDL_Renderer *renderer)
{
    int              i;
    particle_t      *p;
    float            age_ratio;
    float	         alpha;
    float            index;
    const SDL_Color *c;

    Uint32 current_time = SDL_GetTicks();

    for (i = 0; i < MAX_PARTICLES; i++)
    {
        p = &ps->particles[i];
        if (p->style == 0 || current_time < p->created_time)
            continue;

        // Calculate alpha from age
        age_ratio = (float) (current_time - p->created_time) / p->max_life;
        alpha = powf(age_ratio, 0.4545f) * 255.0f;
        if (poolrand() & 1) alpha *= 2; // random flicker (50% chance)
        if (alpha > 255) alpha = 255;

        // Set colour with alpha
        index = age_ratio * PALETTE_SIZE;
        c = &ps->styles[p->style - 1].palette[(int) index];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, (Uint8)alpha);

        // Draw
        rect(renderer, p->x, p->y, p->size);
    }

    // Draw the palettes
    for (int s = 0; s < 3; s++)
    {
        for (i = 0; i < PALETTE_SIZE; i++)
        {
            c = &ps->styles[s].palette[i];
            SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
            rect(renderer, (i + 1) * 4, 4 * (1+s), 4);
        }
    }
}

// Check if system has active particles
int is_active(const particle_system_t *ps)
{
    return ps->active_count > 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:
