#!/bin/bash
# Test17: exact-color ABGR8888 texture upload for R36S OpenGL ES2.
# ALWAYS write a preflight log, even when no updated executable exists.
set +u
PORTS="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$PORTS/killerinstinct"
MAIN="$PORTS/Killer Instinct Arcade.sh"
BIN="$GAMEDIR/${KI_R36S_BINARY:-ki_play.aarch64}"
LOGDIR="$GAMEDIR/logs"
if ! mkdir -p "$LOGDIR"; then
  echo "ERROR: cannot create R36S log directory $LOGDIR"
  exit 1
fi
PREFLIGHT="$LOGDIR/ki-r36s-gles2-rgba-preflight.log"
{
  echo "KI R36S Test17 GLES2 RGBA preflight | $(date -Iseconds)"
  echo "Ports: $PORTS"
  echo "Binary: $BIN"
  if [ ! -f "$MAIN" ]; then
    echo "ERROR: Killer Instinct Arcade.sh missing from Ports directory."
    exit 127
  fi
  if [ ! -f "$BIN" ]; then
    echo "ERROR: updated ARM64 executable missing. Install complete Test17 ZIP."
    exit 127
  fi
  if ! LC_ALL=C grep -a -F -q 'GLES2 framebuffer upload:' "$BIN"; then
    echo "ERROR: this is an old Test16 executable, without GLES2 RGBA support."
    echo "Install the complete Test17 ZIP with ki_play.aarch64, not only .sh files."
    exit 2
  fi
  echo "OK: Test17 executable detected. Launching full-audio arcade game."
} >> "$PREFLIGHT" 2>&1
export KI_R36S_GLES2_RGBA=1
export KI_R36S_GFX_PROFILE=1
export KI_R36S_LOG_BASENAME=ki-r36s-gles2-rgba-fast
exec "$MAIN" "$@"
