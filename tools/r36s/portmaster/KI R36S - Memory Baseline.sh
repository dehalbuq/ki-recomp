#!/bin/bash
# Test15 A/B: same fast MIPS idle, full DSP audio, original memory access.
export KI_R36S_MEM_BASELINE=1
export KI_R36S_LOG_BASENAME=ki-r36s-memory-baseline
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
