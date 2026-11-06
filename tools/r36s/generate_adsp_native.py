#!/usr/bin/env python3
"""Generate a PRIVATE build-only C++ ADSP-2105 opcode-specialization table.

Inspired by UltraCombo's DSP-to-C architecture, but adapted independently
to the MAME-derived ki-recomp ADSP state machine. No copyrighted instructions
are committed to the source repository. Output is generated ONLY in build dir.

The generator emits guarded C++ call sites. At runtime, a specialization is
used only if the current program-memory 24-bit word still matches; writes to
DSP program RAM otherwise fall back to the unmodified interpreter.
"""
import argparse
import hashlib
from pathlib import Path

# L1.5D KI1 DCS boot EPROM u10-l1 from a user-supplied, validated MAME set.
KI_U10_SHA1 = "810d455df8f385d76143e9d7d048f2b555ff8bf0"

# Synthetic, non-game instructions for ROM-free differential testing.
# 0x0400..0x04ff is unmapped on the real KI DCS board.
FIXTURE = {
    0x400: 0x40012A,  # group 0x40, immediate
    0x401: 0x0D02A3,  # internal move
    0x402: 0x26E20F,  # ALU to AF
    0x403: 0x000000,  # NOP (interpreter fallback)
    0x404: 0x340001,  # register group 1 immediate
    0x405: 0x440016,  # register group 0 immediate
    0x406: 0x22000F,  # ALU to AR
    0x407: 0x24000F,  # MAC to MF
    0x408: 0x61000F,  # MAC to MR + DM DAG1 read
    0x409: 0x80005A,  # DM read immediate
    0x40A: 0x90005A,  # DM write immediate
    0x40B: 0x1840CF,  # jump immediate (always)
    0x40C: 0x300001,  # load non-data register immediate
    0x40D: 0x000000,  # NOP
}

def supported(op: int) -> bool:
    group = op >> 16
    return (group == 0x0D
            or 0x18 <= group <= 0x1B
            or 0x20 <= group <= 0x27
            or 0x30 <= group <= 0x4F
            or 0x60 <= group <= 0x7F
            or 0x80 <= group <= 0x9F)

def extract_boot(rom: bytes) -> dict[int, int]:
    if hashlib.sha1(rom).hexdigest() != KI_U10_SHA1:
        raise ValueError("Unexpected u10-l1 ROM checksum. KI1 L1.5D required.")
    words = min((rom[3] + 1) * 8, 0x400)
    if len(rom) < words * 4:
        raise ValueError("Truncated DSP boot ROM")
    image = {}
    for pc in range(words):
        off = pc * 4
        opcode = (rom[off] << 16) | (rom[off+1] << 8) | rom[off+2]
        # The prototype targets internal program memory, avoiding all
        # unmapped mirrored addresses and the game boot sequence.
        if 0x100 <= pc < 0x200 and supported(opcode):
            image[pc] = opcode
    return image

def generate(rom_file: Path | None) -> tuple[str, int]:
    image = dict(FIXTURE)
    game_count = 0
    if rom_file and rom_file.is_file():
        image.update(extract_boot(rom_file.read_bytes()))
        game_count = len(image) - len(FIXTURE)
    lines = [
        "// GENERATED BUILD OUTPUT. DO NOT COMMIT: may contain game-derived opcodes.",
        "// Per-PC runtime opcode guards protect DSP program-RAM modifications.",
        f"#define KI_ADSP_NATIVE_GAME_ENTRIES {game_count}",
    ]
    for pc, opcode in sorted(image.items()):
        if not supported(opcode):
            continue
        lines.extend([
            f"case 0x{pc:04x}u:",
            f"    if (op == 0x{opcode:06x}u) {{",
            f"        execute_native_opcode<0x{opcode:06x}u>();",
            "        ++m_native_opcode_count;",
            "        return true;",
            "    }",
            "    break;",
        ])
    return "\n".join(lines) + "\n", game_count

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, help="Private, legitimately supplied u10-l1 sound ROM")
    p.add_argument("--out", required=True, type=Path)
    a = p.parse_args()
    source, game_count = generate(a.rom)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(source, encoding="utf-8")
    print(f"Generated ADSP native opcode guards: {game_count} KI1 boot instructions + ROM-free fixture; mode OFF by default")
    if a.rom and not a.rom.is_file():
        print("No private audio ROM in this build: using synthetic fixture only.")

if __name__ == "__main__":
    main()
