#!/bin/bash
# Test13 experimental: private-build precompiled ADSP opcode groups.
# Keeps original interpreter for all nonmatching program RAM instructions.
# Does NOT disable the sound board or modify arcade guest clock.
export KI_R36S_DSP_COMPILED=1
export KI_R36S_LOG_BASENAME=ki-r36s-dsp-compiled
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
