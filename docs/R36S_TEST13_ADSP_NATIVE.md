# R36S KI1 Test13: experimental ahead-of-time ADSP opcode specialization

Reference inspiration: https://github.com/thefixinhixon/UltraCombo (MIT source,
2026 thefixinhixon; see its `src/adsprec.c` and `src/adsp.c`).

## First actual implementation (experimental, NOT default)

The native ARM64 Test13 build now prepares source from the user's own
**u10-l1** KI1 DCS sound ROM during the **private** build:

1. `tools/r36s/generate_adsp_native.py` checks the original MAME SHA1
   for u10-l1, interprets its ADSP boot page (24-bit program words packed
   in four bytes) and selects known opcodes in the internal firmware
   range `0x0100..0x01ff`.
2. It emits a **build-directory-only** C++ include,
   `build-r36s/gen_dsp/ki_adsp_native.inc`. No firmware bytes or
   generated original-game instructions are committed to the public repo.
   CI builds without the private u10 use a synthetic ROM-free fixture.
3. `third_party/adsp2100/adsp2105.cpp` instantiates C++ templates
   with **compile-time constant 24-bit opcodes** for supported families:
   register moves, conditional jumps, ALU/MAC, immediates, data-memory
   reads/writes and parallel ALU/MAC + data-memory moves. The compiler
   can remove per-opcode decoding and register-selector branches.
4. The DSP execution loop executes the specialization **only if** the
   current program counter matches **and** the current 24-bit opcode
   equals the generated instruction. If the game modifies program
   memory, **that word automatically falls back** to the normal
   interpreter until it matches again. The existing DSP waitloop
   acceleration stays in place; non-specialized instructions also use
   the interpreter, so the audio board remains fully operational.
5. Exactly one DSP guest instruction is accounted for per translated
   opcode; existing MAME-derived sequencing, IRQ, loops, timers and
   `Dcs::run_until` cycle boundaries are not changed.

This is an **incremental native AOT prototype**, not yet the full
128-word direct-`goto` block translator implemented in UltraCombo.
Its aim is to establish numerical equivalence and practical throughput
on Cortex-A35 before extending the translated regions and adding
direct-block chaining.

## Tests

`tests/adsp_native_selftest.cpp` uses an original, synthetic DSP
program at PCs `0x0400..0x040d` (not addressable on KI DCS) and compares:

- Whole emulated guest register/flags/stacks/timer/cycle state
  via `guest_state_equals`.
- RAM/ROM writes, SPORT output, timer callbacks and exact cycle counts.
- Many partial (1, 2, 3, ... 50,007) slices repeated over 3 passes.
- An opcode changed at runtime: specialization must stop matching
  and transparently use the interpreter.
- Restoring the opcode: the compiled path must resume.

Both the public ROM-free ARM64 CI and private Test13 pipeline build
this differential test. **Passing it does not prove that the game
firmware behaves correctly**: actual KI1 scenes and PCM hashes need
on-device A/B comparison.

## Running on dArkOSRE R36S

Install Test13 over `roms/ports`, retaining the user's own
`kinst.zip`, `kinst.chd` and saves.

- `Killer Instinct Arcade`: unchanged normal Test12 DSP runtime with
  fast ADSP memory and verified waitloops; no experimental compiled DSP.
- `KI R36S - DSP Compiled`: **opt-in** compiler path, keeps all
  earlier optimizations, full audio, 4:3 and ~58.98 Hz guest timing.

Run both for 60 seconds in the same scene, using the same inputs.
Use Start+Select to exit and upload:

`killerinstinct/logs/ki-r36s.log` (baseline)
`killerinstinct/logs/ki-r36s-dsp-compiled.log` (experimental)

The experimental log must show:

```
[dcs] ADSP2105 compiled opcode experiment: ON
[r36s] ADSP_NATIVE ops=N total=M (current 5s window)
```

If `N` remains zero, the private generated boot-image instructions
are not actively used in the measured scene, or the verified DSP
waitloop optimization consumes all relevant execution. We will then
collect more firmware regions **privately** and extend generation
only where the opcode profile justifies it. If `N` is significant,
compare guest FPS and audio timing/PCM checksum, check for incorrect
sounds, freezes, saves and gameplay.

## Scope, licensing and safety

- No original ROMs/CHD are added to the public source or the ZIP as
  separate game-data files. Generated game-derived C++ and snapshots
  must remain private. Never redistribute a game-derived binary
  without confirming appropriate rights.
- The new backend is OFF by default and can be disabled by using
  the ordinary PortMaster entry. No 25 FPS cap, frame skip, GPU
  fallback or removed audio.
- Our templates are newly written against the existing BSD-3-Clause
  MAME interpreter methods; UltraCombo is used as a **design
  reference**, not copied source. Preserve its MIT copyright/license
  if literal UltraCombo implementation is later reused.
- Expect this early per-instruction dispatcher to have overhead. It
  may be slower than the baseline, particularly if firmware opcodes
  rarely match. Hardware measurements decide whether to keep it.
