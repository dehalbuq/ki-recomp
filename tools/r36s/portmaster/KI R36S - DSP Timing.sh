#!/bin/bash
# Test09: measure host CPU wall-clock time inside DCS::run_until.
# The full KI DSP loop fast path remains enabled.
export KI_R36S_TIME_DSP=1
export KI_R36S_LOG_BASENAME=ki-r36s-dsp-timing
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
