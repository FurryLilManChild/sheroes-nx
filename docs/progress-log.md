# Progress log (newest first)

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
