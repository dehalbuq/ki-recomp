#!/bin/bash
# Test12: keep original VBlank behavior and original out-of-line MIPS COP0
# MFC0 reads. All prior DSP optimizations remain enabled, with sound.
export KI_R36S_COP0_BASELINE=1
export KI_R36S_LOG_BASENAME=ki-r36s-cop0-baseline
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
