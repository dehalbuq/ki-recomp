# R36S / RK3326: UltraCombo source review and adaptation plan (Test12)

Reference repository: https://github.com/thefixinhixon/UltraCombo
Checked branch: `main`, GitHub source tree `b7ae26e7605efbe53cb60433ade26d5579900d8e`.
This is a **separate native recompiler**, not a conventional hand-authored
decompilation of the original commercial game. Source is licensed MIT
(copyright 2026 thefixinhixon), while bundled shaders and third-party
libraries carry their own licenses. Cite upstream and preserve the
applicable license and notices for any incorporated code. **No ROMs,
CHD, RAM snapshots or generated copyrighted game/DSP code** belong
in this public repository.

## Key finding: UltraCombo recompiles both CPUs, including the DSP

UltraCombo's initial boot on its interpreter snapshots loaded MIPS code
and the audio firmware and translates them to C; see
`src/recomp.c`, `src/adsprec.c` and `src/rec_build.c`.

**DSP translator**: `src/adsprec.c` generates segments of 128
24-bit ADSP-2105 instructions, with one C label per guest PC, direct
`goto` for known jumps, a dispatcher for unpredictable branches,
per-instruction cycle accounting and interrupt handling. It emits
separate compilation units and shares exact instruction semantics with
the reference interpreter through `src/adsp_core.h`. In
`src/adsp.c`, program-memory writes invalidate the translated words;
execution only enters a generated region while the guest opcodes
still match the source image. Otherwise the interpreter runs. This
is the strongest candidate for our R36S audio code, **not** a
drop-in replacement for our differently organized C++
`third_party/adsp2100/adsp2105.cpp`.

**DSP idle handling**: `src/adsp.c` skips empty/polling cycles
only up to the next SPORT audio sample, and checks DSP timer/IRQ
conditions; full code translation is disabled on TinyCC, because its
output was slower than the interpreter. We already accelerate two
validated firmware 3-word waitloops at PCs 0x0130 and 0x0136, but have
not translated general audio decode/mix instructions.

**MIPS idle detection**: `src/cpu.c` recognizes an exact five-word
poll pattern `mfc0 Cause; nop; andi; beq/bne; delay-slot` and
tracks an optional `addiu` in the delay slot. Its `cpu_skip()`
advances clock and idle counter; the caller must not cross guest
interrupt deadlines. Our statically compiled MIPS code uses
`ki::poll`/safe points and may need a different integration point.
Never generalize this just from the address of a hot function.

**DSP scheduling/locality**: `src/dcs.c` queues short DSP
slices and replays them at the next host-visible boundary, preserving
order. The source comments describe approximately 4% gain on their
PC setup, not a transferable R36S measurement. Our implementation
already runs DCS to scheduled deadlines; verify the existing event
sequence before attempting a queue.

**Translation helper optimizations**: `src/rec_gen.h` caches the
hottest pointers in locals and directly handles low-risk RAM loads
and stores. Evaluate against the current generated `ki::load/store`,
`ldl`, `ldr` profiles and build compiler assembly. Our Test12
inline COP0 specialization is a related idea.

**Disk/memory**: `src/ide.c` uses a 64-slot CHD hunk cache
and keeps the settings sectors separately. Our current port expands
the ~125 MiB CHD at startup; physical SD reads measured **zero
during normal gameplay**. CHD-on-demand could save RAM or reduce
launch time, but may add latency and is not an evidence-based FPS
optimization.

**Video**: `src/video.c` targets desktop OpenGL 2.1 /
GLSL 1.20 plus an SDL renderer fallback. It is not a native
OpenGL ES 2.0 implementation. Avoid transplanting expensive CRT
shader passes to the Mali-450 without a separate GLES2 test.

## Test12 from actual user's DarkOSRE logs

- `ki-r36s(6).log`: COP0 inlining ON; 102 five-second performance
  windows, final-ten mean **40.93 guest FPS**.
- `ki-r36s-cop0-baseline.log`: COP0 inlining OFF; 25 windows,
  final-ten mean **32.87 guest FPS**.
- Both use original VBlank, fast-mapped DCS/ADSP DSP microblocks,
  GLES2 accelerated rendering at about 7.3 ms/guest frame,
  CPU at ~97–98% during gameplay and ~0 MiB physical SD reads.
- The arithmetic change in these *different-length, not obviously
  scene-aligned runs* is +24.5% in the final-ten averages. **Do not
  claim that it equals causal COP0 speedup** without aligning the
  exact attract/fight gameplay scene, controls and time window.
- Both stopped cleanly using Start+Select, with audio enabled.

## Proposed engineering order, conservative and measurable

1. **DSP translation proof of concept on ARM64**: implement a
   **separate, optional** static ARM64 C/C++ translation of a
   representative 128-instruction ADSP firmware region (plus its
   interpreter fallback), borrowing the architectural idea from
   `adsprec.c` but adapting the state, timer, PM write and interrupt
   interfaces to our C++ emulator. Compile generated code in the
   **private CI** from the legally supplied original ROM inputs;
   do not add generated firmware C/C++ or snapshot bytes to the
   public repository or distributable artifacts.
2. Every translated PC must check its original opcode before use,
   maintain ADSP flags, I/M/L addressing, SPORT DAC samples, IRQs
   and elapsed cycles **bit-for-bit** with the existing interpreter.
   On a program-memory write invalidate only affected translated
   words or fall back to the interpreter. Differential-test multiple
   sample commands and reset/IRQ states; compare complete PCM
   checksums and guest timing.
3. Benchmark the translated DSP with sound on/off only as an
   *independent attribution*, not a gameplay optimization. The
   shipping build must always have sound and should report actual
   frame rate in the same scene.
4. **MIPS Cause polling detector (second candidate)**: inspect
   `func_880009a4` and other hot functions against the five-word
   pattern in `src/cpu.c`; if actually present, implement a
   cycle-exact skip bounded by the earliest timer/VBlank/IDE/DCS
   event and account for any delay-slot counter. Use run-time
   opcode verification plus baseline A/B toggle.
5. **Memory and renderer**: only after re-profiling. Test
   inlining/reducing `ldl/ldr` helpers with ROM-free memory
   differential tests. Keep 4:3 nearest-neighbor, render all
   frames, GLES2 compatibility and actual ~58.98-Hz guest clock.

Do not blindly port the whole UltraCombo player or compiler: it
would replace working ARM64/A35-specific paths, add first-run
runtime compilation, and conflict with the current PortMaster
packaging. The realistic near-term objective is a **new optional
DSP backend with interpreter fallback**, then validated MIPS polling
acceleration.

## Source links

- https://github.com/thefixinhixon/UltraCombo/blob/main/src/adsprec.c
- https://github.com/thefixinhixon/UltraCombo/blob/main/src/adsp.c
- https://github.com/thefixinhixon/UltraCombo/blob/main/src/adsp_core.h
- https://github.com/thefixinhixon/UltraCombo/blob/main/src/cpu.c
- https://github.com/thefixinhixon/UltraCombo/blob/main/src/dcs.c
- https://github.com/thefixinhixon/UltraCombo/blob/main/src/rec_gen.h
- https://github.com/thefixinhixon/UltraCombo/blob/main/src/ide.c
- https://github.com/thefixinhixon/UltraCombo/blob/main/LICENSE
