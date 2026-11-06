#!/bin/bash
# Test16: profile original guest blitters for GPU/GLES2-offload feasibility.
# Does NOT change game graphics, DSP, 4:3 pixel scaling or guest frame timing.
# Reports wall-clock CPU MIPS sprite, panorama, prerendered stage calls.
export KI_R36S_GFX_PROFILE=1
export KI_R36S_LOG_BASENAME=ki-r36s-gfx-profile
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
