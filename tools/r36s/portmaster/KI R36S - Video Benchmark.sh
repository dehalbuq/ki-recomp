#!/bin/bash
# Diagnostic ONLY: render every 4th guest frame; logic still runs every frame.
# Do not use for normal gameplay (the image will stutter intentionally).
export KI_R36S_BENCH_RENDER_QUARTER=1
export KI_R36S_LOG_BASENAME="ki-r36s-bench-video"
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
