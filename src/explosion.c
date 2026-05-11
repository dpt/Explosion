// explosion.c
//
// Particle effects
//
// Requires SDL3
//
// Build and run with:
// cc -o explosion explosion.c `pkg-config sdl3 --cflags --libs` && ./explosion
//
// Use:
// - an initial burst will happen
// - click with mouse button to do new burst
// - single particles will spawn as you move your mouse
// - when idle a random burst will happen
// - mouse wheel to adjust number of particles spewed on clicks
//

// TODO
// - Particle spin
// - Random number pool for performance
// - Explosion types - add/improve
// - Cascades (particles randomly trigger more bursts)
// - Emitters (emit particles just by existing)
//

#include <math.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "explosion.h"

/* -------------------------------------------------------------------------- */

// Function to interpolate between two colours
static SDL_Color interpolateColour(SDL_Color a, SDL_Color b, float t)
{
    SDL_Color result;

    result.r = (Uint8)(a.r + (b.r - a.r) * t);
    result.g = (Uint8)(a.g + (b.g - a.g) * t);
    result.b = (Uint8)(a.b + (b.b - a.b) * t);
    result.a = 255; // Opaque
    return result;
}

// Create a gradient palette
void create_gradient_palette(const gradientstop_t *colours,
                             SDL_Color            *palette,
                             int                   paletteSize)
{
    float t;
    int   seg;
    float segmentStart;
    float segmentEnd;
    float segmentT;

    for (int i = 0; i < paletteSize; i++)
    {
        t = (float)i / (paletteSize - 1);

        // Find which segment this index falls into
        for (seg = 0; ; seg++)
            if (t >= colours[seg + 0].stop && t <= colours[seg + 1].stop)
                break;

        // Calculate interpolation factor within segment
        segmentStart = colours[seg + 0].stop;
        segmentEnd   = colours[seg + 1].stop;
        segmentT     = (t - segmentStart) / (segmentEnd - segmentStart);

        // Interpolate between colours
        palette[i] = interpolateColour(colours[seg + 0].colour,
                                       colours[seg + 1].colour,
                                       segmentT);
    }
}

/* -------------------------------------------------------------------------- */

// Unify our rand calls
static unsigned int ourrand(void)
{
    return arc4random();
}

// Return a random value in the specified range
static int randrange(int min, int max)
{
    return min + (ourrand() % (max + 1 - min));
}

// Return a random value in the specified float range
static float randrangef(float min, float max)
{
    return min + ((float)ourrand() / (float)UINT32_MAX) * (max - min);
}

// Return a random angle
static float randangle(float angle, float range)
{
    return (angle + fmodf(ourrand(), range) - range / 2.0f) * (float) M_PI / 180.0f;
}

// Return a random speed
static float randspeed(unsigned int speed)
{
    return 0.5f + (ourrand() % speed) * 0.02f;
}

// Initialize particle system
void init_particle_system(particle_system_t      *ps,
                          const particle_style_t *styles)
{
    int i;

    ps->styles = styles;

    // Initialize all particles to inactive
    for (i = 0; i < MAX_PARTICLES; i++)
        ps->particles[i].style = 0;

    ps->active_count = 0;
}

// Create a new particle
static void create_particle(particle_system_t *ps,
                            int                cx,
                            int                cy,
                            int                smoke_pc)
{
    int                     i;
    particle_t             *p;
    const particle_style_t *s;
    int                     is_smoke;
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

    // Choose a style
    is_smoke = (ourrand() % 100) < (unsigned int) smoke_pc;
    s = &ps->styles[is_smoke];
    p->style = is_smoke + 1;

    // Set initial position at explosion center
    p->x = cx;
    p->y = cy;

    angle = randangle(s->emit_angle, s->emit_range);
    //s->emit_angle += 0.25f; // HACK for spinning
    speed = randspeed(s->emit_speed);

    // Set velocity based on angle and speed (convert to pixels/second)
    p->vx = cosf(angle) * speed * s->vel_scale * PHYSICS_FPS;
    p->vy = sinf(angle) * speed * s->vel_scale * PHYSICS_FPS;

    // Random lifetime between min and max (in milliseconds)
    p->max_life = randrangef(s->min_life, s->max_life);

    // Some particles have a delayed start (stored in delay milliseconds)
    p->life = p->max_life + (ourrand() % (int)(s->delay + 1));

    // Random size
    p->size = randrange(s->min_size, s->max_size);

    // Record creation time
    p->created_time = SDL_GetTicks();
    p->initial_life = p->life;
    p->last_update_time = p->created_time;

    ps->active_count++;
}

// Create explosion effect with additional explosions
void create_explosion(particle_system_t *ps,
                      int                cx,
                      int                cy,
                      int                particle_count,
                      int                smoke_pc,
                      int                create_additional)
{
    int i;

    for (i = 0; i < particle_count; i++)
        create_particle(ps, cx, cy, smoke_pc);

    // Create additional explosions if requested
    if (create_additional && ps->active_count > 0)
    {
        // Create 1-3 additional smaller explosions
        int additional_count = 1 + (ourrand() % 3);
        for (i = 0; i < additional_count; i++)
        {
            // Create explosion at random position near main explosion
            float x = cx + (ourrand() % 100) - 50;
            float y = cy + (ourrand() % 100) - 50;

            // Create temporary system for additional explosion
            create_explosion(ps, x, y, 30, smoke_pc, 0); // No further additional explosions
        }
    }
}

// Update particle system
void update_particles(particle_system_t *ps)
{
    Uint32 current_time = SDL_GetTicks();

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const particle_style_t *s;

        particle_t *p = &ps->particles[i];

        if (p->style == 0)
            continue;

        s = &ps->styles[p->style - 1];

        // Calculate elapsed time since particle creation (in milliseconds)
        Uint32 elapsed_ms = current_time - p->created_time;

        // Calculate delta time since last update
        float dt = (current_time - p->last_update_time) / 1000.0f;

        // Animate when ready (when delay period has passed)
        if (elapsed_ms > (Uint32)s->delay)
        {
            // Update position based on velocity and delta time
            p->x += p->vx * dt;
            p->y += p->vy * dt;

            // Update size decay exponentially based on delta time
            p->size *= powf(s->size_decay, dt);

            // Apply gravity based on delta time
            p->vy += s->gravity * dt;

            // Cascade
            // if (ourrand() % 1000 == 0)
            //     create_explosion(ps, p->x, p->y, 10, 0);
        }

        // Update life (represents remaining lifespan)
        p->life = p->initial_life - elapsed_ms;

        // Update last update time
        p->last_update_time = current_time;

        // Check if particle should die
        if (p->life <= 0 || p->size <= 0.1f || (unsigned int) p->x >= WIDTH || (unsigned int) p->y >= HEIGHT)
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
void render_particles(SDL_Renderer *renderer, particle_system_t *ps)
{
    int              i;
    particle_t      *p;
    const SDL_Color *c;
    float	         alpha;
    int              intensity;
    const SDL_Color *palette;

    for (i = 0; i < MAX_PARTICLES; i++)
    {
        p = &ps->particles[i];

        if (p->style == 0 || p->life >= p->max_life)
            continue;

        // Calculate alpha based on life
        alpha = powf(p->life / p->max_life, 0.4545f) * 255.0f;
        if (ourrand() & 1) alpha *= 2;
        if (alpha > 255) alpha = 255;

        // Set colour with alpha
        const int maxpal = (PALETTE_SIZE - 1);
        intensity = (int)((p->life / p->max_life) * maxpal);
        palette = ps->styles[p->style - 1].palette;
        c = &palette[maxpal - intensity];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, (Uint8)alpha);

        rect(renderer, p->x, p->y, p->size);
    }

    // Draw the palettes
    for (i = 0; i < PALETTE_SIZE; i++)
    {
        int x;

        x = (i + 1) * 4;

        c = &ps->styles[0].palette[i];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
        rect(renderer, x, 4 * 1, 4);

        c = &ps->styles[1].palette[i];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
        rect(renderer, x, 4 * 2, 4);
    }
}

// Check if system has active particles
int is_active(const particle_system_t *ps)
{
    return ps->active_count > 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:
