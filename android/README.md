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
- Controller input uses SDL2's Android gamepad integration. Touch controls
  and Android suspend/resume hardening have not yet been implemented.
- This is an **unverified candidate**: cloud compile and hardware test required.
- Debug signing is used for this **private test** only, not production release.

To build, use `build-android-armv7.yml` from this package in the owner's
private build repository, rather than putting any game assets in the public fork.
