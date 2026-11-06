#!/bin/bash
# Diagnostic A/B: disable both MIPS idle and memory fast paths.
# Keeps optimized DSP and all other emulation settings unchanged.
export KI_R36S_MIPS_IDLE_BASELINE=1
export KI_R36S_MEM_BASELINE=1
export KI_R36S_LOG_BASENAME=ki-r36s-mips-baseline
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
