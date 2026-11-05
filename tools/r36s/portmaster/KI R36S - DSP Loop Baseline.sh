#!/bin/bash
# A/B control: retain the Test05 fast DSP memory map but disable the new
# KI polling-loop microblock acceleration. Keep audio and frame timing intact.
export KI_DCS_BASELINE_HOTLOOP=1
export KI_R36S_LOG_BASENAME=ki-r36s-dsp-loop-baseline
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
