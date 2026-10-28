# Progress log (newest first)

## 2026-10-28 (later): the first PC builds

- Build 1: glslang installed into a folder named `$glslangOut` (Windows PowerShell 5.1 does not expand a variable in
  an unquoted `-DNAME=$var` argument), so configure could not find it. Quoted; the stray folder is removed.
- Build 2: the APU failed under clang on Windows: qemu_shim.h redefined __builtin_ctz/clz for _MSC_VER (clang defines
  _MSC_VER too and has them built in), and M_PI/M_E needed _USE_MATH_DEFINES, set by the toolkit for MSVC only.
- tools/linux/wincheck.sh: a Windows compile and link check in the cloud (clang for x86_64-pc-windows-msvc, MinGW's
  headers in place of Microsoft's, empty system libraries). It found the rest before Mark's next build: setenv and
  unsetenv in usb_gamepad.c (now _putenv_s), clock_gettime in nv2a_pb_exec.c and sheroes_menu.c
  (platform/win32_clock.h, QueryPerformanceCounter), and xbox_guest_pin / xbox_nx_* only defined in the POSIX
  win32_compat.c (platform/win32_host.c on Windows, the one-core pin opt-in as on Linux). All 156 objects compile;
  the link leaves only Windows and C runtime functions.

## 2026-10-28: Switch tests, the Windows Vulkan build

- Switch test 1 (b5ec2c45): black screen, the log quiet after 7 s of loading. Switch test 2 (d957eb02,
  RECOMP_QUIET=0): boot, the attract demo's files at 23 s, the renderer up only at 46 s, then 0.0 to 0.1 fps with
  NOW LOADING showing; 38.7 million RtlEnterCriticalSection / RtlLeaveCriticalSection calls (ordinals 277/294) in
  283 s: a lock the threads spin on under Horizon's scheduler. Parked until the PC build works.
- Linux, the attract demo: the stage loads, but a black full-screen quad drawn first every frame (z 0.001, depth
  write on, LEQUAL, alpha test passes: diffuse alpha) blocks everything after it; skipping it
  (RECOMP_DBG_SKIPNEARQUAD=1) shows the sky, the sea and Team Sonic, with the stage geometry white. Ruled out: float
  or W depth (CONTROL0 0x00100001, Z24S8), stencil (off), SET_CLIP_MIN/MAX (0..16777215; clip support added
  anyway), the cull state (0x405 back, 0x901 CCW front; flipping it turns the models inside out), fog, the render
  target stages 2-3, mip chains, stale surfaces. White ground: the combiner is diffuse x texture 0; blanking stages
  0-1 turns it black, so texture 0 reads white. Diagnostics added: RECOMP_DBG_NOZ, _SKIPNEARQUAD, _FLIPFRONT,
  _NOFOG, _TEXOFF=<mask>, _DXT; the frame trace now prints formats, clip range, cull, alpha test, combiners.
- One crash in DSOUND's voice notification (sub_00341EDC, a null voice slot, on the timer thread) in 15 runs.
- Mark chose the Xbox 360 ports' loop: he builds and tests on his PC. The Windows build now uses the Switch's
  renderer: nv2a_vk on WIN32 (SDL2's VC package, glslang 15.4.0 from source, the Vulkan SDK), a console program,
  the pushbuffer executor on (RECOMP_PB_EXEC); POSIX shims for nv2a_vk (nv2a_vk_win32.h). tools/win/: env.ps1 (from
  mof-nx), check_tools.ps1, build_pc.ps1 (deps, translation on the PC, configure, compile), boot_test.ps1, play.ps1.

## 2026-10-27: the heap's top block

- With a 6 MB bigger heap the story still failed the same way, and the game's use grew by the same 6 MB, so it was
  not the budget. RECOMP_HEAP_BIG=1 (new: logs allocations of 1 MB or more) shows the 8,431,088-byte block allocated
  and freed at boot. The runtime's first-fit heap coalesced freed neighbours but never gave a free block at the top
  back to the bump pointer, so a free tail plus the untouched space above it could not serve one large request.
  xbox_HeapFree now returns such a block (and trims empty slots at the end of the table).
- The attract demo still sits on NOW LOADING on Linux with every stage file opened, the loading music streaming and
  the CRI server thread resumed and suspended about 36 times a second; the main thread is always in the renderer,
  waiting on lavapipe (about 10 to 15 fps on this 2-core machine). Not solved yet.

## 2026-10-27: self-suspending threads, the heap, the main menu on Linux

- Loading never finished (the attract demo sat on NOW LOADING). gdb: a thread looping in sub_002ED700 calling
  sub_0027B5B9, XAPI's SuspendThread (NtSuspendThread, ordinal 231) on its own handle: CRI's server thread does one
  unit of work, then suspends itself until it is resumed the next frame. The POSIX SuspendThread only counted, so the
  thread spun (2.4 million suspends in a 3-minute run) and starved everything else. Now a self-suspend blocks on the
  thread's gate with the guest lock released until ResumeThread brings the count to 0 (`w32_wait_while_suspended`,
  xbox_NtSuspendThread; Linux and the Switch, Windows was already right).
- Run 5 with that: SEGA, Sonic Team, Sofdec/ADX and Dolby logos, the real title screen, Press START, the save prompt
  (Game Data created), Main Menu, 1P Play, Story, team select (all twelve models). Known: some letters are missing in
  the message boxes ("_ease _ress _ button").
- Picking Team Sonic: `xbox_HeapAlloc: out of memory (requested 8431088, used 46362624/50855936)` loading
  event0000.scr, then a null call and the staff roll, then a crash. Thread stacks already come from the heap, so the
  8 MB stack block (main stack plus two runtime worker slices) is now 2 MB and the heap 54.5 MB (a real Xbox leaves
  about 55 MB above this 7.5 MB image).

## 2026-10-27: Linux boots to the title screen, the Switch toolchain

- Linux run 1: about 8,600 lines, then a crash after seven failed indirect calls. `tools/seed_sweep.py` (new) does
  the skill's sweeps plus two more found here: the crash log's failed targets; uncovered code right after a ret with
  no padding; library-section gaps; 16-byte-aligned code right after int3 padding (the static pass merged across the
  padding: one "function" ran 0x001F4EE0 to 0x001F6A20); pointers in .rdata/.data (vtables) to code after padding.
  103 seeds, then 57 more after run 2 (160 in all), each with its evidence.
- Run 2: the Sonic Team logo, then the attract demo loading Seaside Hill (the movies were not copied to the cloud), then
  black, with five new failed indirect calls (one 266,000 times). Run 3, with all 160 seeds: no failed indirect calls;
  the Sonic Team logo, the title screen ("Press START"), the attract demo loading Seaside Hill.
- `timeout` alone does not stop the Linux build (it ignores SIGTERM): use `timeout -s KILL`.
- Switch toolchain on this cloud machine: Mark's C:\devkitPro (devkitA64, libnx, portlibs/switch, cmake, tools;
  537 MB) through the bridge, `setup_switch_wine.sh`; NVK from ghcr.io/danfromtico/switch-dev-legacy-vulkan (layer
  with libvulkan.a; libexpat.a and libzstd.a from another layer); glslang 15.4.0 built with the Switch toolchain into
  /opt/glslang-switch.

## 2026-10-27: first regen, the XDK 5659 hooks

- First regen (tools/regen.sh, 4 min on 2 cores): 22,527 functions, 0 failed, 2.57 million lines of C in 91 files;
  230 unresolved call targets stubbed, 691 untranslated instructions (data read as code), 41 spin loops.
- Kernel: `gen_ordinal_remap.py` on the analysis: all 116 imports are identity, no remap needed.
- Linux Vulkan build (out/build-vk-linux, lavapipe) compiles cleanly, 154 steps in about 10 minutes. The CMake
  project is now `sheroes_recomp`.
- CoD-nx's hooks found again at 5659 addresses (capstone on the D3D and DSOUND sections; evidence in
  src/recomp_manual.c): BlockOnTime 0x0032FC10 (device global 0x0033BC38), DSP command post 0x0034175A, AC'97
  channel reset 0x00346F4D, vblank 0x003324D0 and PGRAPH 0x00332A40 handlers (DPC 0x00332C80, ISR 0x003322D0).
  CoD's per-frame DSP heartbeat has no counterpart: 5659's DSOUND never compares with 0xCCCCCC, and its 10 ms DSP
  stall runs only at init.

## 2026-10-27: kickoff

- Prior art: no existing recompilation or Switch port of Sonic Heroes found (searches in this and two earlier
  threads). The Xbox version is the best base (higher-quality movies, 48 kHz audio).
- Disc: Mark's dump is a trimmed XISO (the XDVDFS volume descriptor at 0x10000, not 0x18300000), so it cannot be
  checked against Redump; the XBE's SHA-256 is recorded in CLAUDE.md instead. Extracted on Mark's PC into
  `assets/game_root` with a small XDVDFS walker.
- `default.xbe`: title SONICHEROES, 5345002B, region 1, entry 0x0027972B. Every library is 1.0.5659 (CoD-nx and
  Burnout 3 are 5849), so CoD-nx's library hooks do not carry over by address. xboxrecomp knows 5659 from Crimson
  Skies (`kernel.h`, `gen_ordinal_remap.py`).
- Repo started as a copy of CoD-nx at 82c236f (music volume): renamed the app to `sheroes`, entry point, empty seed
  list; CoD's hooks taken out of `src/recomp_manual.c` with a note on what each did, to be found again here. Mark's
  OK (2026-10-27) to run xboxrecomp in the cloud and the Switch toolchain (devkitPro under Wine, NVK, glslang 15.4.0).
