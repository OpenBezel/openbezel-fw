#!/usr/bin/env python3
"""check_banking - fail the build on a call that crosses banks without the
trampoline.

A function linked into bank N (address N << 16 | window) can only be reached
through __sdcc_banked_call, i.e. via a __banked declaration.  A missing one
compiles to a plain lcall/ljmp of the low 16 bits, which jumps into whatever
the CURRENTLY mapped bank holds there - silently.  So: for every module's .asm,
any direct lcall/ljmp to a symbol in another bank, and any `#_symbol` of a
banked function (its address - a function pointer, which would also be a
16-bit jump), is an error.  Calls into the common region are always fine.

Usage: check_banking.py build/<board>/openbezel.map build/<board>/*.asm
"""
import re
import sys


XRAM_END = 0xE000       # linker data region 0xD700-0xDFFF; 0xE000 = flash program buffer


def check_xram(mp):
    """sdld runs without its memory summary (banked code), so nothing stopped XDATA
    growing past 0xDFFF into the flash program buffer - where every settings save
    would overwrite live variables.  Check the linked segments here."""
    errors = []
    for line in open(mp):
        m = re.match(r"(XSEG|XISEG|PSEG)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
        if m:
            start, size = int(m.group(2), 16), int(m.group(3), 16)
            if size and start + size > XRAM_END:
                errors.append(f"{m.group(1)} 0x{start:04X}+0x{size:X} ends at 0x{start + size:04X} > 0x{XRAM_END:04X}: "
                              "XDATA overflows into the flash program buffer")
    return errors


def main():
    mp, asms = sys.argv[1], sys.argv[2:]
    bank_of = {}
    for line in open(mp):
        m = re.match(r"\s*C:\s+([0-9A-Fa-f]{8})\s+(\S+)", line)
        if m:
            bank_of.setdefault(m.group(2), int(m.group(1), 16) >> 16)
    errors = []
    for path in asms:
        area_bank = 0
        for n, line in enumerate(open(path), 1):
            m = re.match(r"\s*\.area\s+(\w+)", line)
            if m:
                bm = re.match(r"BANK(\d+)$", m.group(1))
                area_bank = int(bm.group(1)) if bm else 0
                continue
            m = re.match(r"\s*(lcall|ljmp|acall|ajmp)\s+(_\w+)", line)
            if m:
                b = bank_of.get(m.group(2), 0)
                if b and b != area_bank:
                    errors.append(f"{path}:{n}: {m.group(1)} {m.group(2)} (bank {b}) from bank {area_bank} "
                                  "- declare it __banked")
                continue
            for sym in re.findall(r"#\(?(_\w+)", line):
                if bank_of.get(sym, 0) and not sym.endswith(("_PARM_2", "_PARM_3", "_PARM_4")):
                    # a banked function's address: only SDCC's own banked-call
                    # sequence (r0/r1/r2 = target) may take it
                    if not re.match(r"\s*mov\s+r[012],", line):
                        errors.append(f"{path}:{n}: address of banked {sym} taken - no function pointers into banks")
    errors += check_xram(mp)
    if errors:
        sys.exit("check_banking:\n  " + "\n  ".join(errors))
    print(f"  banking OK: {sum(1 for b in bank_of.values() if b)} banked symbols, {len(asms)} modules checked")


if __name__ == "__main__":
    main()
