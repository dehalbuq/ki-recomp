# KI R36S — ADSP-2105 DSP acceleration (test 05)

## Measurements on DarkOSre

From the same user-supplied benchmark set (end-of-log 8 samples):
- Normal game, audio present: **27.1 guest FPS**.
- Audio DSP entirely disabled: **42.1 guest FPS**. (~55% higher FPS,
  but **without sound** and with different emulated I/O behavior.)
- Rendered 1 in 4 guest frames: **31.5 guest FPS** (not suitable for play).

The board reports an accelerated **OpenGL ES 2** renderer. It still
advertises VSync even after SDL requested VSync OFF. Frame conversion costs
~0.23 ms; render ~7.3 ms. sd_read remains zero during gameplay. Audio
emulation is therefore a strong bottleneck, but removing it would **not**
achieve 58.98 guest FPS on the current CPU build; CPU and graphics still
need additional work.

## User-provided reference: Ultra64Emu v3.02 source

The archive `u64emu302src.zip` was inspected **locally**, not uploaded to
the public repository. The source contains a KI1-specific HLE DSP path:
`u64emu/src/hleDSP.cpp`, `hleDSP.h`, and dispatch in `iCPU.cpp`.
This directly reproduces known firmware algorithms including boot setup,
sample decode, sample copy/mixing and command dispatch. A conventional
ADSP2100 interpreter also exists and is selectable in the emulator.

**Do not copy the Windows-era HLE verbatim**: besides relying on global
Windows/DirectSound structures, this archive has no clear redistributable
license. It is useful for understanding the algorithm; implementing a clean
native KI-specific DSP replacement requires new tests against the current
recompiled game's timing, DAC sample output, and IRQ behavior.

## First safe optimization: zero-copy DCS memory map

`third_party/adsp2100/adsp2105.h` now exposes an optional fast DCS map.
`runtime/src/dcs.cpp` attaches the actual DCS memory buffers to it.
This avoids virtual `Bus::program_read` on **every DSP instruction**, plus
virtual data/program bus calls for the hot RAM/ROM ranges.

The fast paths preserve the exact address mapping:
- program 0x0000..0x03ff: internal program RAM
- program 0x0800..0x1fff: external 0x800-word mirrored RAM
- data 0x0000..0x1fff: high 16 bits of external program RAM
- data 0x2000..0x2fff: current 0x1000-word ROM bank
- data 0x3800..0x39ff: internal data RAM
- all I/O addresses remain on the original virtual Bus path, preserving
  input latch ACK, timer, bank changes and control register side effects.

The original interpreter can be restored without recompilation by
`KI_DCS_BASELINE_MEMORY=1`. The R36S package includes the
`KI R36S - DSP Bus Baseline.sh` A/B launcher. Both modes have sound,
use the same firmware, and have unchanged emulated clock rate.

ROM-free differential CI test: `tests/dcs_fastmem_selftest.cpp`, which
compares mirrored memory reads/writes and DSP execution on synthetic NOPs.

## Next steps

1. Compare normal (fast DCS) with **DSP Bus Baseline** on the same attract
   scene; save `ki-r36s.log` and `ki-r36s-bench-dsp-bus.log`.
2. If the improvement is insufficient, profile the top ADSP PCs and the
   ADSP2105 core's per-opcode hotspots; optimize those without changing DSP
   semantics.
3. Consider a clean, optional, firmware-hash-gated HLE of only verified
   KI-specific DSP routines. Validate generated PCM audio, timing and
   command/interrupt behavior against the interpreter.
4. Optimize recompiled MIPS engine separately: DSP-free baseline is only
   ~42 FPS, still under the ~58.98 FPS goal.

The baseline is still preserved, so all risky optimizations can be disabled
for regression tests.
