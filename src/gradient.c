// gradient.c
//
// Gradient palette generation
//

#include <math.h>

#include <SDL3/SDL.h>

#include "gradient.h"

// Function to interpolate between two colours
static SDL_Color interpolate_colour(SDL_Color a, SDL_Color b, float t)
{
    SDL_Color result;

    result.r = (Uint8)(a.r + (b.r - a.r) * t);
    result.g = (Uint8)(a.g + (b.g - a.g) * t);
    result.b = (Uint8)(a.b + (b.b - a.b) * t);
    result.a = (Uint8)(a.a + (b.a - a.a) * t);
    return result;
}

// Create a gradient palette
void create_gradient_palette(const gradientstop_t *colours,
                             SDL_Color            *palette,
                             int                   palette_size)
{
    float t;
    int   seg;
    float seg_start;
    float seg_end;
    float seg_t;

    for (int i = 0; i < palette_size; i++)
    {
        t = (float)i / (palette_size - 1);

        // Find which segment this index falls into
        for (seg = 0; ; seg++)
            if (t >= colours[seg + 0].stop && t <= colours[seg + 1].stop)
                break;

        // Calculate interpolation factor within segment
        seg_start = colours[seg + 0].stop;
        seg_end   = colours[seg + 1].stop;
        seg_t     = (t - seg_start) / (seg_end - seg_start);

        // Interpolate between colours
        palette[i] = interpolate_colour(colours[seg + 0].colour,
                                        colours[seg + 1].colour,
                                        seg_t);
    }
}

// vim:sw=4:sts=4:ts=8:tw=78:
