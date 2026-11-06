# Test 14 — aggressive, event-driven MIPS IRQ idle fast-forward (R36S)

## Why change strategy

The user's Test13 dArkOSRE logs show:
- Normal (AOT DSP OFF) 21 five-second windows, last-ten mean **35.15 guest FPS**.
- Experimental AOT DSP ON 19 windows, last-ten mean **33.71 guest FPS**
  (about **4.1% lower**, with nonidentical gameplay sequences).
- The compiled ADSP backend executed **2,157,389** instructions in the
  startup period, then **zero** native opcodes in every later window.
- At the end, the ADSP polling-loop accelerator had skipped
  446,610,525 guest DSP instructions in normal mode. Video still costs
  around 7.3 ms per displayed frame, 98% of one host CPU core.
- The old AOT boot-ROM specialization is therefore not a useful
  gameplay optimization. It stays off in all normal modes.

What we need is a different architecture, not more startup ROM templates.

## New Test14: exact MIPS idle loops instead of individual instructions

UltraCombo's `src/cpu.c` identifies MIPS instructions that repeatedly
read the COP0 Cause register and do nothing but wait for an IRQ:

    MFC0   $t0, Cause
    NOP
    ANDI   $t0, $t0, 0x0400
    BEQ/BNE $t0, $zero, 1b
    NOP / ADDIU counter,counter,simm

Our separate `tools/kirecomp/idle_patterns.py` implements a strict
five-word matcher. The existing MIPS translator generates the *original*
code for every instruction; only when the pattern exactly matches and
the branch is taken does it call `ki::poll_irq_idle` at that same
guest safe point. The normal game has this optimization disabled.

When `KI_R36S_MIPS_IDLE=1`, this callback:

1. Processes any due scheduled events before optimizing.
2. Verifies that currently asserted board/timer IRQ bits have *not*
   changed since the loop's just-executed MFC0/ANDI instruction.
3. Queries the next event; the KI board's new `irq_edge` query also
   considers **the end of VBlank** (the normal frame scheduler previously
   only scheduled the start). This is critical for correct IRQ wait
   loops.
4. Charges the exact guest instruction cycles for additional redundant
   iterations (MFC0 Cause=2 cycles, NOP/ANDI/BEQ/delay slot=1 each),
   stopping strictly before that event.
5. Updates an optional 32-bit sign-extending ADDIU idle-loop counter
   exactly as repeated MIPS instructions would.
6. Returns to ordinary generated code. The final iterations and
   interrupts are **never** skipped.

If the ROM contains no such complete loop patterns, no leap will
execute. No optimization is claimed until the private image survey and
real R36S logs confirm use.

## A/B procedure

The regular `Killer Instinct Arcade` entry keeps the successful Test12
COP0 inline path, original VBlank, fast ADSP direct memory and previously
validated ADSP waitloop microblocks. Its full audio and game timing are
unchanged.

`KI R36S - MIPS Turbo.sh` sets only:

    KI_R36S_MIPS_IDLE=1
    KI_R36S_LOG_BASENAME=ki-r36s-mips-turbo

A/B compare both entries in **the same game scene with similar inputs**
for 45–60 seconds. Send:

- `killerinstinct/logs/ki-r36s.log`: original MIPS execution;
- `killerinstinct/logs/ki-r36s-mips-turbo.log`: new event skipping.

The turbo log prints:

    [r36s] MIPS event-driven idle accelerator: ON (experimental)
    [r36s] MIPS_IDLE saved=N cycles_this_window total=M iterations=K

A nonzero saved cycle count is necessary (but not sufficient) evidence
for useful acceleration. If it remains zero, the recognizer did not
match the running guest loops and further architecture work is needed.

Do not use a DSP-disabled benchmark, a 1-in-4-frame renderer, or extra
debug instrumentation as a release FPS comparison. Both modes keep
the original ~58.98-Hz target and render every frame with full sound.

## Tests, caveats and next steps

- Python strict matching/rejection checks exercise the recognizer.
- `tests/mips_idle_skip_selftest.cpp` compares skip and unmodified
  loop cycles/counter values across many IRQ boundary offsets, including
  an edge occurring *between* the MFC0 and its branch.
- Tests also verify that the board's special IRQ-edge query exposes
  the falling edge of VBlank, without changing ordinary scheduling.
- The *private* pipeline counts actual candidate patterns in the
  validated KI1 L1.5D code image before compiling.
- Any anomalous crash, frozen attract sequence, sound desynchronization
  or wrong controls requires A/B fallback to the ordinary launcher.

If exact loops exist and acceleration yields a repeatable speedup,
expand to verified IDE status idle loops and other no-side-effect loops.
If none exist, redirect effort to a **private second-pass DSP native
compiler** that captures program RAM *after sound firmware loading*,
or to native specialized MIPS background/renderer functions found in
the previous host PC profile. Never misrepresent a synthetic test
as a 59 FPS hardware result.

The UltraCombo architecture is a reference, not directly copied source.
Its upstream MIT notices remain applicable if code is reused.
No original game ROM/CHD is in this public repository or in packaging.
