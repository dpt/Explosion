// explosion.c
//
// Particle effects
//

// TODO
// - Particle spin
// - Explosion types - add/improve
// - Cascades (particles randomly trigger more bursts)
//

#include <math.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "explosion.h"

#include <assert.h>

/* -------------------------------------------------------------------------- */

// Return a random value in the specified range
static int randrange(const particle_system_t *ps, int min, int max)
{
    return min + (ps->randfn() % (max + 1 - min));
}

// Return a random value in the specified float range
static float randrangef(const particle_system_t *ps, float min, float max)
{
    return min + ((float)ps->randfn() / (float)UINT32_MAX) * (max - min);
}

// Return a random angle
static float randangle(const particle_system_t *ps, float angle, float range)
{
    return (angle + fmodf(ps->randfn(), range) - range / 2.0f) * (float) M_PI / 180.0f;
}

// Return a random speed
static float randspeed(const particle_system_t *ps, unsigned int speed)
{
    return 0.5f + (ps->randfn() % speed) * 0.02f;
}

/* -------------------------------------------------------------------------- */

void reset_particle_system(particle_system_t *ps)
{
    int i;

    // Initialise all particles to inactive
    for (i = 0; i < MAX_PARTICLES; i++)
        ps->particles[i].style = 0;

    // Initialise free index stack (all particles start as free)
    for (i = 0; i < MAX_PARTICLES; i++)
        ps->free_indices[i] = MAX_PARTICLES - 1 - i;  // Stack in reverse
    ps->free_count = MAX_PARTICLES;

    // Initialise emitters
    ps->emitter_count = 0;
    for (i = 0; i < MAX_EMITTERS; i++)
        ps->emitters[i].active = 0;
}

void init_particle_system(particle_system_t      *ps,
                          const particle_style_t *styles,
                          int                     nstyles,
                          particlerand_t          randfn)
{
    int total;
    int i;
    int styleindex;
    int cum;
    int target;

    ps->styles  = styles;
    ps->nstyles = nstyles;
    ps->randfn  = randfn;

    // Total probabilities
    total = 0;
    for (i = 0; i < nstyles; i++)
        total += styles[i].probability;

    // Build a probabilty table for O(1) selection
    styleindex = 0;
    cum = styles[styleindex].probability;
    target = cum * CHANCE_BINS / total;
    for (i = 0; i < CHANCE_BINS; i++)
    {
        if (i >= target)
        {
            cum += styles[++styleindex].probability;
            target = cum * CHANCE_BINS / total;
        }
        ps->chance[i] = styleindex;
    }

    reset_particle_system(ps);
}

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

void create_particle(particle_system_t *ps,
                     int                style,
                     int                cx,
                     int                cy,
                     float              vx,
                     float              vy)
{
    int                     i;
    particle_t             *p;
    const particle_style_t *s;
    float                   angle;
    float                   speed;

    if (ps->free_count <= 0)
        return;

    // Pop from free index stack (O(1) allocation)
    i = ps->free_indices[--ps->free_count];

    p = &ps->particles[i];
    s = &ps->styles[style];
    p->style = style + 1;

    // Set initial position at explosion centre
    p->x = cx;
    p->y = cy;

    angle = randangle(ps, s->emit_angle, s->emit_range);
    speed = randspeed(ps, s->emit_speed);

    // Set velocity based on angle and speed (convert to pixels/second)
    p->vx = cosf(angle) * speed * s->vel_scale * PHYSICS_FPS;
    p->vy = sinf(angle) * speed * s->vel_scale * PHYSICS_FPS;

    // Add additional velocity offset
    p->vx += vx;
    p->vy += vy;

    // Random lifetime between min and max (in milliseconds)
    p->max_life = randrange(ps, s->min_life, s->max_life);

    // Random size
    p->size = randrange(ps, s->min_size, s->max_size);

    // Set created_time to a future time for delayed start (in milliseconds)
    float delay_ms = randrangef(ps, 0.0f, s->max_delay);
    p->created_time = SDL_GetTicks() + (Uint32)delay_ms;
}

void create_explosion(particle_system_t *ps,
                      int                style,
                      int                cx,
                      int                cy,
                      float              vx,
                      float              vy,
                      int                particle_count)
{
    int i;
    int s;

    for (i = 0; i < particle_count; i++)
    {
        s = (style >= 0) ? style : ps->chance[ps->randfn() % CHANCE_BINS];
        create_particle(ps, s, cx, cy, vx, vy);
    }
}

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
        if (s->size_decay)
            p->size *= powf(s->size_decay, dt);

        // Apply gravity based on delta time
        p->vy += s->gravity * dt;

        // Calculate elapsed time since particle activation (in milliseconds)
        Uint32 age = current_time - p->created_time;

        // Bounce back with damping
        if (0)
        {
            const float damping = 0.1f;

            if (p->x < 0)
            {
                p->x = 0;
                p->vx = +fabsf(p->vx) * damping;
            }
            if (p->x >= WIDTH)
            {
                p->x = WIDTH - 1;
                p->vx = -fabsf(p->vx) * damping;
            }
            if (p->y < 0)
            {
                p->y = 0;
                p->vy = +fabsf(p->vy) * damping;
            }
            if (p->y >= HEIGHT)
            {
                p->y = HEIGHT - 1;
                p->vy = -fabsf(p->vy) * damping;
            }
        }

        // Check if particle should die
        if (age >= p->max_life || p->size < 0.5f || (unsigned int) (int) p->x >= WIDTH || (unsigned int) (int) p->y >= HEIGHT)
        {
            p->style = 0;
            ps->free_indices[ps->free_count++] = i;  // Return index to free stack
        }
    }

    // Update emitters
    update_emitters(ps, current_time);
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

void render_particles(particle_system_t *ps, SDL_Renderer *renderer)
{
    int              i;
    particle_t      *p;
    float            age_ratio;
    float	         alpha;
    float            index;
    const SDL_Color *c;
    int              s;

    Uint32 current_time = SDL_GetTicks();

    for (i = 0; i < MAX_PARTICLES; i++)
    {
        p = &ps->particles[i];
        if (p->style == 0 || current_time < p->created_time)
            continue;

        // Calculate alpha from age
        age_ratio = (float) (current_time - p->created_time) / p->max_life;
        alpha = powf(age_ratio, 0.4545f) * 255.0f;
        if (ps->randfn() & 1) alpha *= 2; // random flicker (50% chance)
        if (alpha > 255) alpha = 255;

        // Set colour with alpha
        index = age_ratio * PALETTE_SIZE;
        c = &ps->styles[p->style - 1].palette[(int) index];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, (Uint8)alpha);

        // Draw
        rect(renderer, p->x, p->y, ceilf(p->size));
    }

    // Draw the palettes
    for (s = 0; s < 3; s++)
    {
        float t;

        t = current_time / 100.0;
        for (i = 0; i < PALETTE_SIZE; i++)
        {
            int x, y, z;

            x = sinf(t + i + s) * 2.0f;
            y = cosf(t + i + s) * 2.0f;
            z = 3.0f + sinf(t + i + s) * 1.5f;
            c = &ps->styles[s].palette[i];
            SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
            rect(renderer, (i + 1) * 6 + x, 6 * (1 + s) + y, z);
        }
    }
}

int is_active(const particle_system_t *ps)
{
    return ps->free_count < MAX_PARTICLES;
}

void create_emitter(particle_system_t *ps,
                    float              x,
                    float              y,
                    float              emission_rate,
                    int                style,
                    Uint32             lifetime)
{
    particle_emitter_t *e;

    if (ps->emitter_count >= MAX_EMITTERS)
        return;

    e = &ps->emitters[ps->emitter_count++];
    e->active         = 1;
    e->x              = x;
    e->y              = y;
    e->emission_rate  = emission_rate;
    e->style          = style;
    e->lifetime       = lifetime;
    e->last_emit_time = e->start_time = SDL_GetTicks();
}

void update_emitters(particle_system_t *ps, Uint32 current_time)
{
    int        i;
    particle_emitter_t *e;
    float      dt_since_last;
    int        nparticles;
    int        j;
    int        s;
    int        new_count;

    for (i = 0; i < ps->emitter_count; i++)
    {
        e = &ps->emitters[i];
        if (!e->active)
            continue;

        // Check if lifetime expired (0 meaning no limit)
        if (e->lifetime && current_time - e->start_time >= e->lifetime)
        {
            e->active = 0;
            continue;
        }

        // Time since last emission in seconds
        dt_since_last = (current_time - e->last_emit_time) / 1000.0f;

        // Particles to emit
        nparticles = (int) (e->emission_rate * dt_since_last);
        if (nparticles > 0)
        {
            for (j = 0; j < nparticles; j++)
            {
                s = (e->style >= 0) ? e->style : ps->chance[ps->randfn() % CHANCE_BINS];
                create_particle(ps, s, e->x, e->y, 0.0f, 0.0f);
            }
            e->last_emit_time = current_time;
        }
    }

    // Remove inactive emitters
    new_count = 0;
    for (i = 0; i < ps->emitter_count; i++)
        if (ps->emitters[i].active)
            ps->emitters[new_count++] = ps->emitters[i];
    ps->emitter_count = new_count;
}

void destroy_emitter(particle_system_t *ps, int index)
{
    if (index < 0 || index >= ps->emitter_count)
        return;
    ps->emitters[index].active = 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:
