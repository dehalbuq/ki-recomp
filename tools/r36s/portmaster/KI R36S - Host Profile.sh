#!/bin/bash
# Test20 diagnostic only: native CPU sampling, exact DSP phase timing,
# GPU upload profiling, and MIPS indirect cache hit/miss statistics.
# This intentionally adds measurement overhead; do not use it for FPS A/B.
export KI_R36S_HOST_PROFILE=1
export KI_R36S_TIME_DSP=1
export KI_R36S_GPU_PROFILE=1
export KI_R36S_DISPATCH_PROFILE=1
export KI_R36S_LOG_BASENAME=ki-r36s-test20-cpu-profile
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
