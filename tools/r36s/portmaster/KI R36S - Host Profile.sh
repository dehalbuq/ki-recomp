#!/bin/bash
# Test10: 200 Hz CPU-time sampling of native AArch64 instruction addresses,
# grouped into C++ function names. Diagnostic only: reduces performance.
# No ROM data, no game state and no sampled addresses leave this device;
# the resulting log stays in killerinstinct/logs/ until you share it.
export KI_R36S_HOST_PROFILE=1
export KI_R36S_LOG_BASENAME=ki-r36s-host-profile
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
