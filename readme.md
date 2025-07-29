# LCDBoy

This is a half-working GameBoy emulator cobbled together in C

Currently it can run:
- Tetris
- Dr. Mario
- Super Mario Land
- Pokemon Blue (has graphics issues)

It may seem like it's not running at first, but that's because the PPU implementation (`display.c`) is hot garbage which causes everything to run at ~1FPS

Other than that, the CPU emulation should be "perfect". It passes all of blargg's cpu_instrs tests

PS. I don't know if this compiles on Windows, but it theoretically should with a bit of effort
