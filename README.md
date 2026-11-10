# sheroes-nx (branch xbe-recomp, work in progress)

**Sonic Heroes** (original Xbox, USA, 2004) on the Nintendo Switch and the PC, statically recompiled from the
original Xbox executable. Not an emulator.

This branch rebuilds the port on TGP482's [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) by way of
[brextreme-nx](https://github.com/xsaja/brextreme-nx) (Bloody Roar Extreme), the toolchain behind the Xbox ports that
run at 60 fps at the Switch's stock clocks: Direct3D 8 and DirectSound are replaced at the API level, and a render
thread draws with deko3d. Why: [docs/strategy-review.md](docs/strategy-review.md) and
[docs/feasibility-xbe-recomp.md](docs/feasibility-xbe-recomp.md). Nothing to play on this branch yet; the 0.2.0
pre-release (xboxrecomp) is on `main`.

You need your own copy of the game. No game files are in this repository.

License: GPL-3 ([LICENSE](LICENSE)); the components and their licenses are listed in [LICENSES.txt](LICENSES.txt).
Credits: TGP482 (XboxXBE-Recomp), xsaja (brextreme-nx, street-nx, mvp-nx), maddog847 (MVP05 XBE Recomp),
gerisson1996 (BLACK-NX-PORT), and the projects listed in LICENSES.txt. No affiliation with SEGA, Sonic Team,
Microsoft or Nintendo.
