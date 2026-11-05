#!/bin/bash
# Test11 A/B control: restore original modulo-based VBlank IRQ calculation.
# Preserves the same DSP and all earlier optimizations.
export KI_R36S_VBLANK_BASELINE=1
export KI_R36S_LOG_BASENAME=ki-r36s-vblank-baseline
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
