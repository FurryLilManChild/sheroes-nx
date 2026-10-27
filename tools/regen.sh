#!/usr/bin/env bash
# Regenerate the lifted C from the XBE: disasm -> func_id -> abi -> recomp.
#
#   tools/regen.sh
#
# Environment:
#   XBOXRECOMP_DIR  toolkit            (default: the vendored xboxrecomp/)
#   SH_XBE      default.xbe        (default: assets/game_root/default.xbe)
#   SH_GEN_DIR  output directory   (default: out/gen)
#   LIFT_ONLY=1     skip disasm/func_id/abi (enough after editing
#                   recomp_manual.c; seeds need the full run)
#   TRACE_FUNCS     JSON list of addresses whose entries are logged
#                   ([TRACE] lines; a bring-up probe, regenerate without it)
#
# Seeds (config/seed_functions.json) are entry points the static pass cannot
# see -- thread start routines and indirect-call targets found at runtime.
# Grow them with:  python3 -m tools.seed_from_log run.log default.xbe \
#                    --functions tools/disasm/output/functions.json \
#                    --seeds <this repo>/config/seed_functions.json
# and review every addition: a seed inside a function splits it.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TK="${XBOXRECOMP_DIR:-$HERE/xboxrecomp}"
XBE="${SH_XBE:-$HERE/assets/game_root/default.xbe}"
GEN="${SH_GEN_DIR:-$HERE/out/gen}"

cd "$TK"
if [ "${LIFT_ONLY:-0}" != "1" ]; then
    # disasm reads the section layout from <xbe stem>_analysis.json beside it.
    ANALYSIS="${XBE%.xbe}_analysis.json"
    [ -f "$ANALYSIS" ] || python3 -m tools.xbe_parser "$XBE" --json "$ANALYSIS" --quiet
    python3 -m tools.disasm "$XBE" --seed-functions "$HERE/config/seed_functions.json"
    python3 -m tools.func_id "$XBE"
    python3 -m tools.abi_analysis "$XBE"
fi
# sheroes-nx: the C is written to a fresh folder and only the files whose
# contents changed are copied over, so the build recompiles only those (a full
# rebuild of the 82 files takes about half an hour on two cores).
NEW="$GEN.new"
rm -rf "$NEW"; mkdir -p "$NEW" "$GEN"
python3 -m tools.recomp "$XBE" --game-name "Sonic Heroes" --all --split 250 \
    --gen-dir "$NEW" --exclude-manual "$HERE/src/recomp_manual.c" \
    --mmio-sections DSOUND,XPP,D3D,XGRPH \
    ${TRACE_FUNCS:+--trace-functions "$TRACE_FUNCS"}
changed=0
for f in "$NEW"/*; do
    b="$GEN/$(basename "$f")"
    if ! cmp -s "$f" "$b"; then cp "$f" "$b"; changed=$((changed + 1)); fi
done
for f in "$GEN"/*; do [ -e "$NEW/$(basename "$f")" ] || rm -f "$f"; done
rm -rf "$NEW"
echo "regen: $changed generated file(s) changed"
