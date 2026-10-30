# Third-party components

sheroes-nx's own code and most of the code it builds on are under the MIT License ([LICENSE](LICENSE)). These parts
keep their own licenses:

| Component | Where | License |
| --- | --- | --- |
| xemu's MCPX APU emulation and NV2A register definitions | `xboxrecomp/` (see [xboxrecomp/NOTICE](xboxrecomp/NOTICE)) | LGPL 2.1 or later |
| Mesa (NVK Vulkan driver), from the danfromtico/switch-dev-legacy-vulkan image | linked into the Switch build | MIT |
| glslang 15.4.0 | linked into the Switch build | several permissive licenses (see its LICENSE.txt) |
| devkitPro libnx, switch-sdl2, zlib | linked into the Switch build | ISC-style, zlib, zlib |
| font8x8 by dhepper | `src/sheroes_font8x8.h` | public domain |

xemu's code is LGPL: you may replace it with your own build. Everything needed to rebuild the Switch program is in
this repository (`tools/regen.sh`, `switch/build.sh`), given your own copy of the game. The full LGPL text is in
`xboxrecomp/LICENSES/LGPL-2.1.txt`.
