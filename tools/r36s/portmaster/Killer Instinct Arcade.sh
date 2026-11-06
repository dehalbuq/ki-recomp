#!/bin/bash
# R36S / PortMaster test launcher for the native ki-recomp SDL2 player.
# PortMaster/DarkOSre sources control.txt and device_info.txt. These scripts
# legitimately inspect variables not defined by our game launcher, so Bash
# nounset (-u) must not be enabled while they execute.
set +u
set -o pipefail
PORTS="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$PORTS/killerinstinct"
# Test20 default settings chosen from real Test19 dArkOSRE logs:
# lowest GPU upload time is NEON+SDL_UpdateTexture (not texture lock).
# Audio smoothing was preferred on real RK3326 hardware. No skipped frames.
export KI_R36S_GLES2_RGBA=1
export KI_R36S_AUDIO_SMOOTH=1
# Test21: accelerate only the detected initial blue descending curtain.
# Set KI_R36S_BOOT_BLUE_FAST=0 for a complete exact-speed startup A/B.
export KI_R36S_BOOT_BLUE_FAST="${KI_R36S_BOOT_BLUE_FAST:-1}"
# Obsolete Test19 candidates must never leak into the optimized default.
unset KI_R36S_DIRECT_TEXTURE KI_R36S_RGBA_SCALAR KI_R36S_NO_CLEAR
# Dual ABI PortMaster: resolve the native firmware GLIBC once at startup.
# dArkOSRE modern candidate needs GLIBC >= 2.38; the legacy binary is
# compiled in a GLIBC 2.28 container for older ArkOS4Clone variants.
# Never load the modern binary on a firmware whose libc is too old.
get_firmware_glibc() {
  local v=''
  v="$(LC_ALL=C getconf GNU_LIBC_VERSION 2>/dev/null)" || true
  if [[ "$v" =~ ^glibc[[:space:]]+([0-9]+)\.([0-9]+)$ ]]; then
    printf '%s %s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    return 0
  fi
  return 1
}
select_ki_binary() {
  local major minor
  if [ "$(uname -m)" != "aarch64" ]; then
    echo "ERROR: this PortMaster release needs a 64-bit ARM (AArch64) firmware."
    return 10
  fi
  if ! read -r major minor < <(get_firmware_glibc); then
    echo "ERROR: cannot determine firmware glibc using getconf GNU_LIBC_VERSION."
    echo "Refusing to guess an ABI or replace system libc; run the firmware diagnostic."
    return 11
  fi
  echo "Firmware glibc: $major.$minor"
  if [ "$major" -gt 2 ] || { [ "$major" -eq 2 ] && [ "$minor" -ge 38 ]; }; then
    BIN="$GAMEDIR/ki_play.aarch64"
    KI_LIBS="$GAMEDIR/libs.aarch64"
    KI_VARIANT="modern dArkOSRE (glibc >= 2.38)"
  elif [ "$major" -eq 2 ] && [ "$minor" -ge 28 ]; then
    BIN="$GAMEDIR/ki_play.glibc230.aarch64"
    KI_LIBS="$GAMEDIR/libs.glibc230"
    KI_VARIANT="legacy ArkOS4Clone (glibc 2.28-2.37)"
  else
    echo "ERROR: glibc $major.$minor is older than the supported 2.28 minimum."
    return 12
  fi
  if [ ! -f "$BIN" ]; then
    echo "ERROR: missing $KI_VARIANT executable: $BIN"
    echo "Install the unified dual-ABI ZIP; do not run a newer binary manually."
    return 13
  fi
  echo "Selected: $KI_VARIANT => $(basename "$BIN")"
  if [ -d "$KI_LIBS" ]; then
    export LD_LIBRARY_PATH="$KI_LIBS:${LD_LIBRARY_PATH:-}"
    echo "Per-variant libraries: $KI_LIBS"
  fi
  return 0
}
BIN=""
KI_LIBS=""
KI_VARIANT=""
mkdir -p "$GAMEDIR/logs" "$GAMEDIR/conf"
LOG="$GAMEDIR/logs/${KI_R36S_LOG_BASENAME:-ki-r36s}.log"
exec > >(tee "$LOG") 2>&1

# One-time Test20 migration: PortMaster extraction overlays older packages
# without deleting old launchers. Remove ONLY 23 exact previous KI benchmark
# filenames, not custom ports, the main launcher, CPU profiler, logs or saves.
legacy_ki_launchers=(
  "KI R36S - Audio Trace.sh"
  "KI R36S - COP0 Baseline.sh"
  "KI R36S - Compiler O3.sh"
  "KI R36S - DSP Benchmark.sh"
  "KI R36S - DSP Bus Baseline.sh"
  "KI R36S - DSP Compiled.sh"
  "KI R36S - DSP Hotspots.sh"
  "KI R36S - DSP Loop Baseline.sh"
  "KI R36S - DSP Timing Baseline.sh"
  "KI R36S - DSP Timing.sh"
  "KI R36S - GLES2 RGBA Fast.sh"
  "KI R36S - Graphics Profile.sh"
  "KI R36S - MIPS Baseline.sh"
  "KI R36S - MIPS Turbo.sh"
  "KI R36S - Memory Baseline.sh"
  "KI R36S - RGBA Audio Original.sh"
  "KI R36S - RGBA Audio Smooth.sh"
  "KI R36S - Test19 NEON Locked.sh"
  "KI R36S - Test19 NEON NoClear.sh"
  "KI R36S - Test19 NEON Update.sh"
  "KI R36S - Test19 Scalar Locked.sh"
  "KI R36S - VBlank Fast Experiment.sh"
  "KI R36S - Video Benchmark.sh"
)
legacy_removed=0
for legacy in "${legacy_ki_launchers[@]}"; do
  if [ -f "$PORTS/$legacy" ]; then
    if rm -f -- "$PORTS/$legacy"; then
      legacy_removed=$((legacy_removed + 1))
    else
      echo "[r36s] WARNING: could not remove obsolete launcher: $legacy"
    fi
  fi
done
if [ "$legacy_removed" -gt 0 ]; then
  echo "[r36s] Test20 migration: removed $legacy_removed obsolete KI launchers; reboot/refresh Ports list if needed."
fi

echo "KI R36S dual ARM64 | $(date -Iseconds)"
echo "Kernel: $(uname -r), architecture: $(uname -m)"
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
controlfolder=''
for folder in /opt/system/Tools/PortMaster /opt/tools/PortMaster "$XDG_DATA_HOME/PortMaster" /roms/ports/PortMaster /roms2/ports/PortMaster; do
  if [ -f "$folder/control.txt" ]; then controlfolder="$folder"; break; fi
done
if [ -n "$controlfolder" ]; then
  echo "PortMaster control script: $controlfolder/control.txt"
  # shellcheck source=/dev/null
  source "$controlfolder/control.txt"
  if [ -n "${CFW_NAME:-}" ] && [ -f "$controlfolder/mod_${CFW_NAME}.txt" ]; then
    source "$controlfolder/mod_${CFW_NAME}.txt"
  fi
  if declare -F get_controls >/dev/null; then get_controls; fi
else
  echo "WARNING: PortMaster control.txt was not found; falling back to firmware SDL2 settings."
fi

if select_ki_binary; then :; else exit $?; fi
if [ "${KI_R36S_ABI_SELECT_ONLY:-0}" = 1 ]; then
  echo "ABI selection check only: complete (game not started)"
  exit 0
fi
if [ ! -f "$GAMEDIR/gamedata/kinst.zip" ] || [ ! -f "$GAMEDIR/gamedata/kinst.chd" ]; then
  echo "ERROR: place your own kinst.zip and kinst.chd in $GAMEDIR/gamedata"
  exit 2
fi
export XDG_DATA_HOME="$GAMEDIR/conf"
if [ -n "${sdl_controllerconfig:-}" ]; then
  export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi
# Library search path was selected together with the binary above.
# Do not bundle, override or upgrade the firmware's GLIBC/ld-linux/GBM/EGL.
export SDL_RENDER_SCALE_QUALITY=0
echo "SDL video: ${SDL_VIDEODRIVER:-auto}; SDL audio: ${SDL_AUDIODRIVER:-auto}"
chmod +x "$BIN" || true
cd "$GAMEDIR" || exit 3
if declare -F pm_platform_helper >/dev/null; then pm_platform_helper "$BIN"; fi
"$BIN" --data "$GAMEDIR/gamedata" --aspect 4:3 --scale 2
code=$?
echo "ki_play exit code: $code"
if declare -F pm_finish >/dev/null; then pm_finish; fi
exit "$code"
