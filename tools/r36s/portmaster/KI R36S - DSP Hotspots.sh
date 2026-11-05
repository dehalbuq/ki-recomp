#!/bin/bash
# Diagnostic ONLY: samples 1 in 1024 executed ADSP instructions, reports hot
# firmware PC + opcode every 300 guest frames, and keeps PCM trace enabled.
# Use normal Killer Instinct Arcade for performance comparisons.
export KI_R36S_DSP_HOTSPOTS=1
export KI_R36S_AUDIO_TRACE=1
export KI_R36S_LOG_BASENAME=ki-r36s-dsp-hotspots
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
