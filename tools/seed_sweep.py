#!/usr/bin/env python3
"""Sweep an XBE for function entry points the static pass missed.

    python3 tools/seed_sweep.py <default.xbe> <functions.json> [--log run.log ...] [--seeds config/seed_functions.json]
                                [--write]

The xbox-switch-port skill's three sweeps, plus the failed indirect calls of any runs given with --log:

  log    "[ICALL] Failed to resolve VA 0x..." targets that land in a code section
  ret    16-byte-aligned addresses right after a ret (C3, or C2 imm16) with no padding, that are not a known start
  imm    16-byte-aligned immediates (push imm32, mov reg/mem, imm32) pointing into a code section, not a known start,
         that follow padding, a ret, or a jump, and start like a function
  gap    library sections: the byte after each known function's end, when it is not padding and no function starts
         there

Every candidate must decode as at least a few sane instructions. A new start inside a known function splits it,
which is the point (the static pass merged the callback into the function before it), so review the list before
writing. With --write the new seeds are appended to the seeds file with their evidence.
"""
import argparse, bisect, json, re, struct, sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

PROLOGUES = (b"\x55\x8b\xec", b"\x56", b"\x57", b"\x53", b"\x55", b"\x8b\x44\x24", b"\x8b\x4c\x24", b"\x8b\x54\x24",
             b"\x83\xec", b"\x81\xec", b"\x6a", b"\x68", b"\x8b\xc1", b"\x8b\xff", b"\x51", b"\xb8", b"\x33\xc0",
             b"\xa1", b"\x8b\x0d", b"\x8b\x15", b"\xd9", b"\xdd", b"\x0f\xb6", b"\x8b\x81", b"\x8b\x41", b"\x8b\x01", b"\xff\x25", b"\xe9")


def load_xbe(path):
    d = open(path, "rb").read()
    base = struct.unpack_from("<I", d, 0x104)[0]
    nsec, secaddr = struct.unpack_from("<II", d, 0x11C)
    secs = []
    for i in range(nsec):
        fl, va, vs, ra, rs, na = struct.unpack_from("<IIIIII", d, secaddr - base + i * 0x38)
        nm = d[na - base:na - base + 16].split(b"\0")[0].decode()
        secs.append(dict(name=nm, va=va, vsize=vs, raw=ra, rsize=rs, exec=bool(fl & 4)))
    return d, secs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("xbe")
    ap.add_argument("functions")
    ap.add_argument("--log", action="append", default=[])
    ap.add_argument("--seeds", default=None)
    ap.add_argument("--code", default=".text,D3D,DSOUND,XGRPH,XPP,D3DX,PSFD00,PSFD_I,PSFD_B,PSFD_P,DOLBY")
    ap.add_argument("--libs", default="D3D,DSOUND,XGRPH,XPP,D3DX,PSFD00,PSFD_I,PSFD_B,PSFD_P")
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()

    d, secs = load_xbe(a.xbe)
    code = [s for s in secs if s["name"] in a.code.split(",")]
    md = Cs(CS_ARCH_X86, CS_MODE_32)

    def sec_of(va):
        for s in code:
            if s["va"] <= va < s["va"] + s["rsize"]:
                return s
        return None

    def raw(va, n):
        s = sec_of(va)
        if not s:
            return b""
        o = s["raw"] + va - s["va"]
        return d[o:o + min(n, s["va"] + s["rsize"] - va)]

    funcs = json.load(open(a.functions))
    ranges = sorted((int(f["start"], 16), int(f["end"], 16)) for f in funcs)
    starts = set(s for s, _ in ranges)
    sstarts = [s for s, _ in ranges]
    seeds = []
    if a.seeds:
        seeds = json.load(open(a.seeds))
        starts |= {int(x["start"], 16) for x in seeds}

    def container(va):
        i = bisect.bisect_right(sstarts, va) - 1
        if i >= 0 and ranges[i][0] <= va < ranges[i][1]:
            return ranges[i][0]
        return None

    def sane(va, need=4):
        n = 0
        for ins in md.disasm(raw(va, 64), va):
            if ins.mnemonic in ("outsb", "outsd", "insb", "insd", "arpl", "aaa", "aas", "daa", "das", "into",
                                "hlt", "int3", "les", "lds", "bound", "salc", "sahf", "lahf", "in", "out",
                                "retf", "iretd", "int1", "cli", "sti", "wait", "rcl", "rcr", "scasb", "fcmovb",
                                "fcmove", "fcmovu", "fcmovnb", "fcmovne", "fcmovbe", "fcmovnbe", "fcmovnu", "jecxz", "loop"):
                return False
            if ins.mnemonic.startswith("ret") or ins.mnemonic == "jmp":
                return n >= 1 or (n == 0 and ins.mnemonic == "jmp" and "dword ptr [0x" in ins.op_str)
            n += 1
            if n >= need:
                return True
        return False

    def preceded_ok(va):
        p = raw(va - 4, 4) if sec_of(va - 4) else b""
        if len(p) < 4:
            return False
        if p[3] in (0xCC, 0x90, 0x00) or p[3] == 0xC3 or p[1] == 0xC2:
            return True
        # end of a jmp rel32/rel8 or a jump table (an address in the same section)
        if p[2] == 0xEB or raw(va - 5, 1) == b"\xe9":
            return True
        tgt = struct.unpack("<I", p)[0]
        return sec_of(tgt) is not None

    found = {}
    refs = set()
    for o in range(0, len(d) - 3):
        v = d[o] | d[o + 1] << 8 | d[o + 2] << 16 | d[o + 3] << 24
        if v & 15 == 0 and 0x11000 <= v < 0x00800000:
            refs.add(v)

    def add(va, kind, note):
        if va in starts or va in found:
            return
        if not sane(va):
            return
        c = container(va)
        where = f"inside sub_{c:08X}" if c is not None else "not inside any known function"
        found[va] = (kind, f"{note}; {where}; starts '{first_ins(va)}'")

    def first_ins(va):
        out = []
        for ins in md.disasm(raw(va, 16), va):
            out.append(f"{ins.mnemonic} {ins.op_str}".strip())
            if len(out) == 2:
                break
        return "; ".join(out)

    # log
    for lp in a.log:
        txt = open(lp, errors="replace").read()
        for m in sorted(set(re.findall(r"Failed to resolve VA 0x([0-9A-Fa-f]{8})", txt))):
            va = int(m, 16)
            if sec_of(va):
                add(va, "log", f"'[ICALL] Failed to resolve VA 0x{va:08X}' in {lp.split('/')[-1]}")

    # ret
    for s in code:
        blob = d[s["raw"]:s["raw"] + s["rsize"]]
        for off in range(16, len(blob), 16):
            va = s["va"] + off
            if blob[off - 1] == 0xC3 or (off >= 3 and blob[off - 3] == 0xC2 and blob[off - 1] == 0):
                if blob[off] in (0xCC, 0x90, 0x00):
                    continue
                if blob[off:off + 3] == b"\x8d\x49\x00" or blob[off:off + 4] == b"\x8d\xa4\x24\x00":
                    continue
                inside = container(va) is not None
                if inside and va not in refs:
                    continue
                add(va, "ret", f"{s['name']}: follows a ret with no padding, 16-byte aligned"
                    + (", its address appears in the image" if va in refs else ""))

    # imm
    for s in code:
        for ins in md.disasm(d[s["raw"]:s["raw"] + s["rsize"]], s["va"]):
            if ins.mnemonic not in ("push", "mov"):
                continue
            m = re.search(r"0x([0-9a-f]{6,8})\b", ins.op_str)
            if not m or "[" in ins.op_str.split(",")[-1]:
                continue
            va = int(m.group(1), 16)
            if va & 15 or not sec_of(va) or va in starts:
                continue
            if not preceded_ok(va) or not raw(va, 4).startswith(PROLOGUES):
                continue
            add(va, "imm", f"immediate at 0x{ins.address:08X} ({ins.mnemonic} {ins.op_str})")

    # pad: 16-byte-aligned code right after int3 padding that no function starts at
    for s in code:
        blob = d[s["raw"]:s["raw"] + s["rsize"]]
        for off in range(16, len(blob), 16):
            if blob[off - 1] != 0xCC or blob[off] == 0xCC:
                continue
            va = s["va"] + off
            if va in starts or not blob[off:off + 4].startswith(PROLOGUES):
                continue
            add(va, "pad", f"{s['name']}: follows int3 padding, 16-byte aligned")

    # data: pointers in the data sections (vtables, callback tables) to code after padding or a ret
    for s in secs:
        if s["name"] not in (".rdata", ".data"):
            continue
        blob = d[s["raw"]:s["raw"] + s["rsize"]]
        for off in range(0, len(blob) - 3, 4):
            va = struct.unpack_from("<I", blob, off)[0]
            if va & 15 or va in starts or va in found or not sec_of(va):
                continue
            if not raw(va, 4).startswith(PROLOGUES):
                continue
            pb = raw(va - 1, 1)
            p3 = raw(va - 3, 3)
            if not (pb in (b"\xcc", b"\x90", b"\xc3") or (len(p3) == 3 and p3[0] == 0xC2)):
                continue
            add(va, "data", f"pointer at 0x{s['va'] + off:08X} ({s['name']}), follows padding or a ret")

    # gap
    for s in secs:
        if s["name"] not in a.libs.split(","):
            continue
        for st, en in ranges:
            if not (s["va"] <= st < s["va"] + s["rsize"]) or en in starts:
                continue
            b = raw(en, 1)
            if not b or b[0] in (0xCC, 0x90, 0x00):
                continue
            if raw(en, 3) == b"\x8d\x49\x00" or raw(en, 3) in (b"\x8d\xa4\x24", b"\x8d\x64\x24"):
                continue
            if container(en) is not None:
                continue
            add(en, "gap", f"{s['name']}: right after the end of sub_{st:08X}, no function starts there")

    by = {}
    for va, (k, n) in sorted(found.items()):
        by.setdefault(k, []).append(va)
        print(f"0x{va:08X} [{k}] {n}")
    print("totals:", {k: len(v) for k, v in by.items()}, file=sys.stderr)

    if a.write and a.seeds:
        for va, (k, n) in sorted(found.items()):
            seeds.append({"start": f"0x{va:08X}", "note": f"seed_sweep.py [{k}]: {n}"})
        json.dump(seeds, open(a.seeds, "w"), indent=1)
        print(f"wrote {len(found)} seeds to {a.seeds}", file=sys.stderr)


if __name__ == "__main__":
    main()
