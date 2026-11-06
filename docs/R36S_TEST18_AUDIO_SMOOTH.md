# R36S Test18: retain native GLES2 RGBA performance, reduce DCS audio crackling

## Grounded hardware diagnosis (user's Test16 and Test17 logs)

- Test17 started correctly; preflight confirmed the native ARM64 executable,
  `[r36s] GLES2 framebuffer upload: ABGR8888 (native RGBA)`.
- Test16 original RGB24 texture upload cost approximately **5.01ms/frame**,
  while Test17's RGBA upload was approximately **1.43ms/frame**; the ~71%
  upload reduction is real on RK3326 / Mali-450 GLES2.
- In active scene samples, guest FPS rose from about **45.1** to **50.4**.
  The runs were different sessions and were not precisely scene matched,
  so this is an observation rather than exact deterministic FPS gain.
- The Test17 sound queue reported **0 bytes in 5/14 five-second snapshots**,
  and was below 20ms of PCM in 10/14 snapshots. The sound device consumes
  **31,250 samples/second**. Yet the emulated DSP generates PCM based on
  arcade guest frames; when the entire guest runs at ~50 FPS instead of
  ~58.98, only roughly **26,500 guest samples arrive per wall second**.
  Merely increasing the SDL queue from 50 to 90ms postpones but cannot
  eliminate a **continuous** production deficit.
- Some crackles could also be discontinuities in DSP-produced PCM or
  device-specific behavior. Audio queue starvation is the leading *testable*
  explanation, not yet an exclusive confirmed cause.

## Experimental code introduced in Test18

- `src/ki_r36s_audio_smooth.h` contains a short-grain
  waveform-similarity overlap-add (WSOLA/SOLA) mono S16 stretcher.
  The DSP itself and the original game-generated PCM are unchanged.
  On the **host playback only**, the code produces approximately
  1.18 output samples per input sample during a stable 50 FPS scene;
  this stretches audio duration without directly lowering the PCM
  sample rate and tries to maintain the perceived frequency/pitch.
- A 512-sample source window, 256-sample synthesis hop and 12-sample
  alignment search amortize phase transitions; the interpolated
  overlap samples are bounded to the two original S16 samples, no
  volume gain or clipping.
- A smoothed ratio follows observed host wall-clock time per guest
  frame, limited to ~0.85–1.75. Short-term SDL queue depth gives a
  restrained feedback correction toward ~85ms playback backlog.
- SDL stays paused until ~90ms of actual produced game samples
  (after time stretching) has been queued. No arbitrary silence is
  added at first frame. This prevents immediate startup underruns,
  while avoiding a permanent ~200ms sound delay.
- The existing 500us per-frame audio pacing adjustment previously
  engaged whenever >60ms were queued. It now engages at >160ms
  **only when adaptive smoothing is enabled**, so maintaining an
  85ms buffer does not itself reduce guest FPS.
- On pause/resume and loading a savestate, the experimental queue
  is cleared and re-primed to avoid replaying stale sound.
- The previously validated MIPS Turbo, fast DSP and original audio
  code remain untouched. **By default, the audio is exactly the old
  PCM path**. New behavior requires `KI_R36S_AUDIO_SMOOTH=1`.
- This is an experimental time-stretch playback method. It may
  produce phasing, soft reverberation, latency or altered transients.
  It is NOT yet a proven high-fidelity algorithm for every stage.
  It is not a license to change the game's DSP sound code.

## PortMaster A/B entries

Both launchers enable the same fast GLS2 RGBA presentation, the same
original guest sound/DSP data, all frames and default guest clock.

- **KI R36S - RGBA Audio Original** — normal unmodified audio samples;
  `killerinstinct/logs/ki-r36s-rgba-audio-original.log`.
- **KI R36S - RGBA Audio Smooth** — host-only adaptive WSOLA playback;
  `killerinstinct/logs/ki-r36s-rgba-audio-smooth.log`.

Use the same arena/characters and play both for at least 60 seconds.
Avoid comparing an idle attract scene against a heavy fight. Listen
for stuttering and pitch/echo, and check input/audio latency. The new
log prints every five seconds:

```
[r36s] AUDIO_SMOOTH frames=N in=N out=N stretch=1.20
  queue_min_ms=N queue_max_ms=N under20=N primed=1 sdl_queue_ms=N
```

An ideal test produces `under20=0` *after initial priming*,
the output count exceeds the original input count when below
59 FPS, and the late-game queue does not grow unbounded.
If clicks persist despite a stable queue, the next target is
DSP source PCM discontinuity and SDL output format resampling,
not further video optimization.

## Test automation and safety

`tests/audio_smooth_selftest.cpp` builds without game data and
validates 59/50/43 FPS and 440Hz synthetic signals, input integrity,
bounds and reset behavior. It is part of the native ARM64 public CI
and private build's ROM-free target dependency chain. It does not
prove that the real game/driver output is click-free.

The new test package deliberately includes **no game ROMs or CHD**.
Preserve the original `gamedata/kinst.zip`, `kinst.chd`, saves,
4:3 nearest filtering, native 58.98Hz guest target, full audio,
Start+Select exit and the working exact-color GLES2 RGBA upload.
