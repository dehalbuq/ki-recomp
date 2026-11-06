# R36S Killer Instinct — Test 07: DSP instruction hotspots

## What Test 06 established

Source: user-provided `ki-r36s-audio-trace.log` (2026-11-05).

- 11 PCM checkpoints, frames 300 through 3300 inclusive.
- At frame 3300: **1,748,318 PCM samples**, at **31,250 Hz**.
- Expected samples from the original 58.98 Hz guest clock are about
  1,748,474. The ~156-sample difference is less than one 240-sample DCS
  buffer transfer and is plausible from buffer scheduling; these
  measurements **do not prove audible fidelity**.
- Cumulative PCM FNV-1a 64-bit hash at frame 3300:
  `3218195dd9c4357d`.
- 14 DCS commands were captured with their DSP cycle timestamps.
- The last ten five-second samples averaged **31.66 guest FPS**. CPU
  ~97-98%, renderer ~7.3 ms, physical SD reads ~0 MiB.
- No crash; normal menu exit. FPS in a trace run should not be directly
  compared to a different scene or run without the tracer.

## Why sample the DSP PC?

The DCS interpreter can execute up to 10 million ADSP2105 instruction cycles
per emulated second. The Ultra64Emu 3.02 source has KI-specific high-level
DSP routines (`hleDSPMain`, `hleDSPDecode`, `hleDSPNextSample`, etc.).
To implement an **original, safe, KI-specific HLE**, we need to identify
which guest DSP loops actually dominate on RK3326 and what opcodes they run.

We **must not** replace unknown firmware loops or skip their IRQ side
effects simply because a static PC address resembles Ultra64Emu's code.

## Test 07: optional PC sampling

The normal `Killer Instinct Arcade` menu entry retains fast memory,
full sound and original guest timing without profiling.

`KI R36S - DSP Hotspots` (diagnostic only) sets:

- `KI_R36S_DSP_HOTSPOTS=1`
- `KI_R36S_AUDIO_TRACE=1`
- `KI_R36S_LOG_BASENAME=ki-r36s-dsp-hotspots`

On the ADSP interpreter, **one in 1024 instructions** is sampled into a
14-bit program-counter histogram (16,384 counters), allocated **only**
when profiling is enabled. Every 300 guest frames, the top 12 addresses
are logged with their current 24-bit instruction opcode, count and
percentage of samples. The histogram resets between windows.

Example structure (illustrative, not measured PCs):

```
[r36s] DSP_HOT frame=300 dsp_cycles=... sampled=... top=0800/xxxxxxxx:1234(42.0%),...
```

The source code is `third_party/adsp2100/adsp2105.cpp`,
`runtime/src/dcs.cpp`, and `src/ki_play.cpp`. Synthetic, ROM-free tests
verify the profiler samples exactly 1/1024 instructions and does not
change emulated cycle progression.

**Profiling itself costs CPU.** Do not interpret lower FPS here as a
regression in the normal engine. Use an unprofiled entry to measure real FPS.

## Instructions on R36S (DarkOSRE)

Install the latest Test 07 ZIP over `roms/ports`, keeping
`roms/ports/killerinstinct/gamedata/kinst.zip` and `kinst.chd`.
Run **KI R36S - DSP Hotspots** in the Ports menu. Let the game
attract sequence run through a transition or into gameplay; optionally
make one controlled fight to capture a second type of workload. Use
Start+Select -> EXIT, and send:

`roms/ports/killerinstinct/logs/ki-r36s-dsp-hotspots.log`

We will use the most frequent ADSP addresses to design safe native
routines and compare their PCM hashes, command acknowledgements,
IRQ behavior and final frame rate against the existing interpreter.

## Next stages

1. Group hot sampled PCs by boot, sample decoder, mixing, memory copies,
   idle loops or interrupt handlers.
2. Implement isolated, original ARM64 C++ replacements for **verified**
   KI firmware loops, with interpreter fallback (no external ROM blobs).
3. Add A/B reference tests to compare output PCM + IRQ/cycle schedule.
4. Port other Ultra64Emu-inspired MIPS framebuffer/background
   accelerations only after profiling their use on R36S.

This is a **profiling build**, not yet a complete KI-specific DSP HLE.
