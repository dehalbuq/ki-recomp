#!/bin/bash
# Test18: best native RGBA GLES2 upload plus adaptive pitch-preserving
# 31250Hz SDL audio playback when guest render rate is below 58.98 FPS.
# Original emulated DSP timing/audio and full-frame rendering are unchanged.
export KI_R36S_GLES2_RGBA=1
export KI_R36S_AUDIO_SMOOTH=1
export KI_R36S_LOG_BASENAME=ki-r36s-rgba-audio-smooth
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
