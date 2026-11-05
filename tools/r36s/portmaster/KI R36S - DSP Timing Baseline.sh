#!/bin/bash
# Test09 A/B reference: same DSP timing profiler but original unaccelerated
# three-instruction wait loops. Both modes have full sound.
export KI_R36S_TIME_DSP=1
export KI_DCS_BASELINE_HOTLOOP=1
export KI_R36S_LOG_BASENAME=ki-r36s-dsp-timing-baseline
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
