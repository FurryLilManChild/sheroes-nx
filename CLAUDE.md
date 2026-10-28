# sheroes-nx: notes for Claude

Native PC and Switch port of Sonic Heroes (original Xbox, NTSC-U, title 5345002B, XDK 5659) via xboxrecomp static
recompilation (x86 to C). The repo started as a copy of CoD-nx (FurryLilManChild/cod-nx, Castlevania: Curse of
Darkness), which is itself built on antoxa2584x/burnout3-sw (MIT): its vendored xboxrecomp (the NFS Switch fork: NV2A
Vulkan and OpenGL renderers, Horizon support), `src/main.c`, `src/switch_nx.c`, `src/switch_saltynx.c`,
`switch/build.sh` and `tools/regen.sh`. Do not copy code from nfsu1-sw or nfsu2-sw: they have no license.

The big difference from CoD-nx: Sonic Heroes links XDK 1.0.5659, not 5849, so CoD-nx's D3D and DirectSound hooks
(`src/recomp_manual.c` there) have to be found again at this game's addresses, and the kernel may need an ordinal
remap (`xboxrecomp/tools/kernel_audit/gen_ordinal_remap.py`). The engine is RenderWare (the toolkit's easy case;
Burnout 3 is RenderWare too) with CRI middleware (Sofdec movies, ADX audio; the PSFD and DOLPHIN sections).

## Who is working on this

The owner (Mark) is very tech savvy but has almost no coding experience. Claude writes the code and scripts. Mark
runs builds on his own machines, tests on the PC and his Switch, and sends logs. Instructions for him must be numbered,
click by click, with copy-paste commands. Never use em dashes in anything written for him or in the docs. No
decorative emoji.

His hardware: a Windows PC (Lenovo Legion Go), an unpatched V1 Switch on Atmosphere CFW (sys-clk), an Xbox 360 with
Aurora. His disc image is `C:\sheroes-nx\assets\Sonic Heroes (USA) (En,Ja,Fr,De,Es,It).xiso.iso` (a trimmed XISO:
the game partition starts at offset 0); the extracted disc is `C:\sheroes-nx\assets\game_root`. A cloud session
reaches both through the device bridge. He moves files to the Switch's SD card himself over USB; he does not want
Claude to control the Switch's folders. He drops Switch logs in `C:\sheroes-nx\out\sd\switch\switchlogs`.

Workflow (Mark's choice, 2026-10-28): as on the Xbox 360 ports. Mark builds on his PC in PowerShell
(`tools\win\build_pc.ps1`), runs boot tests and play tests, Claude reads `C:\sheroes-nx\out\logs` through the
bridge and pushes fixes. Get the game working on the PC first, then the Switch. Avoid long cloud test runs (the cloud
Linux build on lavapipe needs about 7 minutes to reach a stage).
Priority (Mark, 2026-10-28): playability first. Get a stage running and playable before cosmetic issues (the team
select models, missing letters in text).

Git: commit as `FurryLilManChild <346934328+FurryLilManChild@users.noreply.github.com>`. History rewrites and force
pushes need his explicit consent.

## Hard rules

- Never commit game data or anything derived from it: the disc image, `default.xbe`, `assets/`, the generated C
  (`out/gen/`), `*_analysis.json`, the toolkit's `xboxrecomp/tools/*/output/`, frame dumps, shader caches,
  `switch_sd/`. `.gitignore` covers these; check `git status` before every commit.
- Every hand-written override in `src/recomp_manual.c` and every seed in `config/seed_functions.json` carries its
  evidence (which run or tool found it, callers, addresses).
- Log each session in `docs/progress-log.md` (newest first).

## Layout

| Where | What |
|---|---|
| `xboxrecomp/` | vendored toolkit (burnout3-sw's copy via CoD-nx, MIT); its tools write to `xboxrecomp/tools/*/output/` (ignored) |
| `src/main.c` | boot: memory layout, kernel, APU, USB, renderer, entry point |
| `src/recomp_manual.c` | hand-written overrides (`recomp_lookup_manual`) |
| `src/sheroes_menu.c` | the port menu (+ and -) and `sheroes_options.txt` |
| `src/switch_nx.c` | Horizon: log, `sheroes_env.txt`, loading screen, profiler, clocks |
| `config/seed_functions.json` | entry points the static pass misses |
| `tools/regen.sh` | XBE to C: disasm, func_id, abi_analysis, recomp (into `out/gen/`) |
| `switch/build.sh` | the Switch NRO (Vulkan with NVK by default; `VULKAN=0` for OpenGL) |
| `tools/win/*.ps1` | the Windows PC build on Mark's PC (same Vulkan renderer as the Switch): `build_pc.ps1`, `boot_test.ps1`, `play.ps1`; logs in `out\logs\` (docs/windows-build.md) |
| `tools/linux/setup_switch_wine.sh` | devkitPro's Windows install under Wine on a cloud machine |
| `assets/game_root/` | the extracted disc (ignored) |

## The game

- `default.xbe`: base 0x00010000, entry 0x0027972B, kernel thunks 0x003ECD0C, image end 0x00752F90. Sections .text
  (0x11000, 0x319E8C bytes), D3D, DSOUND, PSFD00, PSFD_I, PSFD_B, PSFD_P, DOLPHIN, D3DX, XGRPH, XPP, .rdata, .data
  (0x003DC600, 0x36BA4C: a large BSS), DOLBY. Libraries XAPILIB, D3D8, DSOUND, XBOXKRNL, LIBCMT, D3DX8, XGRAPHC, all
  1.0.5659. SHA-256 5a9637976b147d22d2ea845c178379f97a3df9040ee41efe670a37298129c5c6.
- Switch paths: `sdmc:/switch/sheroes/` (the NRO, `sheroes_env.txt`, `sheroes_log.txt`), the extracted disc in
  `sdmc:/switch/sheroes/game/`.
