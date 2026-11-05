# Killer Instinct R36S — DSP microblock accelerator (Test 08)

## Evidence: user-provided Test07 DSP hotspot log

The Test07 log `ki-r36s-dsp-hotspots.log` recorded 20 ADSP sampling
checkpoints, at 300-frame intervals up to frame 6000. After the first
startup window, **72.65% of all sampled guest DSP instructions** among
the top-12-PC reports correspond to just six PCs:

```
0130/0d02a3   0131/26e20f   0132/181305
0136/0d02a3   0137/26e20f   0138/181364
```

These form two three-instruction register/ALU/conditional-branch loops.
At frame 6000, the PCM trace showed 3,178,718 samples at 31,250 Hz,
with stable end-of-run guest frame rate around 27–28 FPS **while the
profiler was running**. The percentage is a *guest DSP instruction
frequency*, not the host CPU utilization attributable to each loop.

## Implementation

`third_party/adsp2100/adsp2105.cpp` now contains a small, original
fast path `try_ki_waitloop()`:

- Dispatch only when PC is `0x0130` or `0x0136`.
- **At runtime, verify all three 24-bit opcode words exactly**, because
  DSP program RAM can change.
- Do not run when the opcode-PC sampler is active or when a hardware
  DO/UNTIL loop terminator overlaps any of the three words.
- Execute the first **three instructions with the same original
  register operations, flag calculation and branch condition**.
- Only if the branch jumps back to the loop head, skip identical pure
  three-op repetitions **within the current `run()` cycle budget**.
  These instructions do not access memory-mapped I/O or modify any
  callback, timer, bank, index pointer or sample buffer.
- Preserve the exact instruction count, post-instruction PC,
  previous PC, condition flags, loop condition and partial 1–2
  instruction tails at the end of any slice. IRQ/DAC scheduling remains
  at existing `run()` boundaries.
- The normal interpreter remains the fallback for any other opcode,
  firmware layout or path.

The original DSP cycle clock (10 MHz) and arcade frame clock are
unchanged. This is a **microblock/polling-loop optimization**, not a
complete DSP high-level emulation (HLE) backend.

Only CMake `KI_R36S=ON` builds enable this by default
(`KI_R36S_FAST_DSP`), while other builds remain on the reference
interpreter. The option `KI_DCS_BASELINE_HOTLOOP=1` disables the
optimization at runtime, leaving the earlier fast-memory DCS path
active.

## Validation and benchmarks

ROM-free differential test:
`tests/dsp_waitloop_selftest.cpp` executes the exact loops in
the normal and accelerated ADSP interpreters, comparing all exposed
PC/clock/index/length/modify registers at a variety of partial and
large instruction slices and different branch inputs.

The regular Ports menu is `Killer Instinct Arcade` (accelerated).
The direct A/B control is `KI R36S - DSP Loop Baseline`, which
keeps all prior optimizations but disables only this new one.

Run both in the **same scene and with the same game inputs**, for
at least 30 seconds. Both have full sound. Send logs:

- `killerinstinct/logs/ki-r36s.log` — optimized
- `killerinstinct/logs/ki-r36s-dsp-loop-baseline.log` — baseline

Each normal performance line now includes an additional log line:

```
[r36s] DSP_FAST skipped=N guest_instructions_this_period total=M
```

The acceleration bypass is disabled automatically under
`KI R36S - DSP Hotspots`, so that old sampler can still capture
real guest instruction-frequency histograms. Do **not** use profiling
mode for headline frame-rate comparisons.

## Safety

These very specific loops were observed in the user's KI L1.5D
firmware only. The project uses no code or ROM file from the
unlicensed Ultra64Emu archive. Any anomalous audio, freeze, wrong
sound effects, broken gameplay, or save-state issue should be tested
against the `DSP Loop Baseline` control. An actual R36S test is
needed to establish performance or correctness beyond synthetic
regression.

Future work is a validated, clean C++ HLE for frequently executed
DSP *audio processing* routines (mixing/decode/commands), not indiscriminately
skipping the DSP. The old interpreter remains necessary for
unknown commands and unverified game states.
