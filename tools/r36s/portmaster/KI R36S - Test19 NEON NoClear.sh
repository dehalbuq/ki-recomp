#!/bin/bash
# Test19 candidate: NEON + direct GLES2 locked texture + remove redundant
# black clear only for full-screen opaque 4:3 image. Compare against Locked.
export KI_R36S_GLES2_RGBA=1
export KI_R36S_AUDIO_SMOOTH=1
export KI_R36S_GPU_PROFILE=1
export KI_R36S_DIRECT_TEXTURE=1
export KI_R36S_NO_CLEAR=1
unset KI_R36S_RGBA_SCALAR
export KI_R36S_LOG_BASENAME=ki-r36s-test19-neon-noclear
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
