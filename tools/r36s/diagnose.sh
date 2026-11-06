#!/usr/bin/env bash
# Read-only local firmware compatibility diagnostic; no ROMs or private saves copied.
set -uo pipefail
OUT="${1:-ki-r36s-diagnostic-$(date +%Y%m%d-%H%M%S).txt}"
{
  echo '=== KI-R36S FIRMWARE DIAGNOSTIC ==='
  date -Iseconds
  uname -a
  printf 'MODEL: '; tr -d '\0' </proc/device-tree/model 2>/dev/null || true; echo
  printf 'ARCH: '; uname -m
  echo '--- OS ---'
  cat /etc/os-release 2>/dev/null || true
  echo '--- libraries and loader ---'
  (getconf GNU_LIBC_VERSION || true) 2>&1
  (ldd --version || true) 2>&1 | head -3
  (ldconfig -p 2>/dev/null || true) | grep -Ei 'SDL2|libEGL|libGLES|libdrm|libstdc\\+\\+' | head -50 || true
  echo '--- display/input devices ---'
  ls -ld /dev/dri /dev/dri/* /dev/fb* /dev/input /dev/input/event* 2>/dev/null || true
  echo '--- environment names only ---'
  for key in SDL_VIDEODRIVER SDL_AUDIODRIVER SDL_GAMECONTROLLERCONFIG_FILE DISPLAY WAYLAND_DISPLAY; do
    if [ -n "${!key:-}" ]; then echo "$key: set"; else echo "$key: unset"; fi
  done
  echo '--- ki_play, if provided as argument 2 ---'
  if [ -f "${2:-}" ]; then
    file "$2"
    (readelf -l "$2" || true) | grep -E 'interpreter|Requesting' || true
    (readelf -d "$2" || true) | grep NEEDED || true
    (readelf --version-info "$2" || true) | grep -oE 'GLIBC_[0-9]+(\\.[0-9]+)+' | sort -Vu | tail -12 || true
  fi
} >"$OUT" 2>&1
echo "Diagnostic: $OUT (inspect before sharing)"
