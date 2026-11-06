#!/bin/bash
# Test15: same full sound + MIPS idle + fast guest memory as the normal game,
# but the guest MIPS C++ translation was compiled with -O3 (vs -O2).
# This only selects the alternate ARM64 executable included in the test ZIP.
export KI_R36S_BINARY=ki_play_o3.aarch64
export KI_R36S_LOG_BASENAME=ki-r36s-compiler-o3
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
