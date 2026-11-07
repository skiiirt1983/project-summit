# firmware/sim — PC simulator

The shell, framework and placeholder panels on LVGL 9.4 (same version as the device), built for the host.

    cmake -S firmware/sim -B build/sim -G Ninja && cmake --build build/sim
    build/sim/summit_sim --selftest --out sim-out   # headless checks + PNG screenshots (CI runs this)
    build/sim/summit_sim --sdl                      # window; needs libsdl2-dev at build time

Window keys: R rotate (via the 400 ms debounce), T toast, F full-screen mode, Q quit.
Mouse = touch: swipe pages, tap chips, swipe down / long-press the strip.

Headless mode uses a virtual clock and a scripted pointer, so gestures go through LVGL's real input
handling and runs are deterministic. It shows the shell's layout and behaviour on a PC only: frame rate,
touch feel and rotation on the real panel still need the Phase 1 hardware spike.
