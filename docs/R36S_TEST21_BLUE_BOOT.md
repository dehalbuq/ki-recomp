# R36S Test21: accelerate the descending blue startup screen

The RK3326 has a strong CPU limit in the cold-boot sequence. In the
user's normal Test20 log, extracting and expanding the original CHD
takes ~7.8 seconds *before* game emulation starts. The first five
seconds of emulation then averaged 46.3 FPS, with host CPU usage ~93%.
The diagnostic Test20 log measured a DSP workload of 13.90 ms/frame
during this initial boot phase. Gameplay has a different performance
profile; do not change the MIPS clock to improve perceived startup.

## Startup-only speedup

Test21 **does not** change the emulator guest cycle clock or patch
game ROM data. After the initial file load, a fast and bounded
blue-region detector examines 360 RGB555 pixels from the current
guest SRAM framebuffer once per VBlank. It detects a largely solid
blue region progressing from the top of the screen toward the bottom.

A positive match needs three consecutive frames with a partly
covered screen. Only during that verified boot transition:

- Normal 58.98 Hz host sleep is bypassed so the boot MIPS/DSP emulation
  runs as fast as the Cortex-A35 can process it, with unchanged guest
  cycle counts and unchanged game-state transitions.
- One frame out of four is sent to the GLES2 renderer. The other
  frames are *still fully emulated* (no skipped game simulation).
- Audio DSP computation is **not disabled**. Host SDL audio playback
  is temporarily muted to prevent accelerated samples from accumulating
  an excessive queue. At the end of the wipe, WSOLA is re-primed and
  normal sound playback resumes.

The detector automatically switches off once the blue region covers
the screen or disappears, and never activates beyond the first 720
guest VBlanks (~12 original-arcade seconds).

The normal PortMaster launcher enables it via:

```bash
export KI_R36S_BOOT_BLUE_FAST="${KI_R36S_BOOT_BLUE_FAST:-1}"
```

To disable this feature for diagnosis, run with the environment
variable `KI_R36S_BOOT_BLUE_FAST=0`. Do not permanently disable
emulated DSP, patch emulated clocks or run the gameplay in turbo mode.

## What to check on actual R36S hardware

1. Install the Test21 private PortMaster ZIP over the previous
   installation; preserve ROM/CHD and save files.
2. Launch `Killer Instinct Arcade` and observe the blue wipe.
3. Check `killerinstinct/logs/ki-r36s.log` for
   `BOOT_BLUE transition=FAST` and `BOOT_BLUE transition=NORMAL`.
4. If the log shows only `BOOT_BLUE probe` lines, the screen is
   not uniform enough for this first visual detector. The feature
   remains inactive and safe; send the log to calibrate the visual
   signature to the actual screen.
5. If a normally paced fight is unexpectedly accelerated, disable the
   feature immediately with `KI_R36S_BOOT_BLUE_FAST=0` and provide
   the corresponding log; it should be impossible after the bounded
   cold-boot detection window, but firmware/gameplay regression
   testing is mandatory.
6. Compare total boot duration and look for correct title screen,
   Start+Select exit, fully functioning sound and normal combat speed.

Important: the 7.8-second CHD expansion itself is not changed here;
this only addresses the blue animation *after* the disk is loaded.
A real-world improvement is not yet verified; the speed depends on
remaining CPU/DSP workload even when SDL rendering is reduced.

ROM-free `boot_wipe_selftest` tests a synthetic downward blue mask,
grayscale/red false positives, finish conditions and maximum boot
window. No proprietary data is required for this validation.
