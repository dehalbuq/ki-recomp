"""Conservative, ROM-independent recognizer for MIPS COP0 Cause idle loops.

Only matches a *complete* five-instruction loop with no user-visible memory
or I/O side effects beyond COP0 Cause polling and an optional 32-bit counter.
The source game image stays private; this checker only sees five integers.
"""
from dataclasses import dataclass
from typing import Sequence

@dataclass(frozen=True)
class IrqIdleLoop:
    reg: int
    mask: int
    counter_reg: int = 0
    counter_imm: int = 0
    cycles: int = 6  # KI recompiler: mfc0 Cause = 2 cycles, other 4 opcodes = 1

def match_irq_idle(words: Sequence[int]) -> IrqIdleLoop | None:
    if len(words) != 5:
        return None
    mfc0, nop, andi, branch, delay = words
    reg = (mfc0 >> 16) & 31
    # MFC0 reg,Cause,sel0 (Coprocessor-0, rs=0, rd=13)
    if (mfc0 >> 26) != 0x10 or ((mfc0 >> 21) & 31) != 0:
        return None
    if ((mfc0 >> 11) & 31) != 13 or (mfc0 & 0x7ff) != 0 or reg == 0:
        return None
    if nop != 0:
        return None
    if (andi >> 26) != 0x0c or ((andi >> 21) & 31) != reg or ((andi >> 16) & 31) != reg:
        return None
    mask = andi & 0xffff
    # Only board IRQs IP2-IP6 and timer IP7, not software IP0/IP1,
    # exception state, or any bits that may change outside scheduler.
    if mask == 0 or mask & ~0xfc00:
        return None
    op = branch >> 26
    if op not in (0x04, 0x05):  # beq or bne only
        return None
    rs, rt = (branch >> 21) & 31, (branch >> 16) & 31
    if not ((rs == reg and rt == 0) or (rs == 0 and rt == reg)):
        return None
    if (branch & 0xffff) != 0xfffc:  # PC+4 - 4*4 = first instruction
        return None
    counter_reg, counter_imm = 0, 0
    if delay != 0:
        if (delay >> 26) != 0x09:  # addiu rY,rY,simm
            return None
        counter_reg = (delay >> 16) & 31
        if counter_reg in (0, reg) or ((delay >> 21) & 31) != counter_reg:
            return None
        immediate = delay & 0xffff
        counter_imm = immediate - 65536 if immediate & 0x8000 else immediate
    return IrqIdleLoop(reg=reg, mask=mask, counter_reg=counter_reg, counter_imm=counter_imm)


if __name__ == "__main__":
    def mfc0(rt):
        return (0x10 << 26) | (rt << 16) | (13 << 11)
    def andi(rt, mask):
        return (0x0c << 26) | (rt << 21) | (rt << 16) | mask
    def beq(rt):
        return (0x04 << 26) | (rt << 21) | 0xfffc
    def bne(rt):
        return (0x05 << 26) | (rt << 21) | 0xfffc
    def addiu(r,imm):
        return (0x09 << 26) | (r << 21) | (r << 16) | (imm & 0xffff)
    base = [mfc0(8), 0, andi(8, 0x400), beq(8), 0]
    assert match_irq_idle(base) == IrqIdleLoop(8, 0x400)
    assert match_irq_idle(base[:3] + [bne(8), addiu(9, -1)]) == IrqIdleLoop(8, 0x400, 9, -1)
    for index, value in [(0, 0), (1, 1), (2, andi(8, 0x8)), (3, beq(9)), (3, 0),
                         (4, addiu(8, 2)), (4, 0xffffffff)]:
        test = list(base)
        test[index] = value
        if index == 0 and value == 0:
            assert match_irq_idle(test) is None
        elif index != 4 or value != 0:
            assert match_irq_idle(test) is None, (index, hex(value))
    print("MIPS five-instruction IRQ idle pattern: strict matches/rejections passed")
