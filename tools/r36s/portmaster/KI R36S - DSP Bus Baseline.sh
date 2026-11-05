#!/bin/bash
# Comparison ONLY: use the original ADSP2105 virtual-bus memory path.
# Same sound firmware and cycle schedule as the optimized launch.
export KI_DCS_BASELINE_MEMORY=1
export KI_R36S_LOG_BASENAME=ki-r36s-bench-dsp-bus
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
