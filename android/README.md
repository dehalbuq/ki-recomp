# Killer Instinct Android ARMv7 — experimental cloud build

## v0.6 Adaptive touch controls and guarded Back exit

- The Android app checks `PackageManager.FEATURE_TOUCHSCREEN` and passes
  `--touchscreen` only on real touch-capable devices. Android TV without a
  touchscreen retains the no-overlay 4:3 centered display.
- Touch and Bluetooth/USB inputs are combined with bitwise OR (active-low P1
  cabinet bits); plugging in a physical controller does not hide the overlay
  or interrupt touch controls.
- The SDL2 overlay supports multiple concurrent fingers: 4-way D-pad, six
  cabinet attacks (H1/H2/H3 and L1/L2/L3), COIN, START, and EXIT.
- In landscape, the game remains centered in the area between the left and
  right touch rails; in portrait, the game is centered in the upper ~60% and
  controls fill the lower section. Integer texture scaling is preferred.
- Android Back is trapped by `KIActivity` and forwarded as a native key
  event. First Back displays an in-game confirmation; a second distinct Back
  within five seconds exits. The touchscreen EXIT button follows the same
  two-step confirmation policy. Double-Back is not needed for the Home button.
- The activity supports sensor-based orientation changes. Background events
  clear all active finger contacts so no attack remains stuck after resume.
- The existing Chanuo A1 PG-9068 raw HID fallback, SDL audio smoothing,
  ARMv7/ARM64 native libraries, Android TV Leanback banner, original assets
  and SDL2/GLES2 renderer remain unchanged.

## Android TV / Google TV — v0.4 universal

The private CI now packages one APK with both `armeabi-v7a` and `arm64-v8a`.
It keeps Android 4.4 / API 19 support for ARMv7, and Android 5.0 / API 21+
for ARM64. A device that supports only x86/x86_64 cannot install these ABIs.

The app has `MAIN` + `LEANBACK_LAUNCHER` intent filters on its launcher
activity, optional leanback/touchscreen/faketouch features, and a generated
320x180 TV banner plus an app icon. Source art is created without proprietary
game graphics by running `python3 android/tools/make_tv_art.py` before Gradle.

TV remote input uses SDL's Android key mappings. D-pad navigation already
uses the arcade keyboard direction bits. For the OK / D-pad center key,
a short press inserts COIN, and a hold of 650ms presses START. The Back
key exits; Bluetooth or USB gamepad is recommended for all six arcade attacks.
Previous Chanuo A1 PG-9068 raw HID detection and SDL audio smoothing remain.

The private release artifact remains restricted to the owner's private Actions
run because it embeds original game assets. Do not distribute the APK or
game-derived files to the public repository. Debug signing on ephemeral cloud
runners may differ between builds, so installing over an older test APK can
require backing up saved game data and uninstalling the previous version.

This public patch contains only source, not original ROMs, CHDs or generated MIPS code.
The matching private workflow must live in the owner's **PRIVATE** repository.

- ABI: `armeabi-v7a` + `arm64-v8a` (universal).
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
  Touch controls are automatically activated on real touchscreen devices since v0.6.
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
