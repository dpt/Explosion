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
#define PALETTE_SIZE  (8)       // num. palette entries to generate
#define CHANCE_BINS   (16)      // number of bins to use for choosing random styles
#define MAX_EMITTERS  (10)      // maximum emitters

/* -------------------------------------------------------------------------- */

// Common macros
//

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))
#define CLAMP(a,min,max) ((a) < (min) ? (min) : (a) > (max) ? (max) : a)

/* -------------------------------------------------------------------------- */

// Callback function pointer for obtaining random values
typedef unsigned int (*particlerand_t)(void);

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

/// An particle_emitter that regularly outputs particles
typedef struct particle_emitter
{
    int     active;          // 1 if active, 0 if inactive
    float   x, y;            // position
    float   emission_rate;   // particles per second
    float   emission_jitter; // emission randomness factor
    float   emission_clump;  // emission clumping factor
    int     style;           // particle style (-1 for random)
    Uint32  lifetime;        // total lifetime in milliseconds
    Uint32  start_time;      // SDL tick time when particle_emitter started
    Uint32  last_emit_time;  // last time a particle was emitted
} particle_emitter_t;

typedef unsigned int particle_system_flags_t;

#define PARTICLE_FLAG_WALLS (1 << 0) // particles bounce off walls
#define PARTICLE_FLAG_NO_GRAVITY (1 << 1) // particles are not affected by gravity

/// A particle system
typedef struct particle_system
{
    // config
    particle_system_flags_t flags;
    const particle_style_t *styles;
    int     nstyles;
    particlerand_t randfn;  // Callback to obtain random values
    float   wall_damping;

    // state
    particle_t particles[MAX_PARTICLES];
    char    chance[CHANCE_BINS];
    particle_emitter_t emitters[MAX_EMITTERS];
    int     emitter_count;               // Number of active emitters
    int     free_indices[MAX_PARTICLES]; // Stack of available particle indices
    int     free_count;                  // Number of free indices available
} particle_system_t;

/* -------------------------------------------------------------------------- */

/// Resets the particle system to its initial state, deactivating all particles.
void reset_particle_system(particle_system_t *ps);

/// Initialises the particle system with the given styles and random callback.
void init_particle_system(particle_system_t      *ps,
                          particle_system_flags_t flags,
                          const particle_style_t *styles,
                          int                     nstyles,
                          particlerand_t          randfn,
                          float                   wall_damping);

/// Updates all active particles in the system based on the elapsed time [dt].
void update_particles(particle_system_t *ps, float dt);

/// Sets default values for a particle style.
void set_default_style(particle_style_t *style,
                       float             frame_ms,
                       SDL_Color        *palette);

/// Creates a single particle in the system with the specified style, initial
/// position and additional velocity offset.
void create_particle(particle_system_t *ps,
                     int                style,
                     int                cx,
                     int                cy,
                     float              vx,
                     float              vy);

/// Creates an explosion at the specified centre coordinates with the given
/// particle count and optional forced style.
///
/// If [style] is -1, styles are chosen randomly based on their
/// probabilities.
void create_explosion(particle_system_t *ps,
                      int                style,
                      int                cx,
                      int                cy,
                      float              vx,
                      float              vy,
                      int                particle_count);

/// Renders all active particles in the system using the provided SDL renderer.
void render_particles(particle_system_t *ps, SDL_Renderer *renderer);

/// Checks if the particle system has any active particles.
int is_active(const particle_system_t *ps);

/// Creates an particle_emitter at the specified position with given parameters.
void create_emitter(particle_system_t *ps,
                    float              x,
                    float              y,
                    float              emission_rate,
                    float              emission_jitter,
                    float              emission_clump,
                    int                style,
                    Uint32             lifetime);

/// Updates all active emitters, potentially spawning particles.
void update_emitters(particle_system_t *ps, Uint32 current_time);

/// Deactivates an particle_emitter by index.
void destroy_emitter(particle_system_t *ps, int index);

#endif // EXPLOSION_H

// vim:sw=4:sts=4:ts=8:tw=78:
