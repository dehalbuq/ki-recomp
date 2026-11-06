#!/bin/bash
# Test17: hardware SDL OpenGLES2 native RGBA upload A/B experiment.
# The SDL RGB24 path remains the default "Killer Instinct Arcade" entry.
# Converts original 320x240 BGR555 framebuffer to ABGR8888 without color
# loss, then uploads a 4-byte-per-pixel texture instead of RGB24.
# Full audio, MIPS Turbo, original frame speed, no frame skip.
export KI_R36S_GLES2_RGBA=1
export KI_R36S_GFX_PROFILE=1
export KI_R36S_LOG_BASENAME=ki-r36s-gles2-rgba-fast
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
