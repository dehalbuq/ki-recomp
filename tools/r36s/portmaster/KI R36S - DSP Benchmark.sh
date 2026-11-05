#!/bin/bash
# Diagnostic ONLY: do not emulate the DCS sound processor.
# Audio will be silent and gameplay may not boot normally.
export KI_R36S_BENCH_NO_DCS=1
export KI_R36S_LOG_BASENAME="ki-r36s-bench-no-dcs"
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
