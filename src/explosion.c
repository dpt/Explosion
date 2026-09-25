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
#include <stdint.h>
#include <stdlib.h>

#include "explosion.h"

#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CLAMP(a,min,max) ((a) < (min) ? (min) : (a) > (max) ? (max) : a)

/* -------------------------------------------------------------------------- */

// Return a random value in the specified range
static int randrange(const particle_system_t *ps, int min, int max)
{
    return min + (ps->rand_cb(32, ps->opaque) % (max + 1 - min));
}

// Return a random value in the specified float range
static float randrangef(const particle_system_t *ps, float min, float max)
{
    return min + ((float) ps->rand_cb(32, ps->opaque) / (float) UINT32_MAX) * (max - min);
}

// Return a random angle
static float randangle(const particle_system_t *ps, float angle, float range)
{
    return (angle + (float) fmod(ps->rand_cb(32, ps->opaque), range) - range / 2.0f) * (float) M_PI / 180.0f;
}

// Return a random speed
static float randspeed(const particle_system_t *ps, unsigned int speed)
{
    return 0.5f + (ps->rand_cb(32, ps->opaque) % speed) * 0.02f;
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

    // Initialise repellers
    ps->repeller_count = 0;
    for (i = 0; i < MAX_EMITTERS; i++)
        ps->repellers[i].active = 0;
}

void init_particle_system(particle_system_t      *ps,
                          particle_system_flags_t flags,
                          const particle_style_t *styles,
                          int                     nstyles,
                          float                   wall_damping,
                          particle_rand_t         rand_fn,
                          particle_time_t         time_fn,
                          particle_render_t       render_fn,
                          void                   *opaque)
{
    int total;
    int i;
    int styleindex;
    int cum;
    int target;

    ps->flags        = flags;
    ps->styles       = styles;
    ps->nstyles      = nstyles;
    ps->rand_cb      = rand_fn;
    ps->time_cb      = time_fn;
    ps->render_cb    = render_fn;
    ps->wall_damping = wall_damping;
    ps->width        = WIDTH;
    ps->height       = HEIGHT;
    ps->opaque       = opaque;

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

void set_default_style(particle_style_t *style, float frame_ms)
{
    style->min_life      = 30 * frame_ms;
    style->max_life      = 90 * frame_ms;
    style->vel_scale     = 1.0f;
    style->emit_angle    = 0.0f; // point right
    style->emit_range    = 360.0f; // full circle
    style->emit_speed    = 100;
    style->min_size      = 1;
    style->max_size      = 3;
    style->max_delay     = frame_ms; // milliseconds
    style->gravity       = GRAVITY * PHYSICS_FPS * PHYSICS_FPS; // pixels/second/second
    style->size_decay    = powf(0.999f, PHYSICS_FPS); // per-second decay factor
    style->palette_index = 0;
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
    p->created_time = ps->time_cb(ps->opaque) + (unsigned int)delay_ms;
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
        s = (style >= 0) ? style : ps->chance[ps->rand_cb(4, ps->opaque) % CHANCE_BINS];
        create_particle(ps, s, cx, cy, vx, vy);
    }
}

void update_particles(particle_system_t *ps, float dt)
{
    unsigned int current_time = ps->time_cb(ps->opaque);

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

        if ((ps->flags & PARTICLE_FLAG_NO_GRAVITY) == 0)
            // Apply gravity based on delta time
            p->vy += s->gravity * dt;

        // Calculate elapsed time since particle activation (in milliseconds)
        unsigned int age = current_time - p->created_time;

        // Bounce back with damping
        if (ps->flags & PARTICLE_FLAG_WALLS)
        {
            if (p->x < 0)
            {
                p->x = 0;
                p->vx = +fabsf(p->vx) * ps->wall_damping;
            }
            if (p->x >= ps->width)
            {
                p->x = ps->width - 1;
                p->vx = -fabsf(p->vx) * ps->wall_damping;
            }
            if (p->y < 0)
            {
                p->y = 0;
                p->vy = +fabsf(p->vy) * ps->wall_damping;
            }
            if (p->y >= ps->height)
            {
                p->y = ps->height - 1;
                p->vy = -fabsf(p->vy) * ps->wall_damping;
            }
        }

        // Check if particle should die
        if (age >= p->max_life ||
                p->size < 0.5f ||
                p->x < 0.0f || p->x >= ps->width ||
                p->y < 0.0f || p->y >= ps->height)
        {
            p->style = 0;
            ps->free_indices[ps->free_count++] = i;  // Return index to free stack
        }
    }

    // Update emitters
    update_emitters(ps, current_time);

    // Update repellers
    update_repellers(ps, current_time);
}

void render_particles(particle_system_t *ps)
{
    int           i;
    particle_t   *p;
    float         age_ratio;
    unsigned int  twinkle;
    int           palette_offset;
    int           colour_index;

    unsigned int current_time = ps->time_cb(ps->opaque);

    for (i = 0; i < MAX_PARTICLES; i++)
    {
        p = &ps->particles[i];
        if (p->style == 0 || current_time < p->created_time)
            continue;

        // Calculate age and twinkling
        age_ratio = (float) (current_time - p->created_time) / p->max_life;

        // Map age to palette index
        colour_index = (int) (age_ratio * PALETTE_SIZE);
        twinkle = (ps->rand_cb(8, ps->opaque) == 0);
        colour_index = CLAMP(twinkle ? 0 : colour_index, 0, PALETTE_SIZE - 1);

        // Get colour from palette
        palette_offset = ps->styles[p->style - 1].palette_index * PALETTE_SIZE + colour_index;

        // Render through callback
        ps->render_cb((int) p->x, (int) p->y, (int) ceilf(p->size), palette_offset, ps->opaque);
    }
}

int is_active(const particle_system_t *ps)
{
    return ps->free_count < MAX_PARTICLES;
}

/* -------------------------------------------------------------------------- */

void create_emitter(particle_system_t *ps,
                    float              x,
                    float              y,
                    float              emission_rate,
                    float              emission_jitter,
                    float              emission_clump,
                    int                style,
                    unsigned int       lifetime)
{
    particle_emitter_t *e;

    if (ps->emitter_count >= MAX_EMITTERS)
        return;

    e = &ps->emitters[ps->emitter_count++];
    e->active          = 1;
    e->x               = x;
    e->y               = y;
    e->emission_rate   = emission_rate;
    e->emission_jitter = emission_jitter;
    e->emission_clump  = emission_clump;
    e->style           = style;
    e->lifetime        = lifetime;
    e->last_emit_time  = e->start_time = ps->time_cb(ps->opaque);
}

void update_emitters(particle_system_t *ps, unsigned int current_time)
{
    int                 i;
    particle_emitter_t *e;
    float               dt_since_last;
    float               jittered_rate;
    float               noisy_rate;
    float               noisy_clumpy_rate;
    int                 nparticles;
    int                 s;
    int                 j;
    int                 new_count;

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
        jittered_rate = e->emission_rate * e->emission_jitter;
        noisy_rate = e->emission_rate + randrangef(ps, -jittered_rate, +jittered_rate);
        if (e->emission_clump > 0.0f && e->emission_clump < 1.0f)
        {
            if (randrangef(ps, 0.0f, 1.0f) >= e->emission_clump)
                noisy_clumpy_rate = 0.0f;
            else
                noisy_clumpy_rate = noisy_rate / (1.0f - e->emission_clump);
        }
        else
        {
            noisy_clumpy_rate = noisy_rate;
        }

        nparticles = (int) (noisy_clumpy_rate * dt_since_last);
        if (nparticles > 0)
        {
            for (j = 0; j < nparticles; j++)
            {
                s = (e->style >= 0) ? e->style : ps->chance[ps->rand_cb(4, ps->opaque) % CHANCE_BINS];
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

/* -------------------------------------------------------------------------- */

void create_repeller(particle_system_t *ps,
                     float              x,
                     float              y,
                     float              strength,
                     float              max_distance)
{
    particle_repeller_t *r;

    if (ps->repeller_count >= MAX_EMITTERS)
        return;

    r = &ps->repellers[ps->repeller_count++];
    r->active       = 1;
    r->x            = x;
    r->y            = y;
    r->strength     = strength;
    r->max_distance = max_distance;
}

void update_repellers(particle_system_t *ps, unsigned int current_time)
{
    int i, j;
    particle_repeller_t *r;
    particle_t *p;
    float dx, dy, distance, force, force_x, force_y;

    for (i = 0; i < ps->repeller_count; i++)
    {
        r = &ps->repellers[i];
        if (!r->active)
            continue;

        for (j = 0; j < MAX_PARTICLES; j++)
        {
            p = &ps->particles[j];

            // Skip inactive particles
            if (p->style == 0)
                continue;

            // Skip particles that haven't started yet
            if (current_time < p->created_time)
                continue;

            // AABB check: quick rejection if particle is outside bounding box
            dx = p->x - r->x;
            dy = p->y - r->y;
            if (r->max_distance > 0.0f && (fabsf(dx) > r->max_distance || fabsf(dy) > r->max_distance))
                continue;

            // Calculate actual distance only if AABB check passes
            distance = sqrtf(dx * dx + dy * dy);

            // Skip if particle is too far from repeller
            if (r->max_distance > 0.0f && distance > r->max_distance)
                continue;

            // Skip if particle is at the same position as repeller
            if (distance < 0.1f)
                continue;

            // Calculate repelling force (inverse square law)
            // Force decreases with distance
            force = r->strength / (distance * distance);

            // Normalize direction vector
            force_x = dx / distance * force;
            force_y = dy / distance * force;

            // Apply force to particle velocity (push away from repeller)
            p->vx += force_x;
            p->vy += force_y;
        }
    }

    // Remove inactive repellers
    int new_count = 0;
    for (i = 0; i < ps->repeller_count; i++)
        if (ps->repellers[i].active)
            ps->repellers[new_count++] = ps->repellers[i];
    ps->repeller_count = new_count;
}

void destroy_repeller(particle_system_t *ps, int index)
{
    if (index < 0 || index >= ps->repeller_count)
        return;
    ps->repellers[index].active = 0;
}

/* -------------------------------------------------------------------------- */

// vim:sw=4:sts=4:ts=8:tw=78:
