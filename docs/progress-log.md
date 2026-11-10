# Progress log (newest first)

The xboxrecomp-era log (0.1.0, 0.2.0) is in `docs/xboxrecomp-era/progress-log.md`. Upstream's is in
`docs/brextreme/progress-log.md`.

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
