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
- Second PC run: NOW LOADING on screen, then a guest trap at 1.76 s: "indirect call to 00323E20 has no recompiled
  function" (return 0x002D4B45). This recompiler had begun that function at its padding (0x00323E17). Instead of
  adding misses one at a time: the xboxrecomp port's disassembler (main, 22,690 functions) was run again, and every
  function start it found that this recompiler did not became a `[functions]` seed when the image holds that
  address as an aligned pointer in .rdata/.data (1,848) or loads it as an immediate (731), plus main's hand seeds
  (20); one that begins inside an instruction here dropped. 23,845 functions now (21,366 before); overlapping
  decodes 263 (79 before). All 64 files compile in the cloud.
- Third PC run: the title screen at 60 fps; after Start a black screen with the music playing. A D3D12 frame capture
  (F3, XBE_CAPTURE_DIR) shows the frame drawn right in the game's target (the "There is no Game Data" prompt, 169
  draws), so the picture was lost on the way to the window. Likely cause (to be confirmed by the next log): the
  game fades by reading the current gamma ramp with D3DDevice_GetGammaRamp (0x0032AF30, the console's code: slot at
  [CDevice + 0x240C], ramp at CDevice + 0x1E04 + slot * 0x300) and the SDK's SetGammaRamp never wrote that memory,
  so the fade scaled a ramp of zeros. SetGammaRamp now mirrors its ramp into both slots, CreateDevice starts them
  at identity, and the first 24 SetGammaRamp calls are logged.
- Fourth PC run: through the menus, team select, the movie and the stage's title card, then an access violation
  while loading the stage: a read at 0xB01CD000, the first byte after a 0x2F0-byte block at 0xB01CCD10 in system
  (pool) memory, 0x3B dwords to copy (ecx). On the console pool memory is plain RAM and such a read sees the next
  block; here the page after the last block was not mapped. AllocSystem now commits one page past each block.
  (The gamma fix worked: the menus showed.)
- Fifth PC run (2026-11-13): the same crash one page further on (read 0xB01CE000, ecx 0x3B again), so the page
  slack was the wrong conclusion and is taken out again. The real cause: RenderWare learns the XDK's pixel shader
  object layout at stage load (0x2908C0, called from 0x2E13FF): it creates a pixel shader from a 0xF0-byte
  definition at 0x3D0498, then scans the object for a copy of that definition (repe cmpsd, 0x3C dwords) and for a
  pointer to it. The console's CreatePixelShader (0x3377E0) allocates 0xFC bytes: refcount 1 at +0, 1 at +4, a
  pointer at +8 to the definition copied to +0xC. The SDK's record was 16 empty bytes, so the scan ran off the end
  of pool memory. CreatePixelShader now lays its record out the same way.
- Sixth PC run (2026-11-13): into the first stage, played the first part. Log: 58 to 60 fps in the stage (one 53.6
  window during loading), about 380 draws a frame, the title thread 0.7 ms to record and the render thread 3 ms a
  frame on the Legion Go. A few single long frames (60 to 130 ms, one 634 ms) while streaming; clean exit.
- First Switch run (2026-11-13, deko3d, stock clocks, full memory): crash at 10.6 s, just after NOW LOADING:
  "memory: backing 01700000+400000 failed (0xCE01)", then a guest write to the unbacked 0x01700000. 0xCE01 is kernel
  result 103 (out of resource). Guest memory on the Switch is heap blocks shown at their guest address as code
  memory, one code memory object per range backed on demand, and Horizon keeps very few of those for the whole
  system (10 in Atmosphere's slab counts). The busy ranges are now backed up front, one object each: 0x00000000+128
  MB (image and heaps), the pool at 0xB0000000+64 MB, the stacks at 0xD0000000+32 MB. Backing on demand now logs a
  warning, to see if anything still needs it.

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
