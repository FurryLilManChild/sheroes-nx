# Progress log (newest first)

The xboxrecomp-era log (0.1.0, 0.2.0) is in `docs/xboxrecomp-era/progress-log.md`. Upstream's is in
`docs/brextreme/progress-log.md`.

## 2026-11-10: first PC boot

- Mark's first PC run (SHEROES-vk.exe): the game found the ISO, created the device 640x480, Vulkan and audio
  started, then an access violation at 2.7 s: guest write to 0x00000038 inside 0x003408EE, called from 0x0026B12A
  with a 0x40-byte DS3DLISTENER. 0x003408EE is IDirectSound_SetAllParameters (the XDK order: after
  CreateSoundBuffer, before SetDistanceFactor), which the SDK did not know, so the console code ran on the host's
  DirectSound object.
- A scan of every direct call from game code into DSOUND found two more unnamed: 0x0033F2D8
  IDirectSoundBuffer_SetPitch (after SetVolume) and 0x0033FAA6 IDirectSoundBuffer_SetConeAngles (between SetMixBins
  and SetMaxDistance). All three named in game.toml and implemented in dsound.cpp (the listener's position and
  orientation; pitch as a frequency, 1/4096 octave from 48 kHz; cone angles accepted). The 19 unnamed D3D functions
  the game calls directly are listed in docs/feasibility-xbe-recomp.md; they only read or set up resources.

## 2026-11-09: branch xbe-recomp started

- Mark: after the review of the other Xbox ports and the feasibility check (`docs/strategy-review.md`,
  `docs/feasibility-xbe-recomp.md`), proceed with moving Sonic Heroes onto TGP482's XboxXBE-Recomp; license left to
  Claude: GPL-3, building on xsaja/brextreme-nx (commit 82d8054, 2026-11-03).
- Branch `xbe-recomp`: brextreme-nx's tree (without its browser installer), names changed to sheroes (`sheroes.nro`,
  `sheroes-settings.toml`, `SHEROES_*`), the Switch folder `sdmc:/switch/sheroes/` as in 0.x, title ID 5345002B,
  English error messages (they were Spanish). brextreme-nx's 5233 game.toml kept as `docs/brextreme/game-5233.toml`.
- XDK 5659: `sdk/xdk/5659/layout.toml` (every offset checked at a 5659 address; the same as 5849 except
  SetGammaRamp's pending index, left unknown), signatures copied from 5849 for now. `game/config/game.toml` from
  xbe_symbols (130 of 238 functions, 9 of 12 data) plus four named by hand: BeginPush 0x32FA00, EndPush 0x32FA20,
  SetPalette 0x32C110, LoadVertexShaderProgram 0x32D1D0.
- Gap found: the SDK logged LoadVertexShader and SelectVertexShader as unsupported, and RenderWare's Xbox driver
  runs its vertex programs exactly that way (0x002BFB40: LoadVertexShaderProgram(program, 12), then
  SelectVertexShader(handle, 12) when the program changes). Now implemented: a 136-slot program memory, the
  selected shader made from the handle's declaration and the slot's program up to the final flag.
- Not linked in this XBE (no references): SetIndices, the overlay (CRI Sofdec does not use it), the fences,
  SetTile, PersistDisplay, DirectSound streams, Bink, the network libraries. Left unnamed, running their own code
  without touching hardware: GetGammaRamp 0x32AF30, GetDisplayFieldStatus 0x32C1D0, the resource creation, lock and
  surface level wrappers (0x335360, 0x3353F0, 0x338930-0x338AC0), the mode enumeration (0x338AD0, 0x338B70).
- `tools/win/build_pc.ps1` (with main's `env.ps1`): the PC build with LLVM clang and the Visual Studio Build Tools.
- Fixed-function vertex blending added to the SDK's shader (the characters' skinning).
- `sdk/xdk/5659/signatures.toml` signed from this game.toml (134 of 239 functions; found again from scratch).
- Cloud compile check of the Switch NRO (devkitA64 GCC, deko3d and NVK from /opt/nvk-sdk): builds, 353 steps,
  26 minutes on 2 cores; the NRO is 36.5 MB (0.2.0's: 62 MB). Not sent: the PC boot comes first.
