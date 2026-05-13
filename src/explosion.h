// explosion.h
//
// Particle effects
//

#ifndef EXPLOSION_H
#define EXPLOSION_H

/* -------------------------------------------------------------------------- */

// Config
//

#define SCALE         (4)       // screen scale
#define WIDTH         (256)     // screen width (pixels)
#define HEIGHT        (192)     // screen height (pixels)
#define MAX_PARTICLES (1000)    // maximum particles
#define NPARTICLES    (MAX_PARTICLES / 2) // num. particles to spawn on clicks
#define GRAVITY       (0.075f)  // default gravity
#define PHYSICS_FPS   (60)      // FPS for physics (affects values)
#define RENDER_FPS    (30)      // FPS for screen updates
#define PALETTE_SIZE  (8)       // num. palette entries to generate
#define CHANCE_BINS   (16)      // number of bins to use for choosing random styles

/* -------------------------------------------------------------------------- */

/// A single particle
typedef struct particle
{
    int     style;      // stores (style+1); 0 means inactive
    float   x, y;       // position
    float   vx, vy;     // velocity
    Uint32  max_life;   // current life, maximum life in milliseconds
    float   size;       // particle size
    Uint32  created_time; // SDL tick time when particle should become active (milliseconds)
} particle_t;

/// A particle style
typedef struct particle_style
{
    int     probability; // relative chance of use
    int     min_life, max_life; // milliseconds
    float   vel_scale;  // velocity scale factor
    float   emit_angle; // degrees (0/90/180/270 is right/down/left/up)
    float   emit_range; // degrees
    int     emit_speed; // 200 is fast
    int     min_size, max_size; // pixels
    float   max_delay;  // milliseconds
    float   gravity;    // pixels/second² (converted from frame-based)
    float   size_decay; // factor (per-millisecond)
    SDL_Color *palette;
} particle_style_t;

/// A particle system
typedef struct particle_system
{
    // config
    const particle_style_t *styles;
    int     nstyles;

    // state
    particle_t particles[MAX_PARTICLES];
    int     active_count;
    char    chance[CHANCE_BINS];
} particle_system_t;

/* -------------------------------------------------------------------------- */

void reset_particle_system(particle_system_t *ps);

void init_particle_system(particle_system_t      *ps,
                          const particle_style_t *styles,
                          int                     nstyles);

void update_particles(particle_system_t *ps, float dt);

void set_default_style(particle_style_t *style,
                       float             frame_ms,
                       SDL_Color        *palette);

void create_explosion(particle_system_t *ps,
                      int                cx,
                      int                cy,
                      int                particle_count,
                      int                force_style);

void render_particles(particle_system_t *ps, SDL_Renderer *renderer);

int is_active(const particle_system_t *ps);

#endif // EXPLOSION_H

// vim:sw=4:sts=4:ts=8:tw=78:
