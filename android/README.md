# Killer Instinct Android ARMv7 — experimental cloud build

This public patch contains only source, not original ROMs, CHDs or generated MIPS code.
The matching private workflow must live in the owner's **PRIVATE** repository.

- ABI: `armeabi-v7a` only.
- Min SDK: API 19 (Android 4.4), NDK r25c.
- Renderer: SDL2/OpenGL ES 2, 4:3, nearest-neighbor scaling.
- SDL2 pinned to `release-2.30.12` by private Actions checkout.
- The private APK includes lawfully supplied original files only for private testing;
  the public branch and public CI must NEVER contain the assets/APK.
- First run copies the original assets from the private APK into app-private
  storage, then launches the native SDL2 game engine. The app can require
  substantial free storage and native RAM because the original code expands
  the entire CHD in memory. This is not yet optimized for very low-RAM devices.
- Android 0.3 adds physical-gamepad scoring and rejects Chanuo A1 IR / virtual
  input devices (notably `gpio_ir_recv`, `virtual-search`, `mtk-kpd`) so they do
  not take player 1. The iPega PG-9068 (VID 6473 PID 1026) is polled as a
  raw SDL joystick each frame, using its verified D-pad buttons 11-14 and
  left-stick axes 0-1. SDL GameController support remains for other devices.
- Hot-plug and SDL app foreground events re-enumerate physical controllers.
  Touch controls are not implemented yet.
- Initial v0.1 APK was confirmed working on Android ARMv7, including Bluetooth gamepad and resume after standby (user hardware test).
- v0.2 is the **audio smoothing experiment**: SDL device buffer grows from 1024 to 2048 samples (at 31250 Hz); queued PCM is primed before playback and the existing R36S WSOLA algorithm can compensate for sustained guest-frame slowdowns without changing arcade guest timing.
- The exact original PCM queue remains available for A/B comparisons by setting `KI_ANDROID_AUDIO_ORIGINAL=1` in the SDL Android app environment (for example via `SDL_ENV.KI_ANDROID_AUDIO_ORIGINAL` in the KIActivity AndroidManifest and rebuilding).
- Inspect Android logcat with `adb logcat -v time KI-Audio:I '*:S'` to see queue occupancy, the number of frames below 20ms, and the stretching ratio. A steady rise in underflow count means more than hardware buffering may be needed.
- v0.3 is pending Chanuo A1 hardware testing. Check D-pad, analog stick,
  six arcade attack buttons, START/COIN, Bluetooth reconnect after sleep and
  whether audio v0.2 smoothing still works as expected.
- To collect the joystick diagnostics using ADB on a connected Android device:
  `adb logcat -v time KI-Gamepad:I SDL:I '*:S'` (look for `[ki-android-pad]`);
  alternatively capture full logcat and filter for `ki-android-pad`.
  These logs show the selected device and active-low P1/P2 inputs.
- Debug signing is used for this **private test** only, not production release.

To build, use `build-android-armv7.yml` from this package in the owner's
private build repository, rather than putting any game assets in the public fork.
