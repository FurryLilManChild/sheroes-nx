# Progress log (newest first)

## 2026-10-29: fixed-function texgen (the character shine)

- The fixed-function vertex shader now generates texture coordinates as the NV2A does (xemu's vsh-ff.c): per stage
  SET_TEXGEN_S/T/R/Q (eye linear, object linear, sphere map, normal map, reflection map) with normals taken to eye
  space by SET_INVERSE_MODEL_VIEW_MATRIX0, then SET_TEXTURE_MATRIX when SET_TEXTURE_MATRIX_ENABLE is on. Register
  addresses from nv2a_regs.h. Vulkan only (the GL back end leaves u_tg at 0 and keeps the old path).
- The frame trace prints each enabled stage's texgen modes and texture matrix, so the next trace confirms which
  modes the character passes use.
- Not yet seen running: Mark's play test checks the characters.

## 2026-10-28: textured stage (play test 10)

- The fixed-function fog fix works: Mark's screenshot shows the stage textured (grass, path, stone, water) at a good
  frame rate. Fog registers in the trace: linear, params 6 and -1/6000, plane (0,0,1,0), colour white, so fog only
  starts far away.
- Next, the characters: their second passes are additive (blend SRC_ALPHA, ONE, depth write off), colour c0 x
  texture 0 (an environment highlight, 80C87F00), and the eye and head draws add texture 1 x c0 the same way. Those
  need generated texture coordinates (SET_TEXGEN_S/T/R/Q from 0x03C0, sphere map or reflection map, and the texture
  matrices). The fixed-function vertex shader has no texgen and passes the model's own coordinates (vT0 = v9), so
  the highlight lands as a plain texture: the white patches on Sonic and Knuckles and Tails' odd head. Plan: print
  the texgen and texture-matrix registers in the trace, then add texgen to the fixed-function shader.
- Flickering lines in the level: not yet looked at (needs a description or a capture of when they show).

## 2026-10-28: why the stage geometry is white (fixed-function fog)

- The texture dump from Mark's play test shows the stage textures intact in guest memory (grass, cliffs, sea), so
  the uploads were never the problem.
- The frame trace shows the cause: the six sky, sea and far-cliff draws, which draw correctly, use the final combiner
  0000000C (R0 as is). Every draw after them uses 130C0300 (fog.a x R0 + (1 - fog.a) x fog colour), and about 150 of
  them are fixed-function (xform 1). The fixed-function vertex shader set vFog to 0, so fog.a was 0 and those draws
  came out as the fog colour: white. The earlier RECOMP_DBG_NOFOG test only touched vertex-program draws, which is
  why fog looked ruled out.
- Fix (gl_vsh.c, gl_psh.c, nv2a_vk.c): the fixed-function shader computes the fog distance as xemu's foggen does
  (spec alpha, radial, planar, abs planar, fog X) from SET_MODEL_VIEW_MATRIX0, SET_FOG_PLANE and SET_FOG_GEN_MODE,
  and the pixel shader applies the fog mode and SET_FOG_ENABLE as for vertex programs. RECOMP_VK_FFP_FOG=0 turns
  it off. The frame trace now prints the fog registers.

## 2026-10-28: diagnostic build for the white level geometry

- The frame trace now prints every combiner stage the stage draws enable (the old line showed two of three), each
  stage's two constant colours, and the final combiner's constants.
- With `RECOMP_TEXDUMP_DIR` set (run_game.ps1 sets it to `out\texdump`), a traced frame also saves the level-0 bytes of
  each draw's textures, once per address, so the stage textures can be decoded and checked offline.
- Nothing visible changes; this build only gathers the data to find why the stage draws white.

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
- Build 3 linked (174 steps). First PC boot: kernel, APU (XAudio2), save folders, then an access violation reading
  0xFED00000 (OHCI) from XPP 0x0037F812. ohci.c protected its registers on Windows for a VEH that this project does
  not have; XPP is lifted with --mmio-sections, so the model is now registered with the MMIO accessors on Windows
  too, as on Linux (the VEH path stays behind XBOX_OHCI_VEH).
- Boot 2 ran 60 s but stalled before the first frame: NOP(9) never taken. A stall report (IRQL holders, the guest
  lock holder, their host stacks resolved with a linker map, out/pc/sheroes_recomp.map) showed the deadlock: the
  main thread held the guest lock spinning in D3D's fence wait (sub_0032FC10_gen), the timer thread waited for the
  lock at DISPATCH to run the PGRAPH ISR. recomp_types.h's poll-loop hint took the `_MSC_VER` branch under clang (a
  bare pause, never yielding the lock); now cl only. build_pc.ps1 re-lifts when that template changes.
- Boot 3: the title screen on Mark's PC (AMD Radeon, Vulkan 1.4.344, swapchain 1280x720), correct logo, sky, Press
  START and the fade on Enter; Mark reached the next menus. About 52 frames/s by the screenshot times, and slow by
  eye. Nothing called timeBeginPeriod, so every 1 ms wait (vblank clock, guest lock, executor) took a 15.6 ms tick:
  timeBeginPeriod(1) at startup. run_game.ps1 turns on RECOMP_FPS_LOG.
- Play 1 (Mark): 58.7 to 59.5 fps on the menus, sound clean. Title, save creation (text missing letters), 1 player,
  story, Team Sonic (models on team select look wrong; screenshot to come), then a crash as the story's first
  Sofdec movie started: ICALL to 0x0035C1C0, the PSFD_I section, which had no functions, nor had PSFD_P. Each is
  one integer-only function, the I and P picture handlers in the CRI decoder's table at 0x003DB1D4. Seeded both.
- Play 2: the movie got further, then ICALL to 0x0031E010, a CRI callback stored with MEM32(reg+0x12EC) = 0x31E010,
  in a 5 KB gap no function covered (one 13.9 KB function with two jump tables). Seeded. Then two sweeps so a stage
  load does not find the next one: constant code addresses written to memory (only 0x0031E010) and
  tools/table_sweep.py, pointers in .rdata/.data tables beside known functions: 0x0021A780 and 0x0023D460 (object
  factories in type descriptors, allocating 0x320 and 0x35C bytes), 0x00272110, 0x00089250, 0x0011D020. Three
  sat right after jump-table index bytes the disassembler's sweep decoded as code, so it rejected them as
  mid-instruction; seeds can now carry "verified": true (checked by hand, stated in the note), trusted like
  observed ones (tools/disasm/__main__.py).
- Play 3: the movie plays, the first stage loads and draws on the PC (no black quad: that was lavapipe), geometry
  white, and in the stage only the triggers (camera) respond. ~20,000 calls to 0: HUD number drawing
  (sub_0003AB20) calls 0x002D4B80 = jmp [obj+0x12C] with obj = [0x00558A8C] = 0, the result of the sprite-set
  lookup by name "a_num_s" (0x002D4B70). In the stage the game read font\a_num_s.met (METRICS2, pages a_num_sa.png
  and a_num_sb.png) and a_num_sa.png and never opened a_num_sb.png: PNG decode suspected (libpng 1.0.12, zlib
  1.1.3), which also fits the menu text missing letters. png_error 0x00293410 and png_warning 0x00293430 now log
  their message. The stage's DXT1 uploads hold real data ([DXT] log). tools/win/trace_frame.ps1 traces one frame.
- Play 4: every PNG failed with "Decompression error"; zlib's inflate (0x0029B4E0) returned 32511, not a zlib code.
  Reproduced on Linux in 90 s (the fonts at boot). Probes: inflate_blocks came back with esp 36 bytes off and the
  bad value; inflate_codes never returned. Cause: a seed of mine, 0x002A16B0 (the [pad] sweep, 2026-10-27, which
  took operand bytes CC for int3 padding; its note even shows the garbage decode), split inflate_codes
  (0x002A1320-0x002A1A30, switch table at 0x002A1A04) inside an instruction at 0x002A16AB. Removed; all PNGs
  decode. A scan tracing every function's control flow for starts inside another real function's instructions
  found nothing else (only harmless alias starts in sub_001D7CC0). Kept: [ZLIB] logs any failing inflate.
- Play 5 (Mark): zero PNG errors, zero null calls, the HUD (score, 00:00:00, rings) and full text drawn; still no
  control in the stage. The screenshot shows NOW LOADING for good and the timer at 0. The CRI stream keeps reading;
  the Linux attract demo loads the same files and then s01.dmo and runs. The log shows RtlEnterCriticalSection /
  RtlLeaveCriticalSection 4.3 million times (the Switch run: 38 million): a poll under a lock. Diagnostics:
  tools/win/dump_threads.ps1 (flag file) prints the guest-lock holder, IRQL holders, the hottest
  RtlEnterCriticalSection callers (counted in the kernel dispatcher) and every thread's host stack. The window's
  close button now ends the program (SDL_QUIT was ignored).
- Play 6 (thread dump): no deadlock; the main thread renders, CRI's threads idle (the self-suspending one resumed
  ~36/s). The hot lock is DirectSound's global lock (sub_0033E675). The attract demo stops at the same NOW LOADING
  (title card), so it is the stage start, not input. The stage scene (class 0x003897A0: init 0x0001A740, update
  0x0001D1D0, a 17-state machine on [this+8]) goes 2 -> 4 and stays in 4 (PC run, [SCENE] log). State 4
  (0x0001D543) waits for the screen fader at [0x0055867C] (vtable 0x0038A5D8) to finish: mode +0x28 back to 0.
  The fader update (vtable+4, 0x00038320) steps colour +0x30 toward target +0x2C by 5 and always converges, so
  either it is not called or the fade keeps restarting. [FADE] diagnostics log the update and every fade start.
- Play 7: the base fader's update never ran, but the stage's fader is the subclass (vtable 0x0038A650) whose update
  0x00038BB0 eases a float toward a target and ends the fade on `fucomip; lahf; test ah, 0x44; jp`. The lifter
  emitted LAHF after the x87 FCOMI family as a comment (its generic path has no SF expression for them), so AH was
  stale and the fade never ended: every stage waited in state 4 at NOW LOADING. lifter.py: LAHF from g_fp_cmp for
  fcomi/fcomip/fucomi/fucomip/fcompi/fucompi. 34 generated files changed. build_pc.ps1 re-lifts when the
  lifter's .py files change.
- Play 8 (Mark): the stage title card shows in full for the first time, the stage starts (scene 4 -> 5), the
  characters respond to every button, a death restarts the stage (5 -> 8 -> 4 -> 5), the timer runs. No crash, no
  failed calls, ~53 fps with -Shots. Next for playability: the stage geometry draws white.

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
