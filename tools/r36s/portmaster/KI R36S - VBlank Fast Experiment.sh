#!/bin/bash
# Re-enable the Test11 fast VBlank deadline experiment. Disabled by default
# after a 4% slowdown against the original modulo path on dArkOSRE.
export KI_R36S_VBLANK_FAST_EXPERIMENT=1
export KI_R36S_COP0_BASELINE=1
export KI_R36S_LOG_BASENAME=ki-r36s-vblank-fast-experiment
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
