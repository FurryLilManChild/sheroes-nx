# sheroes-nx: notes for Claude (branch xbe-recomp)

Native PC and Switch port of Sonic Heroes (original Xbox, NTSC-U, title 5345002B, XDK 5659), on this branch rebuilt
on TGP482's XboxXBE-Recomp (`sdk/`, MIT) by way of xsaja/brextreme-nx (Bloody Roar Extreme, GPL-3), itself built on
street-nx and mvp-nx, whose Switch layer comes from gerisson1996/BLACK-NX-PORT (no license; Mark: fine to build on).
The x86 code is translated to C++ (`xbe_recomp`), and Direct3D 8 and DirectSound are replaced at the API level
(`sdk/runtime/xdk`): no NV2A command stream, a render thread replays recorded draws with deko3d or Vulkan. Why, and
the measurements behind it: `docs/strategy-review.md`, `docs/feasibility-xbe-recomp.md`. The xboxrecomp version of
the port (0.2.0) stays on `main`; its notes are in `docs/xboxrecomp-era/` and are the knowledge base for this game
(the D3D and DSOUND addresses at 5659, the RenderWare fixed-function needs, the disc's file count, CRI middleware).

License: GPL-3 (`LICENSE`), with the components listed in `LICENSES.txt`. Code from main (MIT) keeps its notices.

Upstream docs carried over: `docs/brextreme/` (how its SDK, PGO, the profiler and the XDK 5233 matching work).
Comments marked `// STREET:` and `// BREXTREME:` say which port made a change; keep them.

## Who is working on this

The owner (Mark) is very tech savvy but has almost no coding experience. Claude writes the code and scripts. Mark
runs builds on his own machines, tests on the PC and his Switch, and sends logs. Instructions for him must be numbered,
click by click, with copy-paste commands. Never use em dashes in anything written for him or in the docs. No
decorative emoji.

His hardware: a Windows PC (Lenovo Legion Go), an unpatched V1 Switch on Atmosphere CFW (sys-clk), an Xbox 360 with
Aurora. His disc image is `C:\sheroes-nx\assets\Sonic Heroes (USA) (En,Ja,Fr,De,Es,It).xiso.iso` (a trimmed XISO);
the extracted disc is `C:\sheroes-nx\assets\game_root`. A cloud session reaches both through the device bridge. He
moves files to the Switch's SD card himself over USB; he does not want Claude to control the Switch's folders. He
drops Switch logs in `C:\sheroes-nx\out\sd\switch\switchlogs`.

Workflow: Mark builds on his PC, runs boot tests and play tests, Claude reads the logs through the bridge and pushes
fixes. PC first, then the Switch. Do not start cloud test runs that run the game on your own; build checks are fine.
Present findings and the next build's plan, and wait for his go-ahead before each test build. Goal: 60 fps at stock
clocks (Mark, 2026-11-09).

Git: commit as `FurryLilManChild <346934328+FurryLilManChild@users.noreply.github.com>`. History rewrites and force
pushes need his explicit consent.

## Hard rules

- Never commit game data or anything derived from it: the disc, `default.xbe`, `assets/`, `game/assets/`,
  `game/generated/`, `out/`, frame dumps, shader caches, `switch_sd/`. Check `git status` before every commit.
- Every hand-named XDK function in `game/config/game.toml` and every value in `sdk/xdk/5659/layout.toml` carries its
  evidence (the 5659 address and instruction that proves it).
- Log each session in `docs/progress-log.md` (newest first).

## Layout

| Where | What |
|---|---|
| `sdk/` | XboxXBE-Recomp: `tools/recomp` (xbe_recomp), `tools/symbols` (xbe_symbols), `runtime/` (kernel, D3D8, DSound, render thread), `xdk/<build>/` (signatures and layouts) |
| `game/config/game.toml` | the recompiler's input for Sonic Heroes: sections, hand-named functions, `[symbols]` |
| `game/src/` | game hooks (`X86_FUNC(sub_...)` replaces a recompiled function) and `game_info.cpp` |
| `pc/` | the Windows host (D3D12 and Vulkan exes) |
| `switch/` | the Switch host (deko3d and Vulkan renderers, audio, input, profiler, PGO) |
| `tools/build_pc.cmd` | the Windows build: tools, game.toml, recompile into game/generated, PC exes |
| `switch/build.sh` | the Switch NRO |

## The game

- `default.xbe`: base 0x00010000, entry 0x0027972B. Sections .text (0x11000), D3D (0x32AEA0-0x33E428), DSOUND
  (0x33E440-0x35AA54), PSFD00/_I/_B/_P (CRI Sofdec), DOLPHIN, D3DX, XGRPH, XPP, .rdata, .data, DOLBY. Libraries
  XAPILIB, D3D8, DSOUND, XBOXKRNL, LIBCMT, D3DX8, XGRAPHC, all 1.0.5659. SHA-256
  5a9637976b147d22d2ea845c178379f97a3df9040ee41efe670a37298129c5c6.
- Switch paths: `sdmc:/switch/sheroes/` (the NRO, `sheroes-settings.toml`, `logs/`), the extracted disc in
  `sdmc:/switch/sheroes/game/` (the same game folder as the 0.x releases).
