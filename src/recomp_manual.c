/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* The generated register model (g_eax/g_esp are thread-local there) and the
 * XBOX_PTR/MEM32 accessors; the register names below are its macros. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

/* ── Library hooks (XDK 5659) ─────────────────────────────────
 *
 * sheroes-nx. Sonic Heroes links XDK 1.0.5659 (CoD-nx and Burnout 3 link
 * 5849). Each hook below is CoD-nx's, moved to the same library function at
 * this game's address. Found by reading the XBE's D3D and DSOUND sections
 * with capstone on 2026-10-27 (kickoff); evidence with each.
 *
 * Not ported: CoD's per-frame DSP heartbeat (sub_003E59DE). 5659's DSOUND
 * has no "cmp dword [eax], 0xCCCCCC" anywhere; its only 10 ms stall
 * (push 0x2710; call KeStallExecutionProcessor at 0x00346754, in
 * sub_00346738) is reached once, from DirectSound init sub_00346BEE. */

/* ── D3D fence wait, BlockOnTime (0x0032FC10) ─────────────────
 *
 * Stdcall (fence at [esp+4], flags at [esp+8], ret 8), as CoD's 5849 build.
 * Its body opens with the device from the global 0x0033BC38, then
 * [dev+0x30] (pointer to the GPU's completion semaphore) and [dev+0x2C]
 * (write sequence): the layout CoD's hook relies on. Called from
 * 0x0032FE6F ("push 8; push fence"), 0x0032FF20, 0x0032FF40, 0x0032FFC0,
 * 0x00338780. With the pushbuffer executor (RECOMP_PB_EXEC) the real
 * semaphore is handed to it once and the original wait runs; without it the
 * GPU is reported caught up. */
#define SH_D3D_DEVICE_PTR 0x0033BC38u
extern void nv2a_pb_set_semaphore_target(uint32_t guest_va);
extern void sub_0032FC10_gen(void);

void sub_0032FC10(void)
{
    static int exec = -1;
    uint32_t dev = MEM32(SH_D3D_DEVICE_PTR);
    uint32_t sem = dev ? MEM32(dev + 0x30) : 0;

    if (exec < 0)
        exec = getenv("RECOMP_PB_EXEC") != NULL;
    if (exec) {
        static uint32_t registered;
        if (sem && sem != registered) {
            nv2a_pb_set_semaphore_target(sem);
            fprintf(stderr, "[D3D] GPU semaphore at 0x%08X\n", sem);
            registered = sem;
        }
        sub_0032FC10_gen();
        return;
    }
    if (sem)
        MEM32(sem) = MEM32(dev + 0x2C);     /* "GPU" is always caught up */
    eax = 0;
    esp += 4 + 8;                           /* return address, fence, flags */
}

/* ── DirectSound DSP command post (0x0034175A) ───────────────
 *
 * CoD's sub_003E501C (Burnout 3's 0x002F7EC2), the same body: thiscall,
 * ebx = ***(this + 8 + 0x10) is the GP DSP's scratch area, the command code
 * goes to scratch + 0x810 and it spins until the DSP zeroes it
 * (0x003418B7: add ebx,0x810; 0x003418BF: mov [ebx],eax; 0x003418C1:
 * cmp [ebx],0; jne 0x003418C1). There is no DSP, so the word is registered
 * with the runtime's completion list first. Found by searching DSOUND for
 * the disp32 0x810 (one hit, 0x003418B9). One caller, 0x003419C7. */
extern int xbox_ApuDspAckWord(uint32_t va);
extern void sub_0034175A_gen(void);

void sub_0034175A(void)
{
    uint32_t obj = MEM32(ecx + 8);
    uint32_t blk = obj ? MEM32(obj + 0x10) : 0;
    uint32_t scratch = blk ? MEM32(blk) : 0;

    if (scratch)
        xbox_ApuDspAckWord(scratch + 0x810);
    sub_0034175A_gen();
}

/* ── DirectSound AC'97 channel reset (0x00346F4D) ────────────
 *
 * CoD's sub_003EB725 (Burnout 3's 0x002FE736). Found by searching DSOUND
 * for the NABM control byte 0xFEC0010B (as disp32 -0x13FFEF5): it sets RR
 * (bit 1) in the channel's control byte (0xFEC0010B + table[0x0035A1A0 +
 * 4 * channel], 0x00346F6A) and MSVC hoisted the re-read out of the wait
 * (0x00346F7A read once; 0x00346F83-0x00346F85 test cl,cl; jne back to the
 * test), so with no controller it spins forever. This is the same function
 * with the reset completing at once. Lock: the stack object's constructor
 * 0x0033E470 (ecx = frame - 8); then 0x00346D57 (four pushes) and the
 * destructor 0x0033E492. Callers: 0x00347035 (sub_0034700D). */
void sub_00346F4D(void)
{
    uint32_t ebp_saved = g_ebp, frame, chan, nabm;

    PUSH32(esp, ebp_saved);
    frame = esp;
    g_ebp = frame;
    g_seh_ebp = frame;
    esp -= 8;
    MEM32(frame - 4) = 0;
    PUSH32(esp, esi);
    esi = ecx;
    chan = esi;

    ecx = frame - 8;
    PUSH32(esp, 0x00346F61u); RECOMP_ABI_CALL(0x0033E470u, sub_0033E470);

    nabm = MEM32(MEM32(chan) * 4 + 0x0035A1A0u);
    MEM8(nabm + 0xFEC0010Bu) = 2;
    MEM8(nabm + 0xFEC0010Bu) &= (uint8_t)~2u;   /* reset complete */
    eax = nabm;
    SET_LO8(ecx, 0);

    MEM32(nabm + 0xFEC00100u) = MEM32(chan + 0x1C);
    if (MEM32(chan) == 1)
        MEM32(0xFEC0017Cu) = MEM32(chan + 0x28);

    PUSH32(esp, 1);
    PUSH32(esp, 1);
    ecx = chan;
    PUSH32(esp, MEM8(chan + 0x24));
    PUSH32(esp, MEM8(chan + 0x25));
    eax = MEM8(chan + 0x25);
    g_ebp = frame;
    g_seh_ebp = frame;
    PUSH32(esp, 0x00346FB4u); RECOMP_ABI_CALL(0x00346D57u, sub_00346D57);

    ecx = frame - 8;
    g_ebp = frame;
    g_seh_ebp = frame;
    PUSH32(esp, 0x00346FBCu); RECOMP_ABI_CALL(0x0033E492u, sub_0033E492);

    POP32(esp, esi);
    esp = frame;
    POP32(esp, ebp_saved);
    g_ebp = ebp_saved;
    esp += 4;
}

/* ── D3D vblank and PGRAPH handlers (0x003324D0, 0x00332A40) ──
 *
 * CoD's hooks for its 0x003C1C40 / 0x003C21B0 (Burnout 3's 0x00356DA0 /
 * 0x00357000). D3D's init sub_00333334 calls KeInitializeDpc (0x0033334B)
 * with the DPC 0x00332C80 and KeInitializeInterrupt (0x003333D3) with the
 * routine 0x003322D0 on vector 3. The DPC reads PMC_INTR ([edi+0x100]) and
 * calls the PGRAPH handler 0x00332A40 (bit 0x1000) and the vblank handler
 * 0x003324D0 (bit 0x1000000), both thiscall with ecx = the block whose +0
 * is the register base and +0x1BC / +0x1C0 the flip retire index and vblank
 * count (the same offsets appear in 0x003322A2-0x003325B0). Both run at
 * DISPATCH_LEVEL so they cannot interleave with D3D's own busy-waits; the
 * PGRAPH handler takes the executor's trap under its lock; retired flips go
 * to the executor's FLIP_STALL. */
extern uint8_t xbox_KfRaiseIrql(uint8_t irql);
extern void xbox_KfLowerIrql(uint8_t irql);
extern uint8_t xbox_CurrentIrql(void);
extern int xbox_gil_suspend(void);
extern void xbox_gil_resume(int depth);
extern void xbox_Nv2aVblankTaken(void);
extern void nv2a_pb_flip_retired(unsigned n);
extern void nv2a_pb_trap_lock(void);
extern void nv2a_pb_trap_unlock(void);
extern void nv2a_pb_trap_taken(void);
extern void sub_003324D0_gen(void);
extern void sub_00332A40_gen(void);

static uint8_t d3d_isr_enter(void)
{
    uint8_t old = xbox_CurrentIrql();

    if (old < 2) {
        int d = xbox_gil_suspend();
        old = xbox_KfRaiseIrql(2);
        xbox_gil_resume(d);
    }
    return old;
}

static void d3d_isr_leave(uint8_t old)
{
    if (old < 2)
        xbox_KfLowerIrql(old);
}

void sub_003324D0(void)
{
    uint32_t blk = ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t head = MEM32(blk + 0x1BC);

    xbox_Nv2aVblankTaken();
    sub_003324D0_gen();
    nv2a_pb_flip_retired(MEM32(blk + 0x1BC) - head);
    d3d_isr_leave(old);
}

void sub_00332A40(void)
{
    volatile uint32_t *nv = (volatile uint32_t *)XBOX_PTR(0xFD000000u);
    uint32_t blk = ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t head = MEM32(blk + 0x1BC);
    uint32_t nsource;

    nv2a_pb_trap_lock();
    nsource = nv[0x400108 / 4];
    sub_00332A40_gen();
    nv2a_pb_flip_retired(MEM32(blk + 0x1BC) - head);
    if (nsource)
        nv2a_pb_trap_taken();
    nv2a_pb_trap_unlock();
    d3d_isr_leave(old);
}

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    /* sheroes-nx: the call site (the return address the call pushed, at
     * [esp]) and the stack words above it, rate-limited per site: the first PC
     * stage run logged 19,765 null calls at 18 lines each (2026-10-28). */
    enum { SITES = 64 };
    static uint32_t site_ra[SITES], site_n[SITES];
    uint32_t ra = g_esp ? MEM32(g_esp) : 0;
    int k;
    for (k = 0; k < SITES && site_ra[k] && site_ra[k] != ra; k++)
        ;
    if (k < SITES) {
        site_ra[k] = ra;
        if (++site_n[k] > 3) {
            if ((site_n[k] & (site_n[k] - 1)) == 0)
                fprintf(stderr, "[ICALL] 0x%08X from 0x%08X: %u times\n", va, ra, site_n[k]);
            return;
        }
    }
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu), return address 0x%08X, "
            "stack %08X %08X %08X %08X %08X %08X, ecx %08X\n",
            va, (unsigned long long)g_icall_count, ra,
            MEM32(g_esp + 4), MEM32(g_esp + 8), MEM32(g_esp + 12), MEM32(g_esp + 16),
            MEM32(g_esp + 20), MEM32(g_esp + 24), g_ecx);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

/* ── Untranslated instructions ───────────────────────────────────────────
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, in place of the bare comment it used to leave. The
 * instruction is still a no-op; this only stops the omission being silent.
 * RECOMP_UNIMPL_TRAP=1 aborts at the first hit, at the guest address of the
 * cause rather than wherever the damage surfaces. */
#include <stdlib.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}
