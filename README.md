# zenrelax

A soothing terminal screensaver written in pure C. Five visual modes render calming animations directly in your terminal using ANSI escape codes and 256-color palettes.

## Modes

1. **Plasma Wave** - Layered sine/cosine interference patterns
2. **Fractal Mandelbrot** - Slowly drifting Julia set exploration
3. **Particle Physics** - 128 particles with gravity, drag, and wave forces
4. **Quantum Flow** - Flowing field visualization with layered oscillations
5. **Orbital Harmony** - 12 orbiting bodies with pulsating glow effects

## Build

```
gcc zenrelax.c -o zenrelax -lm
```

## Usage

```
./zenrelax         # interactive mode selector
./zenrelax 3       # jump straight to Particle Physics
```

Press `q`, `ESC`, or `Ctrl+C` to quit.

## Features

- Tmux-aware with SIGWINCH resize handling
- Alt screen buffer (restores terminal on exit)
- Raw input mode for responsive key handling
- Runs at ~20fps with buffered output
