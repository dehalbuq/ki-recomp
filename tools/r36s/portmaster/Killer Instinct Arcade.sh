#!/bin/bash
# R36S / PortMaster test launcher for the native ki-recomp SDL2 player.
# PortMaster/DarkOSre sources control.txt and device_info.txt. These scripts
# legitimately inspect variables not defined by our game launcher, so Bash
# nounset (-u) must not be enabled while they execute.
set +u
set -o pipefail
PORTS="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$PORTS/killerinstinct"
BIN="$GAMEDIR/${KI_R36S_BINARY:-ki_play.aarch64}"
mkdir -p "$GAMEDIR/logs" "$GAMEDIR/conf"
LOG="$GAMEDIR/logs/${KI_R36S_LOG_BASENAME:-ki-r36s}.log"
exec > >(tee "$LOG") 2>&1

echo "KI R36S prototype | $(date -Iseconds)"
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

if [ ! -f "$BIN" ]; then
  echo "ERROR: no compiled ARM64 binary at $BIN"
  exit 127
fi
if [ ! -f "$GAMEDIR/gamedata/kinst.zip" ] || [ ! -f "$GAMEDIR/gamedata/kinst.chd" ]; then
  echo "ERROR: place your own kinst.zip and kinst.chd in $GAMEDIR/gamedata"
  exit 2
fi
export XDG_DATA_HOME="$GAMEDIR/conf"
if [ -n "${sdl_controllerconfig:-}" ]; then
  export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi
if [ -d "$GAMEDIR/libs.aarch64" ]; then
  export LD_LIBRARY_PATH="$GAMEDIR/libs.aarch64:${LD_LIBRARY_PATH:-}"
fi
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
