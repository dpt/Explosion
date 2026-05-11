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
// - click with mouse buttons to do new bursts (three styles - one per button)
// - when idle a random burst will happen
// - press 'g' to toggle gravity
// - mouse wheel to adjust gravity (g-g to reset)

// TODO
// - Particle spin
// - Separate physics from frame rate
// - Random number pool
// - Explosion types - improve
// - Re-explosions

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>

/* -------------------------------------------------------------------------- */

// Config
#define SCALE         (4)
#define WIDTH         (256)
#define HEIGHT        (192)
#define MAX_PARTICLES (1000)
#define NPARTICLES    (MAX_PARTICLES / 2)
#define GRAVITY       (0.075f)
#define FPS           (60)

/* -------------------------------------------------------------------------- */

// Particle structure
typedef struct particle
{
    int     style;      // stores (style+1); 0 means inactive
    float   x, y;       // position
    float   vx, vy;     // velocity
    float   life;       // current life (life > max_life means don't render yet)
    float   max_life;   // maximum life
    float   size;       // particle size
} particle_t;

// Particle style
typedef struct particle_style
{
    int     min_life;   // frames [TIME]
    int     max_life;   // frames [TIME]
    float   vel_scale;  // factor
    float   emit_angle; // degrees (0/90/180/270 is right/down/left/up)
    float   emit_range; // degrees
    float   emit_speed; // 200 is fast
    int     min_size;   // pixels
    int     max_size;   // pixels
    int     delay;      // frames [TIME]
    float   gravity;
    float   size_decay; // factor (per-frame)
    SDL_Color *palette;
} particle_style_t;

// Particle system structure
typedef struct particle_system
{
    // config
    int     smoke_pc;	// percentage smoke [ought to be array of factors per style]
    const particle_style_t *styles;

    // state
    particle_t particles[MAX_PARTICLES];
    int     active_count;
} particle_system_t;

/* -------------------------------------------------------------------------- */

// A gradient colour
typedef struct gradientstop
{
    SDL_Color colour;
    float     stop;
} gradientstop_t;

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
static void createGradientPalette(const gradientstop_t *colours,
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

// Return a random angle
static float randangle(float angle, float range)
{
    return (angle + fmodf(ourrand(), range) - range / 2.0f) * M_PI / 180.0f;
}

// Return a random speed
static float randspeed(unsigned int speed)
{
    return 0.5f + (ourrand() % speed) * 0.02f;
}

// Initialize particle system
void init_particle_system(particle_system_t      *ps,
                          const particle_style_t *styles,
                          int                     smoke_pc)
{
    int i;


    ps->styles = styles;
    ps->smoke_pc = smoke_pc;

    // Initialize all particles to inactive
    for (i = 0; i < MAX_PARTICLES; i++)
        ps->particles[i].style = 0;

    ps->active_count = 0;
}

// Create a new particle
void create_particle(particle_system_t *ps, int cx, int cy)
{
    const particle_style_t *s;
    particle_t             *p;

    if (ps->active_count >= MAX_PARTICLES)
        return;

    int i = 0;
    // Find first inactive particle
    while (i < MAX_PARTICLES && ps->particles[i].style > 0)
        i++;

    if (i >= MAX_PARTICLES)
        return;

    p = &ps->particles[i];

    // Choose a style
    int is_smoke = (ourrand() % 100) < (unsigned int) ps->smoke_pc;
    s = &ps->styles[is_smoke];
    p->style = is_smoke + 1;

    // Set initial position at explosion center
    p->x = cx;
    p->y = cy;

    float angle = randangle(s->emit_angle, s->emit_range);
    //s->emit_angle += 0.25f; // HACK for spinning
    float speed = randspeed(s->emit_speed);

    // Set velocity based on angle and speed
    p->vx = cosf(angle) * speed * s->vel_scale;
    p->vy = sinf(angle) * speed * s->vel_scale;

    // Random lifetime between 30-80 frames
    p->max_life = randrange(s->min_life, s->max_life);

    // Some particles have a delayed start
    p->life = p->max_life + (ourrand() % s->delay);

    // Random size between 1-4 pixels
    p->size = randrange(s->min_size, s->max_size);

    ps->active_count++;
}

// Create explosion effect with additional explosions
void create_explosion(particle_system_t *ps, int cx, int cy, int particle_count, int create_additional)
{
    for (int i = 0; i < particle_count; i++)
        create_particle(ps, cx, cy);

    // Create additional explosions if requested
    if (create_additional && ps->active_count > 0)
    {
        // Create 1-3 additional smaller explosions
        int additional_count = 1 + (ourrand() % 3);
        for (int i = 0; i < additional_count; i++)
        {
            // Create explosion at random position near main explosion
            float x = cx + (ourrand() % 100) - 50;
            float y = cy + (ourrand() % 100) - 50;

            // Create temporary system for additional explosion
            create_explosion(ps, x, y, 30, 0); // No further additional explosions
        }
    }
}

// Update particle system
void update_particles(particle_system_t *ps)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const particle_style_t *s;

        particle_t *p = &ps->particles[i];

        if (p->style == 0)
            continue;

        s = &ps->styles[p->style - 1];

        // Animate when ready
        if (p->life < p->max_life)
        {
            // Update position
            p->x += p->vx;
            p->y += p->vy;

            // Update size
            p->size *= s->size_decay;

            // Apply gravity
            p->vy += s->gravity;
        }

        // Update life
        p->life -= 1.0f;

        // Check if particle should die
        if (p->life <= 0 || p->size <= 0 ||
                (unsigned) p->x >= WIDTH || (unsigned) p->y >= HEIGHT)
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
        intensity = (int)((p->life / p->max_life) * 15);
        palette = ps->styles[p->style - 1].palette;
        c = &palette[15 - intensity];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, (Uint8)alpha);

        rect(renderer, p->x, p->y, p->size);
    }

    // Draw the palettes
    for (i = 0; i < 16; i++)
    {
        int x;

        x = (i + 1) * SCALE;

        c = &ps->styles[0].palette[i];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
        rect(renderer, x, SCALE * 1, SCALE);

        c = &ps->styles[1].palette[i];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
        rect(renderer, x, SCALE * 2, SCALE);
    }
}

// Check if system has active particles
int is_active(particle_system_t *ps)
{
    return ps->active_count > 0;
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    // Define colour stops
    static const gradientstop_t firey[] =
    {
        { { 255, 255, 255, 255 }, 0.0f }, // White
        { { 255, 232,   8, 255 }, 0.1f }, // Yellow
        { { 255, 206,   0, 255 }, 0.2f }, // Yellow-Orange
        { { 255, 154,   0, 255 }, 0.5f }, // Orange
        { { 255,  90,   0, 255 }, 0.6f }, // Red
        { {   0,   0, 127, 255 }, 1.0f }, // Dark Blue
    };

    static const gradientstop_t smokey[] =
    {
        { { 191, 191, 191, 255 }, 0.0f }, // Light Grey
        { {   0,   0,   0, 255 }, 1.0f }, // Black
    };


    SDL_Color         fire_palette[16];
    SDL_Color         smoke_palette[16];
    particle_style_t  styles[2];
    particle_system_t ps;
    SDL_Event         e;

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("Retro Explosion Particle System",
					  WIDTH * SCALE, HEIGHT * SCALE, 0);
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Set render scale to 2x for pixel doubling effect
    SDL_SetRenderScale(renderer, SCALE, SCALE);

    // Enable alpha blending
    // SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    createGradientPalette(firey, &fire_palette[0], 16);
    createGradientPalette(smokey, &smoke_palette[0], 16);

    styles[0].min_life   = 30;
    styles[0].max_life   = 90;
    styles[0].vel_scale  = 1.0f;
    styles[0].emit_angle = 270.0f; // point up
    styles[0].emit_range = 90.0f;  // quarter circle
    styles[0].emit_speed = 100.0f;
    styles[0].min_size   = 1;
    styles[0].max_size   = 3;
    styles[0].delay      = FPS; // frames
    styles[0].gravity    = GRAVITY;
    styles[0].size_decay = 0.999f;
    styles[0].palette    = fire_palette;

    styles[1].min_life   = 120;
    styles[1].max_life   = 180;
    styles[1].vel_scale  = 0.1f;
    styles[1].emit_angle = 0.0f;   // point right
    styles[1].emit_range = 360.0f; // full circle
    styles[1].emit_speed = 50.0f;
    styles[1].min_size   = 1;
    styles[1].max_size   = 2;
    styles[1].delay      = FPS; // frames
    styles[1].gravity    = GRAVITY * 0.025;
    styles[1].size_decay = 0.999f;
    styles[1].palette    = smoke_palette;

    // Initialize particle system
    init_particle_system(&ps, styles, 8 /* smoke % */);

    // Create initial explosion
    create_explosion(&ps,
                  WIDTH / 2, HEIGHT / 2,
                  NPARTICLES,
                  0);

    // Game loop
    int quit = 0;
    int pause = 0;

    while (!quit)
    {
        Uint64 start = SDL_GetPerformanceCounter();

        // Handle events
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_EVENT_QUIT:
                quit = 1;
                break;

            // Mouse click to create new explosion
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                switch (e.button.button)
                {
                case 1:
                    create_explosion(&ps, e.button.x / SCALE, e.button.y / SCALE, NPARTICLES, 1);
                    break;
                case 2:
                    break;
                case 3:
                    break;
                }
                break;

            case SDL_EVENT_KEY_UP:
                switch (e.key.key)
                {
                case SDLK_SPACE:
                    pause = !pause;
                    break;
                case SDLK_G:

                    break;
                case SDLK_Q:
                    quit = 1;
                    break;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                create_explosion(&ps, e.button.x / SCALE, e.button.y / SCALE, 1, 0);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                //ps.styles[0].gravity += e.wheel.y / 100.0f;
                //ps.styles[1].gravity = ps.styles[0].gravity * 0.025;
                break;

                //default:
                //    printf("Unhandled event code {%d}\n", e.type);
                //    break;
            }
        }

        if (!pause)
        {
            // Update particles
            update_particles(&ps);

            // Add more particles when idle
            if (!is_active(&ps))
                create_explosion(&ps, ourrand() % WIDTH, ourrand() % HEIGHT, NPARTICLES, 0);
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render particles
        render_particles(renderer, &ps);

        // Update screen
        SDL_RenderPresent(renderer);

        // Frame rate control
        Uint64 end = SDL_GetPerformanceCounter();

        float elapsedMS = (end - start) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
//        printf("fps: %.2f\n", 1.0 / (elapsedMS / 1000.0f));

        // Cap to configured FPS
        SDL_Delay(floor(1000.0f / FPS - elapsedMS));
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:

