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
