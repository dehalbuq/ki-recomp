#!/bin/bash
# Test18 A/B control: same fast RGBA GLES2 + MIPS/DSP optimizations,
# but SDL plays the EXACT unmodified 31250Hz game PCM queue (as Test17).
export KI_R36S_GLES2_RGBA=1
unset KI_R36S_AUDIO_SMOOTH
export KI_R36S_LOG_BASENAME=ki-r36s-rgba-audio-original
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
