// gradient.h
//
// Gradient palette generation
//

#ifndef GRADIENT_H
#define GRADIENT_H

#include <SDL3/SDL.h>

// A gradient colour stop
typedef struct gradientstop
{
    SDL_Color colour;
    float     stop;
} gradientstop_t;

// Create a gradient palette from colour stops
void create_gradient_palette(const gradientstop_t *colours,
                             SDL_Color            *palette,
                             int                   palette_size);

#endif // GRADIENT_H

// vim:sw=4:sts=4:ts=8:tw=78:
