#!/bin/bash
# Diagnose PCM sample sequence and DCS command timings with sound enabled.
# Not for normal gameplay: hashing/logging introduces a small measurement overhead.
export KI_R36S_AUDIO_TRACE=1
export KI_R36S_LOG_BASENAME=ki-r36s-audio-trace
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
