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

#define SCALE              (4)   // screen scale
#define MAX_STYLES         (4)
#define NPARTICLES         (MAX_PARTICLES / 2) // num. particles to spawn on clicks

#define MOUSE_EMIT_RATE    5.0f
#define MOUSE_EMIT_MAX     10    // cap burst on unpause
#define MOUSE_DAMPING      0.25f

/* -------------------------------------------------------------------------- */

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

static const gradientstop_t pastel[] =
{
    { { 251, 243, 185 }, 0.0f }, // Lemon
    { { 255, 220, 204 }, 0.3f }, // Peach
    { { 253, 183, 234 }, 0.7f }, // Pink
    { { 183, 177, 242 }, 1.0f }, // Mauve
};

/* -------------------------------------------------------------------------- */

static const int fpses[] = { 1, 2, 5, 10, 15, 30, 60, 120, 240, 480, 960 };

/* -------------------------------------------------------------------------- */

static void draw_rect(int x, int y, int w, int h, const SDL_Color *colour,
                      SDL_Renderer *renderer, int filled)
{
    SDL_FRect r;

    r.x = x - w / 2.0f;
    r.y = y - h / 2.0f;
    r.w = w;
    r.h = h;

    SDL_SetRenderDrawColor(renderer, colour->r, colour->g, colour->b, colour->a);
    if (filled)
        SDL_RenderFillRect(renderer, &r);
    else
        SDL_RenderRect(renderer, &r);
}

/// Render a filled square centred on (x,y) with dimensions (size,size).
static void squarefill(int x, int y, int size, const SDL_Color *colour, SDL_Renderer *renderer)
{
    draw_rect(x, y, size, size, colour, renderer, 1);
}

/// Render an unfilled square centred on (x,y) with dimensions (size,size).
static void square(int x, int y, int size, const SDL_Color *colour, SDL_Renderer *renderer)
{
    draw_rect(x, y, size, size, colour, renderer, 0);
}

/* -------------------------------------------------------------------------- */

typedef struct State
{
    particle_system_t ps;
    rand_pool_t       randpool;
    SDL_Renderer     *renderer;
    SDL_Color         palettes[PALETTE_SIZE * MAX_STYLES];

    // Game loop
    int    quit;
    int    pause;
    int    nparticles;
    int    selectedFPS;
    Uint64 last_physics_time;
    float  cps; // counter ticks per second

    // Mouse state
    Uint64 last_mouse_time;       // (Uint64)-1 = not yet seen
    int    last_mouse_x;
    int    last_mouse_y;
    float  last_mouse_vx;
    float  last_mouse_vy;
    Uint64 last_mouse_emit_time;  // (Uint64)-1 = not yet seen
} State;

/* -------------------------------------------------------------------------- */

// Callbacks

static unsigned int rand_callback(int nbits, void *opaque)
{
    State *state = opaque;
    return randpool_get(&state->randpool, nbits);
}

static unsigned int get_ticks_callback(void)
{
    return SDL_GetTicks();
}

static void render_particle_callback(int x, int y, int size, int palette_index, void *opaque)
{
    State *state = opaque;
    squarefill(x, y, size, &state->palettes[palette_index], state->renderer);
}

/* -------------------------------------------------------------------------- */

// Render helpers

static void render_palettes(State *state)
{
    float t  = SDL_GetTicks() / PHYSICS_FPS;
    int   pi = 0;

    for (int p = 0; p < MAX_STYLES; p++)
        for (int i = 0; i < PALETTE_SIZE; i++)
        {
            float s = sinf(t / 2.0f + p + i) * 2.0f;
            float c = cosf(t / 2.0f + p + i) * 2.0f;
            render_particle_callback((i + 1) * 4 + s,
                                     (p + 1) * 5 + c,
                                     3.0f + s * 0.75f,
                                     pi++,
                                     state);
        }
}

static void render_others(State *state)
{
    static const SDL_Color repeller  = { 255, 0, 0, 255 };
    static const SDL_Color attractor = { 0, 0, 255, 255 };

    for (int i = 0; i < state->ps.emitter_count; i++)
    {
        particle_emitter_t *e = &state->ps.emitters[i];
        if (e->active)
            square(e->x, e->y, 3, &smokey[0].colour, state->renderer);
    }

    for (int i = 0; i < state->ps.repeller_count; i++)
    {
        particle_repeller_t *r = &state->ps.repellers[i];
        if (r->active && r->strength != 0.0f)
            square(r->x, r->y, r->max_distance * 2,
                   (r->strength >= 0) ? &repeller : &attractor,
                   state->renderer);
    }
}

static void render(State *state)
{
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);
    render_palettes(state);
    render_others(state);
    render_particles(&state->ps);
    SDL_RenderPresent(state->renderer);
}

/* -------------------------------------------------------------------------- */

// Physics + mouse trail + idle explosion

static void update(State *state, Uint64 now)
{
    float physics_dt = (now - state->last_physics_time) / state->cps;
    state->last_physics_time = now;
    update_particles(&state->ps, physics_dt);

    if (state->last_mouse_emit_time == (Uint64)-1)
    {
        state->last_mouse_emit_time = now;
    }
    else
    {
        float emit_dt = (now - state->last_mouse_emit_time) / state->cps;
        int   pts     = CLAMP((int)(emit_dt * MOUSE_EMIT_RATE), 0, MOUSE_EMIT_MAX);
        if (pts > 0)
        {
            while (pts-- > 0)
                create_particle(&state->ps,
                                3,
                                state->last_mouse_x, state->last_mouse_y,
                                state->last_mouse_vx * MOUSE_DAMPING,
                                state->last_mouse_vy * MOUSE_DAMPING);
            state->last_mouse_emit_time = now;
        }
    }

    if (!is_active(&state->ps))
        create_explosion(&state->ps, -1,
                         rand() % WIDTH, rand() % HEIGHT,
                         0.0f, 0.0f,
                         NPARTICLES);
}

/* -------------------------------------------------------------------------- */

// Key handlers

static void key_pause(State *s)
{
    s->pause = !s->pause;
}

static void key_reset(State *s)
{
    reset_particle_system(&s->ps);
}

static void key_quit(State *s)
{
    s->quit = 1;
}

static void key_gravity(State *s)
{
    s->ps.flags ^= PARTICLE_FLAG_NO_GRAVITY;
    printf("Gravity %s\n", (s->ps.flags & PARTICLE_FLAG_NO_GRAVITY) ? "disabled" : "enabled");
}

static void key_walls(State *s)
{
    s->ps.flags ^= PARTICLE_FLAG_WALLS;
    printf("Walls %s\n", (s->ps.flags & PARTICLE_FLAG_WALLS) ? "enabled" : "disabled");
}

static void key_fps_delta(State *s, int d)
{
    s->selectedFPS = CLAMP(s->selectedFPS + d, 0, (int)NELEMS(fpses) - 1);
    printf("%dfps\n", fpses[s->selectedFPS]);
}

static void key_fps_dec(State *s) { key_fps_delta(s, -1); }
static void key_fps_inc(State *s) { key_fps_delta(s, +1); }

typedef void (*key_fn_t)(State *);

typedef struct
{
    SDL_Keycode key;
    key_fn_t    fn;
} key_binding_t;

static const key_binding_t key_table[] =
{
    { SDLK_SPACE,        key_pause   },
    { SDLK_DELETE,       key_reset   },
    { SDLK_G,            key_gravity },
    { SDLK_W,            key_walls   },
    { SDLK_Q,            key_quit    },
    { SDLK_LEFTBRACKET,  key_fps_dec },
    { SDLK_RIGHTBRACKET, key_fps_inc },
};

static void handle_key_down(State *state, const SDL_KeyboardEvent *ev)
{
    for (int i = 0; i < (int)NELEMS(key_table); i++)
    {
        if (key_table[i].key == ev->key)
        {
            key_table[i].fn(state);
            return;
        }
    }
    printf("key down: %s\n", SDL_GetKeyName(ev->key));
}

/* -------------------------------------------------------------------------- */

// Mouse event handlers

// Maps mouse button index (button - 1) to particle style.
// Button 1 = random (-1), 2 = fire (0), 3 = fleck (2), 4+ = pastel (3).
static const int button_styles[] = { -1, 0, 2, 3 };

static void handle_mouse_button_down(State *state, const SDL_MouseButtonEvent *ev)
{
    int btn   = ev->button - 1;
    int last  = (int)NELEMS(button_styles) - 1;
    int style = (btn <= last) ? button_styles[btn] : button_styles[last];
    create_explosion(&state->ps, style,
                     ev->x / SCALE, ev->y / SCALE,
                     state->last_mouse_vx, state->last_mouse_vy,
                     state->nparticles);
}

static void handle_mouse_motion(State *state, const SDL_MouseMotionEvent *ev, Uint64 now)
{
    int   mouse_x = ev->x / SCALE;
    int   mouse_y = ev->y / SCALE;
    float vx = 0.0f, vy = 0.0f;

    if (state->last_mouse_time != (Uint64)-1)
    {
        float dt = (now - state->last_mouse_time) / state->cps;
        if (dt > 0.001f)
        {
            vx = (mouse_x - state->last_mouse_x) / dt;
            vy = (mouse_y - state->last_mouse_y) / dt;
        }
    }

    state->last_mouse_time = now;
    state->last_mouse_x    = mouse_x;
    state->last_mouse_y    = mouse_y;
    state->last_mouse_vx   = vx;
    state->last_mouse_vy   = vy;
}

static void handle_mouse_wheel(State *state, const SDL_MouseWheelEvent *ev)
{
    state->nparticles += ev->y * 10.0f;
    state->nparticles = CLAMP(state->nparticles, 1, MAX_PARTICLES);
}

/* -------------------------------------------------------------------------- */

// Event dispatch

static void handle_event(State *state, const SDL_Event *e, Uint64 now)
{
    switch (e->type)
    {
    case SDL_EVENT_QUIT:
        state->quit = 1;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        handle_mouse_button_down(state, &e->button);
        break;
    case SDL_EVENT_KEY_DOWN:
        handle_key_down(state, &e->key);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        handle_mouse_motion(state, &e->motion, now);
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        handle_mouse_wheel(state, &e->wheel);
        break;
    }
}

static void handle_events(State *state, Uint64 now)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
        handle_event(state, &e, now);
}

/* -------------------------------------------------------------------------- */

// Initialisation helpers

static void init_styles(particle_style_t *styles)
{
    float frame_ms = 1000.0f / PHYSICS_FPS;

    set_default_style(&styles[0], frame_ms);
    styles[0].probability   = 90;
    styles[0].palette_index = 0; // Fire
    styles[0].emit_angle    = 270.0f;
    styles[0].emit_range    = 90.0f;

    set_default_style(&styles[1], frame_ms);
    styles[1].probability   = 8;
    styles[1].palette_index = 1; // Smoke
    styles[1].emit_angle    = 270.0f;
    styles[1].emit_range    = 90.0f;
    styles[1].min_life     *= 4;
    styles[1].max_life     *= 4;
    styles[1].vel_scale     = 0.1f;
    styles[1].emit_speed    = 10;
    styles[1].min_size      = 1;
    styles[1].max_size      = 2;
    styles[1].gravity      /= -100.0f;

    set_default_style(&styles[2], frame_ms);
    styles[2].probability   = 2;
    styles[2].palette_index = 2; // Fleck
    styles[2].emit_speed    = 200;

    set_default_style(&styles[3], frame_ms);
    styles[3].probability   = 0;
    styles[3].palette_index = 3; // Pastel
    styles[3].min_life     /= 2;
    styles[3].max_life     /= 2;
    styles[3].emit_speed    = 25;
    styles[3].gravity      /= 2.0f;
}

static void setup_emitters(State *state)
{
    for (int i = 1; i < 5; i++)
        create_emitter(&state->ps,
                       WIDTH * i / 5, HEIGHT * 4 / 5,
                       10.0f,
                       (i - 1.0f) / (4.0f - 1.0f),
                       0.01f + (i - 1) * 0.02f,
                       1, 0);

    create_repeller(&state->ps, WIDTH * 1 / 3, HEIGHT / 2,  100.0f, 5.0f);
    create_repeller(&state->ps, WIDTH * 2 / 3, HEIGHT / 2, -100.0f, 5.0f);
}

static SDL_Window *init_sdl(State *state)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SDL could not initialise! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Window *window = SDL_CreateWindow("Retro Explosion Particle System",
                                          WIDTH * SCALE, HEIGHT * SCALE, 0);
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }

    state->renderer = SDL_CreateRenderer(window, NULL);
    if (state->renderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }

    SDL_SetRenderScale(state->renderer, SCALE, SCALE);
    return window;
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    particle_style_t  styles[MAX_STYLES];
    State            *state = calloc(1, sizeof(*state));

    if (state == NULL)
    {
        printf("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    randpool_init(&state->randpool, 1024 * 4);

    create_gradient_palette(firey,  &state->palettes[PALETTE_SIZE * 0], PALETTE_SIZE);
    create_gradient_palette(smokey, &state->palettes[PALETTE_SIZE * 1], PALETTE_SIZE);
    create_gradient_palette(fleck,  &state->palettes[PALETTE_SIZE * 2], PALETTE_SIZE);
    create_gradient_palette(pastel, &state->palettes[PALETTE_SIZE * 3], PALETTE_SIZE);

    init_styles(styles);

    SDL_Window *window = init_sdl(state);
    if (window == NULL)
    {
        free(state);
        return 1;
    }

    init_particle_system(&state->ps,
                         0,
                         styles,
                         NELEMS(styles),
                         0.2f,
                         rand_callback,
                         get_ticks_callback,
                         render_particle_callback,
                         state);

    setup_emitters(state);

    srand(time(NULL));

    state->nparticles        = NPARTICLES;
    state->selectedFPS       = 6; // 60fps
    state->last_physics_time = SDL_GetPerformanceCounter();
    state->last_mouse_time   = (Uint64)-1;
    state->last_mouse_emit_time = (Uint64)-1;
    state->cps               = (float)SDL_GetPerformanceFrequency();

    while (!state->quit)
    {
        Uint64 now = SDL_GetPerformanceCounter();

        handle_events(state, now);

        if (!state->pause)
            update(state, now);

        render(state);

        Uint64 end       = SDL_GetPerformanceCounter();
        float  elapsedMS = (end - now) / state->cps * 1000.0f;
        float  delay     = 1000.0f / (float)fpses[state->selectedFPS] - elapsedMS;
        if (delay > 0.0f)
            SDL_Delay((Uint32)delay);
    }

    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(state);

    return 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:
