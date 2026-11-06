# Killer Instinct for R36S — Test 12: optimize MIPS COP0 polling

## Outcome of Test 11 (dArkOSRE, RK3326)

From the user's two logs, `ki-r36s(5).log` (experimental VBlank
deadline path) and `ki-r36s-vblank-baseline.log` (original modulo):

| Last ten 5-second samples | Fast VBlank | Original VBlank |
|---|---:|---:|
| Guest FPS average | 32.30 | 33.65 |
| Render, ms per guest frame | 7.276 | 7.276 |
| SDL swap, ms per guest frame | 2.023 | 2.031 |
| Process CPU utilization | 97.9% | 98.0% |
| Storage physical reads per 5s | 0 MiB | 0 MiB |

The experimental VBlank deadline path **did not help** in this
particular real-hardware A/B; it measured ~4.0% fewer frames
than the original, although scenes/durations were not exactly
identical. Both runs exited cleanly; the DSP waitloop accelerator
was enabled in both.

**Test12 restores the original modulo-based VBlank IRQ calculation
by default.** To separately re-enable Test11's experiment, set
`KI_R36S_VBLANK_FAST_EXPERIMENT=1`; the included diagnostic launcher
`KI R36S - VBlank Fast Experiment.sh` does exactly that, and disables
the COP0 inlining for a direct comparison.

## Next candidate from the Test 10 CPU samples

The prior native ARM64 profile reported 10.0% of samples in
`KinstBoard::interrupt_lines` and 9.9% in `ki::mfc0`. Calls to
`mfc0` are emitted by the static MIPS recompiler for each guest
MFC0/DMFC0 instruction. The register number is known **at translation
time**, but the existing code performed an out-of-line C++ call and
switched on the register for every guest read.

The new `ki::mfc0_recompiled` wrapper in `runtime.h` provides a
conservative R36S-only inline path: the compiler can specialize it
at each call site, with a constant register index. It reads Count,
Cause, PRId and other COP0 registers with **exactly the same
semantics** as the original `ki::mfc0` function:

- Count: `uint32_t(count_base + cycles / 2)`
- Cause: preserved software/timer bits combined with board IRQ bits
  `io->interrupt_lines(c) & 0x7c00`
- PRId: `0x2020`
- Other registers: `cp0[reg & 31]`

The original `ki::mfc0` is unchanged, and the generated code uses the
inlined method only with `KI_R36S_FAST_COP0`. Other targets compile to
the original implementation.

**Test12 normal gameplay enables this optimization.** The PortMaster
launcher `KI R36S - COP0 Baseline.sh` disables it via
`KI_R36S_COP0_BASELINE=1`, leaving *all other* optimizations identical.

The native ARM64 CI runs `mfc0_inline_selftest`, a ROM-free
differential test covering all 32 register numbers at multiple cycle
values, board IRQ line masks, and both a present/absent I/O handler.

## Install and test

Extract the Test12 ZIP into `roms/ports/`, preserving
`roms/ports/killerinstinct/gamedata/kinst.zip`,
`kinst.chd` and saved data. An older script named
`KI R36S - VBlank Baseline.sh` may remain if it was installed by
Test11: it is obsolete and can be deleted from `roms/ports/`.

Use the SAME scene and input sequence for ~45–60 seconds:
1. **Killer Instinct Arcade** (original VBlank, COP0 inlined).
2. **KI R36S - COP0 Baseline** (original VBlank, COP0 out-of-line).

Upload:
- `killerinstinct/logs/ki-r36s.log`
- `killerinstinct/logs/ki-r36s-cop0-baseline.log`

Compare after the first two warmup windows, ideally from a deterministic
game state. Do not use the Host Profile launcher's FPS for baseline
timing because it adds CPU signal-sampling overhead.

If COP0 inlining does not deliver a repeatable gain, disable it.
The next options are measuring the translated guest function
`func_880009a4` and exact inline paths for unaligned
`ldl`/`ldr`, where correctness requires careful tests. Avoid
altering guest clock, sound, VBlank interrupt ordering or video
frame presentation merely to inflate FPS.
