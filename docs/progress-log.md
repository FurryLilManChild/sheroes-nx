# Progress log (newest first)

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
