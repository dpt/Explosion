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

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))
#define CLAMP(a,min,max) ((a) < (min) ? (min) : (a) > (max) ? (max) : a)

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

    static const gradientstop_t fleck[] =
    {
        { {   0, 255,   0, 255 }, 0.0f }, // Green
        { {   0, 127,   0, 255 }, 0.5f }, // Dark Green
        { {   0,   0,   0, 255 }, 1.0f }, // Black
    };

    // Frames/sec we'll allow for refresh
    static const int fpses[] = {
        1, 2, 5, 10, 15, 30, 60, 120, 240, 480, 960
    };

    SDL_Color         fire_palette[PALETTE_SIZE];
    SDL_Color         smoke_palette[PALETTE_SIZE];
    SDL_Color         fleck_palette[PALETTE_SIZE];
    particle_style_t  styles[3];
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

    create_gradient_palette(firey, &fire_palette[0], PALETTE_SIZE);
    create_gradient_palette(smokey, &smoke_palette[0], PALETTE_SIZE);
    create_gradient_palette(fleck, &fleck_palette[0], PALETTE_SIZE);

    // Convert frame-based values to millisecond-based values
    // 1 frame = 1000/60 ms
    float frame_ms = 1000.0f / PHYSICS_FPS;

    // Set default styles
    set_default_style(&styles[0], frame_ms, fire_palette);
    set_default_style(&styles[1], frame_ms, smoke_palette);
    set_default_style(&styles[2], frame_ms, fleck_palette);

    // Set differences from defaults
    styles[0].probability = 90;
    styles[0].emit_angle  = 270.0f; // point up
    styles[0].emit_range  = 90.0f;  // quarter circle

    styles[1].probability = 8;
    styles[1].min_life    = 120 * frame_ms;
    styles[1].max_life    = 180 * frame_ms;
    styles[1].vel_scale   = 0.1f;
    styles[1].emit_speed  = 50;
    styles[1].max_size    = 2;
    styles[1].gravity     = styles[0].gravity * 0.025f; // pixels/second/second

    styles[2].probability = 2;
    styles[2].emit_speed  = 200;

    // Initialize particle system
    init_particle_system(&ps, styles, NELEMS(styles));

    // Create initial explosion
    create_explosion(&ps,
                     WIDTH / 2, HEIGHT / 2,
                     NPARTICLES, -1,
                     0);

    srand(time(NULL));

    // Game loop
    int quit = 0;
    int pause = 0;
    int nparticles = NPARTICLES;
    int selectedFPS = 6; /* 60fps */
    Uint64 last_physics_time = SDL_GetPerformanceCounter();

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
                    create_explosion(&ps,
                                     e.button.x / SCALE, e.button.y / SCALE,
                                     nparticles, -1,
                                     1);
                    break;
                case 2:
                    create_explosion(&ps,
                                     e.button.x / SCALE, e.button.y / SCALE,
                                     nparticles, 0,
                                     0);
                    break;
                case 3:
                    create_explosion(&ps,
                                     e.button.x / SCALE, e.button.y / SCALE,
                                     nparticles, 2,
                                     0);
                    break;
                }
                break;

            case SDL_EVENT_KEY_UP:
                switch (e.key.key)
                {
                case SDLK_SPACE:
                    pause = !pause;
                    break;
                case SDLK_B:
                    // Enable alpha blending
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    break;
                case SDLK_G:
                    // Disable gravity
                    styles[0].gravity = styles[1].gravity = 0.0f;
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
                    printf("key up: %s\n", SDL_GetKeyName(e.key.key));
                    break;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                create_explosion(&ps,
                                 e.button.x / SCALE, e.button.y / SCALE,
                                 1, 1,
                                 0);
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
            update_particles(&ps, dt);

            // Add more particles when idle
            if (!is_active(&ps))
                create_explosion(&ps,
                                 rand() % WIDTH, rand() % HEIGHT,
                                 NPARTICLES, -1,
                                 0);
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

        float elapsedMS = (end - start) / (float) SDL_GetPerformanceFrequency() * 1000.0f;

        // Cap to renderFPS (not PHYSICS_FPS)
        SDL_Delay(floorf(1000.0f / (float) fpses[selectedFPS] - elapsedMS));
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

// vim:sw=4:sts=4:ts=8:tw=78:
