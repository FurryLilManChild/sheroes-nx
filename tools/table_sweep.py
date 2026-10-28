#!/usr/bin/env python3
"""Sweep .rdata/.data for pointers into code that are not known function starts but sit beside known starts
(vtables, object-type descriptors, state tables). Run from the repo root after tools/regen.sh; review each hit
(starts that land in int3 padding are numbers, not pointers). Found 0x0021A780, 0x0023D460, 0x00272110, 0x00089250,
0x0011D020 on 2026-10-28."""
import json,sys,struct
sys.path.insert(0,'tools')
from seed_sweep import load_xbe
from capstone import *
d,secs=load_xbe('assets/game_root/default.xbe')
md=Cs(CS_ARCH_X86,CS_MODE_32)
S={s['name']:s for s in secs}
codes=[S[n] for n in ('.text','D3D','DSOUND','PSFD00','PSFD_I','PSFD_B','PSFD_P','DOLPHIN','D3DX','XGRPH','XPP')]
def csec(v):
    for s in codes:
        if s['va']<=v<s['va']+s['rsize']: return s
fns=json.load(open('xboxrecomp/tools/disasm/output/functions.json'))
fl=fns if isinstance(fns,list) else fns.get('functions',fns)
r=[]
for f in (fl.values() if isinstance(fl,dict) else fl):
    a=int(f['start'],16) if isinstance(f['start'],str) else f['start']; e=f.get('end'); e=int(e,16) if isinstance(e,str) else e
    r.append((a,e or a+1))
starts={a for a,_ in r}
import bisect
r.sort(); keys=[a for a,_ in r]
def inside(v):
    i=bisect.bisect_right(keys,v)-1
    return i>=0 and r[i][0]<v<r[i][1]
def rd(va,s): return struct.unpack_from('<I',d,va-s['va']+s['raw'])[0]
PRO=(b"\x55\x8b\xec",b"\x56",b"\x57",b"\x53",b"\x55",b"\x8b\x44\x24",b"\x8b\x4c\x24",b"\x8b\x54\x24",b"\x83\xec",b"\x81\xec",b"\x6a",b"\x8b\xc1",b"\x51",b"\xb8",b"\x33\xc0",b"\xa1",b"\x8b\x0d",b"\x8b\x15",b"\xd9",b"\x8b\x81",b"\x8b\x41",b"\x8b\x01",b"\xe9",b"\xc2",b"\xc3",b"\x8b\x49",b"\x8b\x89",b"\x0f\xb6",b"\x8a",b"\x66",b"\x83\x79",b"\x80\x79",b"\xf6",b"\x8b\x46",b"\x8b\xff",b"\x32\xc0",b"\xb0")
found={}
for sn in ('.rdata','.data'):
    s=S[sn]
    vals=[(va,rd(va,s)) for va in range(s['va'],s['va']+s['rsize']-3,4)]
    for k,(va,v) in enumerate(vals):
        cs=csec(v)
        if not cs or v in starts: continue
        # neighbours: a known start within 2 slots either side
        nb=sum(1 for j in (k-2,k-1,k+1,k+2) if 0<=j<len(vals) and vals[j][1] in starts)
        if nb==0: continue
        o=v-cs['va']+cs['raw']; prev=d[o-1]
        ins=list(md.disasm(d[o:o+32],v))
        if len(ins)<3: continue
        ok_pro=d[o:o+3].startswith(PRO)
        bound=prev in (0xC3,0xCC,0x90) or (o>=3 and d[o-3]==0xC2)
        if inside(v) and not bound: continue   # a jump-table case label
        if not (ok_pro or bound): continue
        found.setdefault(v,[]).append(va)
for v in sorted(found):
    cs=csec(v); o=v-cs['va']+cs['raw']
    ins=list(md.disasm(d[o:o+32],v))[:3]
    print(hex(v),cs['name'],'refs',[hex(x) for x in found[v][:3]],'inside' if inside(v) else 'free','prev %02X'%d[o-1],'|','; '.join(i.mnemonic+' '+i.op_str for i in ins))
print(len(found))
