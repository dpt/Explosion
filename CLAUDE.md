# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a retro particle explosion system built with SDL3. It creates a particle effects playground with various explosion types, particle behaviors, and interactive controls.

## Key Components

- `main.c`: Main application loop with SDL3 initialization, event handling, and rendering
- `explosion.c`/`explosion.h`: Core particle system implementation including particle creation, physics, and rendering
- `gradient.c`/`gradient.h`: Gradient palette generation for color transitions
- `random-pool.c`/`random-pool.h`: Efficient random number generation using a pre-computed pool

## How to Build and Run

Build:
```
cmake -B build -S .
cmake --build build
```

Run:
```
./build/Explosions
```

## Controls

- Click with mouse button 1: Create explosion with random weighted style
- Click with mouse button 2: Create explosion with fire particles
- Click with mouse button 3: Create explosion with fleck particles
- Click with mouse button 4+: Create explosion with pastel particles
- Mouse wheel: Adjust number of particles spewed on clicks
- Space: Pause/resume animation
- Delete: Reset particle system
- G: Toggle gravity
- W: Toggle walls (particles bounce off edges)
- Q: Quit
- []: Change frame rate (1-960fps)

## Customization

- Modify particle setup in `main.c`
- Adjust configuration in `explosion.h`
- Change color palettes in `main.c` (firey, smokey, fleck, pastel)

## Development Notes

The particle system uses a stack-based free list for efficient particle allocation. Particles have physics including gravity, velocity, and lifetime. The system supports multiple particle styles with different behaviors and visual properties.

The codebase uses SDL3 for graphics and input handling, with a focus on performance and retro visual aesthetics.

## Particle Styles

- Firey: Warm, orange-red flames with a gradual fade to dark blue (high probability in random mode)
- Smoke: Orange to grey to black gradient with extended lifetime; used by emitters, not directly clickable
- Fleck: White to yellow to green to dark blue to black with small particles
- Pastel: Lemon to peach to pink to mauve gradient with short-lived, slow-falling particles

## Emitters and Repellers

The system supports static emitters (continuously spawn particles) and repellers (push particles away). Repellers with negative strength act as attractors (pull particles in). Both are drawn as coloured boxes: emitters in smokey orange, repellers in red, attractors in blue.