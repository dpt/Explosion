// main.c
//
// Particle effects
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "explosion.h"
#include "gradient.h"
#include "random-pool.h"

/* -------------------------------------------------------------------------- */

// Config
//

#define SCALE         (4)       // screen scale
#define MAX_STYLES    (3)
#define NPARTICLES    (MAX_PARTICLES / 2) // num. particles to spawn on clicks

/* -------------------------------------------------------------------------- */

// Define colour stops
//

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
    { { 255, 154,   0, 255 }, 0.0f }, // Orange
    { { 127, 127, 127, 255 }, 0.4f }, // Mid Grey
    { {  31,  31,  31, 255 }, 0.9f }, // Dark Grey
    { {   0,   0,   0, 255 }, 1.0f }, // Black
};

static const gradientstop_t fleck[] =
{
    { { 255, 255, 255, 255 }, 0.0f }, // White
    { { 255, 255,   0, 255 }, 0.2f }, // Yellow
    { {   0, 255,   0, 255 }, 0.3f }, // Green
    { {   0, 127,   0, 255 }, 0.5f }, // Dark Green
    { {   0,   0, 127, 255 }, 0.9f }, // Dark Blue
    { {   0,   0,   0, 255 }, 1.0f }, // Black
};

/* -------------------------------------------------------------------------- */

typedef struct State
{
    particle_system_t ps;
    rand_pool_t       randpool;
    SDL_Renderer     *renderer;
    SDL_Color         palettes[PALETTE_SIZE * MAX_STYLES];
} State;

/* -------------------------------------------------------------------------- */

/// Render a filled rectangle centered on (x,y) with dimensions (w,h).
static void rectfill(int x, int y, int w, int h, const SDL_Color *colour, SDL_Renderer *renderer)
{
    SDL_FRect rect;

    rect.x = x - w / 2.0f;
    rect.y = y - h / 2.0f;
    rect.w = w;
    rect.h = h;

    SDL_SetRenderDrawColor(renderer, colour->r, colour->g, colour->b, colour->a);
    SDL_RenderFillRect(renderer, &rect);
}

/// Render a filled square centred on (x,y) with dimensions (size,size).
static void squarefill(int x, int y, int size, const SDL_Color *colour, SDL_Renderer *renderer)
{
    rectfill(x, y, size, size, colour, renderer);
}

/// Render an unfilled rectangle centered on (x,y) with dimensions (w,h).
static void rect(int x, int y, int w, int h, const SDL_Color *colour, SDL_Renderer *renderer)
{
    SDL_FRect rect;

    rect.x = x - w / 2.0f;
    rect.y = y - h / 2.0f;
    rect.w = w;
    rect.h = h;

    SDL_SetRenderDrawColor(renderer, colour->r, colour->g, colour->b, colour->a);
    SDL_RenderRect(renderer, &rect);
}

/// Render an unfilled square centred on (x,y) with dimensions (size,size).
static void square(int x, int y, int size, const SDL_Color *colour, SDL_Renderer *renderer)
{
    rect(x, y, size, size, colour, renderer);
}

/* -------------------------------------------------------------------------- */

// Random value callback
static unsigned int rand_callback(int nbits, void *opaque)
{
    State *state = opaque;
    return randpool_get(&state->randpool, nbits);
}

// Time callback
static unsigned int get_ticks_callback(void)
{
    return SDL_GetTicks();
}

// Render callback for particles
static void render_particle_callback(int x, int y, int size, int palette_index, void *opaque)
{
    State *state = opaque;
    squarefill(x, y, size, &state->palettes[palette_index], state->renderer);
}

/* -------------------------------------------------------------------------- */

// Renders dancing palette
static void render_palettes(State *state)
{
    float t;
    int   pi;
    int   p;
    int   i;
    float s,c;

    t  = SDL_GetTicks() / 100.0;
    pi = 0;
    for (p = 0; p < MAX_STYLES; p++)
        for (i = 0; i < PALETTE_SIZE; i++)
        {
            s = sinf(t + p + i);
            c = cosf(t + p + i);
            render_particle_callback((i + 1) * 6 + s * 2.0f,
                                     (p + 1) * 6 + c * 2.0f,
                                     3.0f + s * 1.5f,
                                     pi++,
                                     state);
        }
}

// Renders other things
static void render_others(State *state)
{
    static const SDL_Color repeller = { 255, 0, 0, 255 };
    static const SDL_Color attractor = { 0, 0, 255, 255 };

    int i;

    for (i = 0; i < state->ps.emitter_count; i++)
    {
        particle_emitter_t *e = &state->ps.emitters[i];
        if (e->active)
            square(e->x, e->y, 3, &smokey[0].colour, state->renderer);
    }

    for (i = 0; i < state->ps.repeller_count; i++)
    {
        particle_repeller_t *r = &state->ps.repellers[i];
        if (r->active && r->strength != 0.0f)
            square(r->x, r->y, r->max_distance * 2, (r->strength >= 0) ? &repeller : &attractor, state->renderer);
    }
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    // Frames/sec we'll allow for refresh
    static const int fpses[] =
    {
        1, 2, 5, 10, 15, 30, 60, 120, 240, 480, 960
    };

    State            *state;
    particle_style_t  styles[3];
    SDL_Event         e;
    int               i;

    state = calloc(1, sizeof(*state));
    if (state == NULL)
    {
        printf("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    // Initialise random pool (4KB of random numbers)
    randpool_init(&state->randpool, 1024 * 4);

    create_gradient_palette(firey,  &state->palettes[PALETTE_SIZE * 0], PALETTE_SIZE);
    create_gradient_palette(smokey, &state->palettes[PALETTE_SIZE * 1], PALETTE_SIZE);
    create_gradient_palette(fleck,  &state->palettes[PALETTE_SIZE * 2], PALETTE_SIZE);

    // Convert frame-based values to millisecond-based values
    // 1 frame = 1000/60 ms
    float frame_ms = 1000.0f / PHYSICS_FPS;

    // Set default styles
    set_default_style(&styles[0], frame_ms);
    styles[0].probability = 90;
    styles[0].palette_index = 0;  // Fire palette
    styles[0].emit_angle  = 270.0f; // point up
    styles[0].emit_range  = 90.0f;  // quarter circle

    set_default_style(&styles[1], frame_ms);
    styles[1].probability = 8;
    styles[1].palette_index = 1;  // Smoke palette
    styles[1].emit_angle  = 270.0f; // point up
    styles[1].emit_range  = 90.0f;
    styles[1].min_life   *= 4;
    styles[1].max_life   *= 4;
    styles[1].vel_scale   = 0.1f;
    styles[1].emit_speed  = 10;
    styles[1].min_size    = 1;
    styles[1].max_size    = 2;
    styles[1].gravity    /= -100.0f; // pixels/second/second

    set_default_style(&styles[2], frame_ms);
    styles[2].probability = 2;
    styles[2].palette_index = 2;  // Fleck palette
    styles[2].emit_speed  = 200;

    // Initialise SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SDL could not initialise! SDL_Error: %s\n", SDL_GetError());
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
    state->renderer = SDL_CreateRenderer(window, NULL);
    if (state->renderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Set render scale to 2x for pixel doubling effect
    SDL_SetRenderScale(state->renderer, SCALE, SCALE);

    // Initialise particle system with callbacks
    init_particle_system(&state->ps,
                         0,
                         styles,
                         NELEMS(styles),
                         0.2f,
                         rand_callback,
                         get_ticks_callback,
                         render_particle_callback,
                         state);

    // Create smoke particle emitters
    // 10 particles/sec, smoke style, indefinite lifetime
    // small chance of emission, else nothing
    for (i = 1; i < 5; i++)
        create_emitter(&state->ps,
                       WIDTH * i / 5, HEIGHT * 4 / 5,
                       10.0f,
                       (i - 1.0f) / (4.0f - 1.0f),
                       0.01f + (i - 1) * 0.02f,
                       1, 0);

    // Create a repeller at the centre left of the screen with moderate strength
    create_repeller(&state->ps,
                    WIDTH * 1 / 3, HEIGHT / 2,
                    100.0f,
                    5.0f);

    // Create an attractor at the centre right of the screen with moderate strength
    create_repeller(&state->ps,
                    WIDTH * 2 / 3, HEIGHT / 2,
                    -100.0f,
                    5.0f);

    // We only use rand() in main.c
    srand(time(NULL));

    // Game loop
    int quit = 0;
    int pause = 0;
    int nparticles = NPARTICLES;
    int selectedFPS = 6; /* 60fps */
    Uint64 last_physics_time = SDL_GetPerformanceCounter();

    // Mouse velocity tracking
    int last_mouse_x = -1, last_mouse_y = -1;
    Uint64 last_mouse_time = 0;
    float last_mouse_vx = 0.0f, last_mouse_vy = 0.0f;

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
                    create_explosion(&state->ps, -1,
                                     e.button.x / SCALE, e.button.y / SCALE,
                                     last_mouse_vx, last_mouse_vy,
                                     nparticles);
                    break;
                case 2:
                    create_explosion(&state->ps, 0,
                                     e.button.x / SCALE, e.button.y / SCALE,
                                     last_mouse_vx, last_mouse_vy,
                                     nparticles);
                    break;
                case 3:
                    create_explosion(&state->ps, 2,
                                     e.button.x / SCALE, e.button.y / SCALE,
                                     last_mouse_vx, last_mouse_vy,
                                     nparticles);
                    break;
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                switch (e.key.key)
                {
                case SDLK_SPACE:
                    pause = !pause;
                    break;
                case SDLK_DELETE:
                    reset_particle_system(&state->ps);
                    break;
                case SDLK_G:
                    // Toggle gravity
                    state->ps.flags ^= PARTICLE_FLAG_NO_GRAVITY;
                    printf("Gravity %s\n", (state->ps.flags & PARTICLE_FLAG_NO_GRAVITY) ? "disabled" : "enabled");
                    break;
                case SDLK_W:
                    // Toggle walls
                    state->ps.flags ^= PARTICLE_FLAG_WALLS;
                    printf("Walls %s\n", (state->ps.flags & PARTICLE_FLAG_WALLS) ? "enabled" : "disabled");
                    break;
                case SDLK_Q:
                    quit = 1;
                    break;
                case SDLK_LEFTBRACKET:
                case SDLK_RIGHTBRACKET:
                    selectedFPS += (e.key.key == SDLK_RIGHTBRACKET) ? +1 : -1;
                    selectedFPS = CLAMP(selectedFPS, 0, NELEMS(fpses) - 1);
                    printf("%dfps\n", fpses[selectedFPS]);
                    break;
                default:
                    printf("key down: %s\n", SDL_GetKeyName(e.key.key));
                    break;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
            {
                int   current_x = e.motion.x / SCALE;
                int   current_y = e.motion.y / SCALE;
                float vx        = 0.0f;
                float vy        = 0.0f;
                float damping   = 0.5f;

                if (last_mouse_time != 0)
                {
                    float dt = (start - last_mouse_time) / (float) SDL_GetPerformanceFrequency();
                    if (dt > 0.001f)
                    {
                        vx = (float) (current_x - last_mouse_x) / dt;
                        vy = (float) (current_y - last_mouse_y) / dt;
                    }
                }
                create_particle(&state->ps, 1, current_x, current_y, vx * damping, vy * damping);
                last_mouse_x = current_x;
                last_mouse_y = current_y;
                last_mouse_time = start;
                last_mouse_vx = vx;
                last_mouse_vy = vy;
            }
            break;

            case SDL_EVENT_MOUSE_WHEEL:
                nparticles += e.wheel.y * 10.0f;
                nparticles = CLAMP(nparticles, 1, MAX_PARTICLES);
                break;

                //default:
                //    printf("Unhandled event code {%d}\n", e.type);
                //    break;
            }
        }

        if (!pause)
        {
            // Update particles with delta time
            Uint64 current_physics_time = SDL_GetPerformanceCounter();
            float dt = (current_physics_time - last_physics_time) / (float) SDL_GetPerformanceFrequency();
            last_physics_time = current_physics_time;
            update_particles(&state->ps, dt);

            // Add more particles when idle
            if (!is_active(&state->ps))
                create_explosion(&state->ps, -1,
                                 rand() % WIDTH, rand() % HEIGHT,
                                 0.0f, 0.0f,
                                 NPARTICLES);
        }

        // Clear screen
        SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
        SDL_RenderClear(state->renderer);

        // Draw the palettes and that
        render_palettes(state);
        render_others(state);

        // Render particles
        render_particles(&state->ps);

        // Update screen
        SDL_RenderPresent(state->renderer);

        // Frame rate control
        Uint64 end = SDL_GetPerformanceCounter();

        // Cap to selected FPS (not PHYSICS_FPS)
        float elapsedMS = (end - start) / SDL_GetPerformanceFrequency() * 1000.0f;
        float delay = 1000.0f / (float) fpses[selectedFPS] - elapsedMS;
        SDL_Delay(delay);
    }

    // Cleanup
    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(state);

    return 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:
