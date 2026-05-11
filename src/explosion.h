// explosion.h
//
// Particle effects
//

#ifndef EXPLOSION_H
#define EXPLOSION_H

/* -------------------------------------------------------------------------- */

// Config
#define SCALE         (4)
#define WIDTH         (256)
#define HEIGHT        (192)
#define MAX_PARTICLES (1000)
#define NPARTICLES    (MAX_PARTICLES / 2)
#define GRAVITY       (0.075f)
#define PHYSICS_FPS   (60)
#define RENDER_FPS    (30)
#define PALETTE_SIZE  (8)

/* -------------------------------------------------------------------------- */

// Particle structure
typedef struct particle
{
    int     style;      // stores (style+1); 0 means inactive
    float   x, y;       // position
    float   vx, vy;     // velocity
    float   life;       // current life in milliseconds (life > max_life means don't render yet)
    float   max_life;   // maximum life in milliseconds
    float   size;       // particle size
    Uint32  created_time; // SDL tick time when particle was created (milliseconds)
    float   initial_life; // initial total life including delay
    Uint32  last_update_time; // last time physics were updated
} particle_t;

// Particle style
typedef struct particle_style
{
    float   min_life;   // milliseconds [TIME]
    float   max_life;   // milliseconds [TIME]
    float   vel_scale;  // factor
    float   emit_angle; // degrees (0/90/180/270 is right/down/left/up)
    float   emit_range; // degrees
    float   emit_speed; // 200 is fast
    int     min_size;   // pixels
    int     max_size;   // pixels
    float   delay;      // milliseconds [TIME]
    float   gravity;    // per millisecond
    float   size_decay; // factor (per-millisecond)
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

void create_gradient_palette(const gradientstop_t *colours,
                             SDL_Color            *palette,
                             int                   paletteSize);

/* -------------------------------------------------------------------------- */

void init_particle_system(particle_system_t      *ps,
                          const particle_style_t *styles,
                          int                     smoke_pc);

void update_particles(particle_system_t *ps);

void create_explosion(particle_system_t *ps,
                      int                cx,
                      int                cy,
                      int                particle_count,
                      int                create_additional);

void render_particles(SDL_Renderer *renderer, particle_system_t *ps);

int is_active(particle_system_t *ps);

#endif // EXPLOSION_H

// vim:sw=4:sts=4:ts=8:tw=78:
