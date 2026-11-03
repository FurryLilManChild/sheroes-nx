# sheroes-nx

A work-in-progress native Nintendo Switch (and PC) port of **Sonic Heroes** (original Xbox, USA, 2004), made with
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) static recompilation, on the Switch runtime of
[burnout3-sw](https://github.com/antoxa2584x/burnout3-sw). It started as a copy of
[CoD-nx](https://github.com/FurryLilManChild/CoD-nx).

It is not an emulator: the x86 code of the game's `default.xbe` is translated to C and compiled for the Switch and
the PC. The Xbox's graphics chip (NV2A) is drawn by a Vulkan renderer, and its sound chip by xemu's APU code.

## Status

**Version 0.2.0 is a pre-release for the Switch.** It boots, plays the menus, the movies and the first stage, with
the graphics right (textures, lighting, character models).

**It needs the CPU boost to run at full speed.** At the Switch's stock CPU clock (1020 MHz) the game runs at about
40-48 frames a second, and since the game times itself by frames, everything plays slower than it should and the
sound lags. With the port menu's **CPU boost** (1785 MHz, a clock Nintendo itself uses for boost mode) it reaches
55-60. Making it fast enough at stock clocks is the next goal.

| Stage | State |
| --- | --- |
| Code translation | Done (`tools/regen.sh`) |
| Switch build | Playable with CPU boost (Vulkan); first stage tested |
| PC build | Windows (Vulkan) runs and plays the first stage; for development, no release yet |

Session-by-session notes: [docs/progress-log.md](docs/progress-log.md).

### Known issues

- Slow at stock clocks: turn on **CPU boost** in the port menu (takes effect after a restart).
- Only the first stage has been played. Anything past it is untested.
- A dark screen for a while at startup. 0.2.0 made it much shorter (the port now lists the disc once instead of
  opening every file on the SD card), but it is not instant.
- Start the game with title takeover (hold R while opening a game, then pick Sonic Heroes). The album applet does
  not have enough memory.
- The aspect ratio and CPU boost settings take effect after a restart.

## Download and install (Switch)

You need a Switch with custom firmware (Atmosphere) and your own copy of the game: the USA Xbox disc, dumped as an
`.iso`.

1. Download `sheroes-nx-v0.2.0-switch.zip` from this repository's **Releases** page.
2. Extract the zip to the root of your SD card. It creates `switch/sheroes/` with `sheroes.nro` in it.
3. Extract the files from your disc image with [extract-xiso](https://github.com/XboxDev/extract-xiso), for example
   `extract-xiso -x "Sonic Heroes (USA).iso"`, which makes a folder of the disc's files.
4. Copy everything from that folder (`default.xbe` and the `DVD` folder and the rest, about 3.9 GB) into
   `switch/sheroes/game/` on the SD card.
5. On the Switch, hold **R** while opening any game to get the homebrew menu with full memory, and start
   **Sonic Heroes (Xbox recomp)**.
6. Press **+ and -** together, turn on **CPU boost**, choose **Save and close**, and restart the game.

The supported `default.xbe` has this SHA-256:
`5a9637976b147d22d2ea845c178379f97a3df9040ee41efe670a37298129c5c6`.

### Reporting problems

Open an issue with where in the game it happened, what you saw, whether CPU boost was on, your Atmosphere version,
and the log files from `sdmc:/switch/sheroes/`: `sheroes_log.txt` and `xbox_kernel.log`.

## The port menu

Press **+ and -** together to open it (the game pauses). D-pad or left stick to move, left/right or A to change, B to
close. Settings are saved to `sheroes_options.txt` next to the program (`sdmc:/switch/sheroes/` on the Switch):

| Setting | Choices |
| --- | --- |
| Button layout | Xbox positions (default), Switch labels, or any Switch button for each Xbox button (A, B, X, Y, the two shoulder buttons, both triggers) |
| Invert camera | left/right and up/down, separately (right stick) |
| Aspect ratio | 4:3 (default) or 16:9, after a restart |
| Hide blurry edges | off (default), 16 lines, 24 lines |
| Resolution | native (default), 1.5x, 2x |
| Smoothing (FXAA), texture filtering, FPS counter | on/off, off/4x/16x, on/off |
| Music volume | 100% to 300% (default 200%), on top of the game's own setting |
| CPU boost | off (default) or on: the CPU at 1785 MHz while the game runs, after a restart |

## Building

The game's code is generated from your own `default.xbe`, so there is nothing to build without the game.

1. Extract the disc into `assets/game_root` (the `assets/` folder never goes into git).
2. `tools/regen.sh` translates `default.xbe` to C in `out/gen/`.
3. For the Switch: devkitPro with switch-sdl2, the NVK Vulkan driver and glslang built for the Switch (see
   `switch/build.sh`). Then `switch/build.sh`.
4. For Windows: [docs/windows-build.md](docs/windows-build.md).

## Layout

| Folder | Contents |
| --- | --- |
| `src/` | The game project: boot (`main.c`), hand-written fixes (`recomp_manual.c`), the port menu, the Switch platform layer |
| `xboxrecomp/` | The recompilation toolkit and its runtime (kernel, Direct3D 8, NV2A renderers, audio, input) |
| `config/` | Entry points the translator needs to be told about |
| `tools/` | Code generation (`regen.sh`), profiling report, Windows build scripts |
| `switch/` | The Switch build script and icon |
| `docs/` | Progress log, Windows build notes |

## Credits

- xboxrecomp by sp00nznet (MIT; the xemu parts LGPL 2.1)
- burnout3-sw by antoxa2584x (MIT): the Switch runtime, NV2A Vulkan and OpenGL renderers
- mcdub3-nx by laureladastra (MIT): the NV2A 2D engine blit
- CoD-nx: the port this one started from
- Font: dhepper/font8x8 (public domain)

## Legal

No affiliation with SEGA, Sonic Team, Microsoft or Nintendo. "Sonic Heroes" and "Sonic the Hedgehog" are trademarks
of SEGA. This repository contains no assets or program code from the original game. Users must provide their own
legally obtained copy, and everything made from it (`assets/`, `out/`, the generated C, shader caches) is made on
their own computer. Running homebrew requires custom firmware, which violates Nintendo's terms of service.

MIT License ([LICENSE](LICENSE)); third-party parts in [NOTICE.md](NOTICE.md).
