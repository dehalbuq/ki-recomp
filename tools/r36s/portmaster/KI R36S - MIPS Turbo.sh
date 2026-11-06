#!/bin/bash
# Test14: experimental aggressive event-driven MIPS IRQ idle acceleration.
# No sound disable, no frame skip, no 25 FPS cap; retains original guest clock.
# The exact 5-instruction pattern is required; all other MIPS code unchanged.
export KI_R36S_MIPS_IDLE=1
export KI_R36S_LOG_BASENAME=ki-r36s-mips-turbo
exec "$(dirname "$0")/Killer Instinct Arcade.sh" "$@"
