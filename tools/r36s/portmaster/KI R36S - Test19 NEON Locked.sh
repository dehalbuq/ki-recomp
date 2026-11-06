#!/bin/bash
# Test19 candidate: AArch64 NEON writes directly into SDL2 GLES2 streaming texture
# Identical original DSP/emulation cadence, RGBA GLES2 output, full frames,
# and pitch-preserving WSOLA. GPU-only timing (no MIPS profiler overhead).
export KI_R36S_GLES2_RGBA=1
export KI_R36S_AUDIO_SMOOTH=1
export KI_R36S_GPU_PROFILE=1
export KI_R36S_DIRECT_TEXTURE=1
unset KI_R36S_RGBA_SCALAR
export KI_R36S_LOG_BASENAME=ki-r36s-test19-neon-locked
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
