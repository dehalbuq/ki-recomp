# R36S Killer Instinct — Test11: optimize the MIPS VBlank polling path

## Source: user's Test10 host profiler log

The user's `ki-r36s-host-profile.log` contains 36 non-overlapping
5-second CPU sampling windows. After excluding the first three startup
windows, 33 windows / **31,614** samples remain. Percentages are
native **process CPU instruction-pointer sample shares**, not guest
instructions or precise per-function timings.

| Native function | Recorded hit share |
|---|---:|
| `ki::KinstBoard::interrupt_lines` | 10.02% |
| `ki::mfc0` | 9.94% |
| `adsp::Adsp2105::run` | 9.07% |
| `ki::gen::func_880009a4` | 6.53% |
| `adsp::Adsp2105::try_ki_waitloop` | 4.42% |
| `<unknown>` | 21.16% |

Other recurring costs include `ki::ldl`, `ki::ldr`,
`ki::gen::func_880107a8_original` (sprite draw) and
`ki::gen::func_88018c98_original` (panorama draw).
Later windows average **~33.8 guest FPS**, with CPU at ~98% of one core
and no physical SD-card reads during gameplay.
The DSP acceleration remains active and has skipped **732,080,691**
guest waitloop instructions by normal exit.

The VBlank poll path is particularly important: `ki::mfc0` reads the
MIPS COP0 Cause register, which asks the `KinstBoard` for interrupt
lines. The board used to compute VBlank with
`(c.cycles + phase_) % kCyclesPerFrame >= kVblankStart` on every call.

## New exact deadline-based fast path

`runtime/src/kinst_board.cpp` now compares against the board's already
scheduled `next_vblank_` absolute cycle when the board has not passed
that event:

```cpp
c.cycles < next_vblank_
    ? (next_vblank_ - c.cycles > kVblankStart)
    : ((c.cycles + phase_) % kCyclesPerFrame >= kVblankStart);
```

The fast path avoids the modulo in the common COP0 polling path.
This preserves both VBlank start/end boundaries, snapshot phase
(`set_cycles_to_vblank`) and guest CPU clocks. Overdue or unusual
event cycles fall back to the original calculation.

The R36S build enables the fast path by default; the launcher
**KI R36S - VBlank Baseline** sets `KI_R36S_VBLANK_BASELINE=1`
and uses the original modulo formula while **retaining every DSP
optimization**.

The ROM-free differential test `tests/kinst_vblank_fast_selftest.cpp`
compares both paths over multiple phases, exact line boundaries and
several frame transitions. Passing automated tests does not prove
a hardware FPS gain; the final A/B measurement is needed.

## Resolving the remaining unknown CPU samples

The optional native host sampler now groups un-symbolized addresses
by the library module name (for example an OpenGL driver library)
instead of placing them all in one `<unknown>` bucket. This improves
diagnostic classification without modifying the normal game path.

## How to test

Extract Test11 over `roms/ports` on dArkOSRE while keeping original
`kinst.zip`, `kinst.chd` and saves.

Launch the same scene for 45–60 seconds in both modes:
1. `Killer Instinct Arcade` — fast VBlank, fast DSP.
2. `KI R36S - VBlank Baseline` — old VBlank modulo, fast DSP.

Upload `killerinstinct/logs/ki-r36s.log` and
`killerinstinct/logs/ki-r36s-vblank-baseline.log`.
The optional `KI R36S - Host Profile` launcher can additionally
identify the library behind previous unknown samples, but its FPS
should not be used for the direct A/B comparison.

If this change helps, the next candidate will be reducing the expense
of `ki::mfc0`/interrupt dispatch or an exact, verified native override
for `func_880009a4`, always with the original code kept as fallback.
Avoid skipping MIPS cycles or interrupt side effects: gameplay and
audio must remain synchronized.
