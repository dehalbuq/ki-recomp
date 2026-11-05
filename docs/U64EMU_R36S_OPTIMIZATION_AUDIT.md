# Ultra64Emu 3.02: optimization audit for native KI R36S

**Status:** source-code research/implementation plan, not a claim of finished HLE.
**Target:** RK3326 / Cortex-A35 / AArch64 / dArkOSRE / GLES2, original game
timing (~58.98 Hz), 4:3, full audio.

This document describes ideas identified in the user-supplied
`u64emu302src.zip`. The original archive is **not** committed to this
repository, and no copyrighted code or game assets were copied.

## Measured baseline (same hardware, Test 05)

Source: user logs `ki-r36s(3).log` and `ki-r36s-bench-dsp-bus.log`.
The last ten reported 5-second intervals were compared to avoid the
attract-mode startup transient.

| Path | Mean guest FPS (last 10) | Relative to baseline |
|---|---:|---:|
| Original DCS virtual Bus | 26.59 | baseline |
| Optimized DCS direct map | 28.49 | +7.15% |

The two logs show near-99% CPU, ~7.3 ms total render, ~2.0 ms
`SDL_RenderPresent`, 0.23 ms RGB conversion, 0.00 MiB additional physical SD
read per 5-second interval, and fixed CPU0 1,512 MHz. EGL/SDL renderer
name `opengles2`, flags `0xe` (accelerated **and still VSync flagged**,
despite requesting it off). The CHD is unpacked into RAM at startup
(~125 MiB) and is not a likely per-frame I/O bottleneck.

Earlier Test 04's no-DCS experiment stabilised around ~42 guest FPS while
the full game was ~27: a very strong signal that DCS is costly, **but it
also shows optimizing DCS alone is unlikely to reach 59 FPS**. The
no-DCS experiment changes simulated I/O responses and has no sound;
it is a diagnostic, not a playable baseline.

## Inventory of concrete opportunities from Ultra64Emu

The references below are exact **paths/functions in the supplied archive**,
not new imports.

### P0 — KI-specific DSP HLE, clean implementation

**Evidence:** `u64emu/src/hleDSP.cpp`: `hleDSPMain`,
`hleDSPDispatch`, `hleDSPDecode`, `hleDSPCopySample`,
`hleDSPNextSample`, `hleDSPUpdateAudio`, and `hleDSPFunc[A-L]`.
`u64emu/src/iCPU.cpp`: `iDspThreadProc`, `iCpuStepDSP`.
`u64emu/src/EmuObject1.cpp`: `CEmuObject::UpdateAudio`.

- The old emulator has firmware-specific **high-level** sample/voice
  handling, avoiding millions of ADSP instructions on the CPU.
- Its audio worker can operate separately from the MIPS CPU. Porting the
  old thread/sleep model unchanged would be incorrect: Windows timer and
  DirectSound assumptions are different from Linux/SDL2, and DSP
  command-latch/IRQ ordering must remain deterministic.
- A **new** fixed-point mixing and sample-decoding engine can be coded
  in C++ for ARM64. Investigate safe NEON vectorization for repeated
  per-voice mixing *only after* sample-exact scalar tests.
- Keep DCS interpreter as runtime fallback and test with the real,
  hash-validated KI L1.5D sound ROMs privately. The HLE fast path
  must be disabled automatically for an unknown firmware.
- The main CPU expects specific DCS input-empty status, IRQ2 ack and
  response timing. Never just skip the DSP's work or artificially force
  a "ready" status.
- Verify PCM sample hashes, output cadence, command acknowledgements,
  DCS bank changes, audible fidelity, and guest speed at identical
  input sequences. Protect cold boot, attract, character selection,
  fight, KO and save/restore.

### P1 — CPU-side HLE for expensive KI graphics routines

**Evidence:** `u64emu/src/hleMain.cpp`: `hleBuildBG` (comment at
approximately line 1080 reports ~2x speed improvement for specific
background routines), `hleScrollBlock`, `hleBackground2`,
`hleSkyAnim`, `hleScreenCopy` (`memcpy` of a fixed image region),
`hleFMVSub1`, `hleFMVSub2`, `hleTri`, and `hleLine`.

- These are mostly **software rendering algorithms on the emulated
  R4600**, not a port of DirectX/D3D itself. Candidate replacements
  are native background assembly/interpolation, sky, image copy and
  palette conversion.
- Unlike Ultra64Emu's dynamic hooking, our project uses a static
  recompilation pipeline. A new native override must be attached by
  exact MIPS function address and verified code-image SHA1.
- Preserve MIPS-visible register results, memory writes, cycle
  accounting, and interrupt timing. Compare frame-buffer checksums and
  per-frame pixels before and after each override.
- The ~2x source comment is a claim **about particular old routines**;
  it is not an estimate of full-game gains on Cortex-A35.
- **Prioritize measuring actual MIPS call hotspots first**. Avoid
  blindly overriding a function that seldom runs.
- The current `CMakeLists.txt` already supports the `--override` path
  (`KI_OVERRIDES`) and provides `func_<address>_original` as a
  fallback. Candidate hooks are already wrapped in
  `src/overrides/sprites.cpp` (including stage rendering
  `func_88030b9c`, setup `func_8803108c`) and
  `src/overrides/fight.cpp`. This lets us add **verified** native
  background/copy specializations without changing the translator's
  function-discovery algorithm. Do not assume old Ultra64Emu PC addresses
  correspond directly to those recompilation entry points.

### P1 — Fast guest RAM access and page-based specialization

**Evidence:** `u64emu/src/dynaSmartMem.cpp`: `dynaBuildReadMap`,
`dynaBuildWriteMap`; `u64emu/src/dynaFastMem.cpp` describes
per-access specialization. `u64emu/src/DynaCompiler.cpp` maintains
a native-register cache and exploits constant/GPR-zero cases.

- Our `runtime/include/ki/runtime.h::ram_ptr` currently performs
  address translation, alignment checking, range checking and fallback
  to an I/O handler for guest memory operations.
- Candidate: branch-light fast path for proven SRAM/DRAM addresses
  with cached host base pointers; retain the existing fully checked
  slow path for MMIO, TLB zero-page mapping, unaligned accesses and
  exceptions. A generated-code per-site specialization is safer
  than globally guessing which page a register will reference.
- Do **not** copy Ultra64Emu's x86 self-modifying/JIT patches into the
  portable AArch64 executable. They require ARM instruction encoders,
  I-cache invalidation and writable+executable memory (often not
  acceptable to PortMaster environments).
- Differential memory tests should cover SRAM, DRAM, 0x0000 TLB,
  KSEG0/KSEG1 aliases, byte/halfword/dword sizes, misalignment,
  unmapped reads and writes, and game-level checksums.

### P2 — Control-flow/branch optimization for recompiled MIPS

**Evidence:** `u64emu/src/dynaBranchSP.cpp` specializes **same-page**
branches, `DynaCompiler.cpp` handles register caching and
`GPR0_OPTIMIZATION`. These techniques fit a recompilation pipeline
but need adaptation for the existing static translator.

- Profile indirect call lookup, branch-heavy tight loops, MIPS
  register spills and `ki::service` interrupts. Inline direct
  known-target calls or precompute dispatch only where safe.
- Existing `-O2 -mcpu=cortex-a35` is already active. Blindly enabling
  aggressive floating-point optimizations / `-ffast-math` could
  change guest behavior. Measure rather than assume.
- Test state/exception/fault handling and reproducible frame
  checkpoints after each change.

### P2 — Video and data movement

**Evidence:** `hleMain.cpp::hleScreenCopy` uses bulk memcpy;
`mmDisplay.cpp` handles Windows DirectDraw/Direct3D.

- A native memcpy/memmove override for specific validated MIPS
  screen-transfer functions may remove large interpreted copy loops.
- Our SDL2 renderer already uses GLES2 and nearest-neighbor 4:3.
  `SDL_RenderPresent` still accounts for ~2 ms; the reported VSync
  flag remains active after the hint. Explore driver-specific
  EGL swap interval in a separate build and profile carefully.
- Compare an RGB555 16-bit SDL texture against current RGB888
  conversion without altering pixel colors. Expected benefit limited
  because current RGB conversion is only ~0.23 ms.
- Do not transplant DirectX code or rely on source-specific x86
  inline assembly; implement equivalent functionality through GLES2.

### P3 — IDE / CHD / audio buffering

**Evidence:** `u64emu/src/iATA.cpp`: sector handling;
`EmuObject1.cpp::UpdateAudio`: circular audio buffers.

- CHD is already fully decompressed (~125 MiB) at startup, and logs
  show 0 physical SD reads while playing; another cache would mainly
  affect startup, not FPS. Avoid caching decompressed game data
  on the SD as a performance fix.
- SDL audio queue often reports 0 bytes with current low frame rate,
  consistent with DSP not producing samples quickly enough. A new
  audio engine should generate PCM in bounded blocks. Buffering can
  prevent underruns but cannot fix 27 FPS of guest simulation.
- Consider a dedicated DSP thread only with explicit cycle fencing
  and deterministic command/read barriers. The RK3326 has four cores,
  but naive parallel updates would produce races and desync.

## Prioritized implementation / acceptance tests

1. **DCS/HLE reference traces**: instrument output PCM hashes, timing,
   DCS latch transitions and command IDs (private ROMs only).
2. **New KI-only audio backend**: implement independently using
   fixed-point algorithms derived from high-level behavior;
   interpreter fallback by command/firmware gate.
3. **Differential test suite**: identical audio commands -> identical
   PCM, buffer boundaries and IRQ/state timing. Any disagreement
   returns to the interpreter.
4. **Profile host MIPS hotspots** under full audio; prioritize
   background assembly and bulk screen copy overrides.
5. **Optimize memory and dynamic call paths** with synthetic tests,
   on-device frame hash comparison, and reference save-state testing.
6. **GPU/SD last** unless metrics change: their measured share is
   comparatively small.

## Licensing / safety boundaries

The user's Ultra64Emu archive does **not** contain a clear standalone
redistributable license file. Treat it as a **technical reference only**
until rights are established. Do not commit its source, binary blobs or
derived copied functions to this fork. Create an original implementation
from observed semantics and differential tests. The existing ADSP
interpreter is derived separately from MAME BSD-3-Clause code with
attribution. All tests using commercial sound ROMs remain on private
runners, never in public GitHub Actions or public release ZIPs.

## Trace instrumentation (Test 06)

A new optional diagnostic launcher, `KI R36S - Audio Trace.sh`, sets
`KI_R36S_AUDIO_TRACE=1` and writes `ki-r36s-audio-trace.log`.
It preserves the usual audio engine but logs guest-frame-indexed,
running FNV-1a 64-bit hashes of 16-bit little-endian PCM samples, the number
of samples and sample rate every 300 guest frames. DCS command values and
DSP cycle timestamps are also logged.

Run this launcher first at an identical game state with the baseline engine,
then with a future HLE candidate. Sample data and timing mismatches are
regressions to investigate; non-identical game inputs, saves, or execution
paths will naturally make traces incomparable. The extra hashing/logging
cost is paid **only** in trace mode.

## Status

- **Done (Test 05):** DCS direct-memory path, ROM-free equivalence
  regression, safe fallback mode.
- **Done (Test 06):** optional per-guest-frame PCM hash and DCS command traces.
- **Not yet implemented:** new HLE audio backend, GPU shader replacement,
  MIPS background HLE, static per-site memory specialization, DSP threading.
- **Do not promise FPS gains** for the latter until measured on R36S.
