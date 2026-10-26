#!/usr/bin/env python3
"""mkimage - turn SDCC's .ihx output into a flashable RTD image.

The boot ROM executes flash offset 0 raw - no header, no checksum - so all
this does is place the Intel-hex records into a 0xFF-filled image, sanity-
check the layout, stamp a build-info block, and pad to a 64 KB boundary:

  - reset vector at 0 must be LJMP (0x02)
  - nothing may land in 0xFF00-0xFFEF (reserved: bank-migration + info block)
  - info block at 0xFF00: 'OBFW' + 12-byte version/git string, NUL padded
  - with --map, the recovery path stays inside the first code page

Usage: mkimage.py in.ihx out.bin [--version STRING]
"""

import argparse
import re
import sys

INFO_BLOCK_ADDR = 0xFF00

# The recovery path: everything needed to get DDC up, serve the EDID, assert
# HPD and sit in the ISP break-in window.  If any of this ever moves out of the
# first code page, a host can no longer reflash a wedged board over DDC and the
# only way back is a SPI clip.  Checked here so that a future
# move to banked code cannot relocate it silently.
SAFE_BOOT_SYMBOLS = [
    "_main",
    "_board_early_init",
    "_rtd_ddc_external_enable",
    "_rtd_serve_edid",
    "_board_gpio_init",
    "_delay_ms",
    "_board_edid",          # must stay __code: serving it IS the recovery path
    # Not recovery, but must be in the COMMON region (replicated in every
    # bank): the INT0 vector lands here whatever bank is mapped, and the
    # trampolines run across the switch.
    "_dp_aux_isr",
    "__sdcc_banked_call",
    "__sdcc_banked_ret",
]
INFO_MAGIC = b"OBFW"
BANK_SIZE = 0x10000


def parse_map(path):
    """-> {symbol: address} for code-space globals in an SDCC/ASxxxx .map."""
    syms = {}
    for line in open(path):
        m = re.match(r"\s*C:\s+([0-9A-Fa-f]{8})\s+(\S+)", line)
        if m:
            syms.setdefault(m.group(2), int(m.group(1), 16))
    return syms


def check_safe_boot(map_path, limit):
    syms = parse_map(map_path)
    missing = [s for s in SAFE_BOOT_SYMBOLS if s not in syms]
    if missing:
        sys.exit("mkimage: recovery-path symbols not found in %s: %s\n"
                 "         (renamed?  update SAFE_BOOT_SYMBOLS deliberately, not reflexively)"
                 % (map_path, ", ".join(missing)))
    bad = [(s, syms[s]) for s in SAFE_BOOT_SYMBOLS if syms[s] >= limit]
    if bad:
        sys.exit("mkimage: recovery path left the first code page (limit 0x%X):\n%s\n"
                 "         A board that wedges after this can only be recovered with a SPI clip."
                 % (limit, "\n".join("           %-28s 0x%05X" % (s, a) for s, a in bad)))
    print("  recovery path OK: %d symbols, highest 0x%05X < 0x%X"
          % (len(SAFE_BOOT_SYMBOLS), max(syms[s] for s in SAFE_BOOT_SYMBOLS), limit))


def parse_ihx(path):
    """-> {addr: byte} from an Intel HEX file (I8HEX/I16HEX subset)."""
    mem = {}
    base = 0
    for lineno, line in enumerate(open(path), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"{path}:{lineno}: not an ihx record")
        rec = bytes.fromhex(line[1:])
        count, addr, rtype = rec[0], (rec[1] << 8) | rec[2], rec[3]
        data, cksum = rec[4:4 + count], rec[4 + count]
        if (sum(rec[:4 + count]) + cksum) & 0xFF:
            raise ValueError(f"{path}:{lineno}: bad checksum")
        if rtype == 0x00:
            for i, b in enumerate(data):
                a = base + addr + i
                if a in mem:
                    raise ValueError(f"overlapping data at 0x{a:04X}")
                mem[a] = b
        elif rtype == 0x01:
            break
        elif rtype == 0x04:  # extended linear address
            base = ((data[0] << 8) | data[1]) << 16
        elif rtype == 0x02:  # extended segment address
            base = ((data[0] << 8) | data[1]) << 4
        else:
            raise ValueError(f"{path}:{lineno}: record type {rtype}")
    return mem


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("ihx")
    p.add_argument("out")
    p.add_argument("--version", default="dev")
    p.add_argument("--version-file", help="read the version from this file (meson vcs_tag)")
    p.add_argument("--map", help="SDCC .map; checks the recovery path stays in page 0")
    p.add_argument("--split", type=lambda v: int(v, 0), default=BANK_SIZE,
                   help="end of the COMMON region; banked code above it")
    p.add_argument("--bank", action="append", default=[], metavar="N:FILE",
                   help="write bank N's image (flash N * 64 KB) to FILE; repeatable")
    args = p.parse_args()
    if args.version_file:
        args.version = open(args.version_file).read().strip() or "dev"
    if args.map:
        check_safe_boot(args.map, args.split)

    allmem = parse_ihx(args.ihx)
    if not allmem:
        sys.exit("empty image")
    mem = {a: b for a, b in allmem.items() if a < BANK_SIZE}
    outs = {}
    for spec in args.bank:
        n, f = spec.split(":", 1)
        outs[int(n, 0)] = f
    if 1 in outs or 2 in outs:
        sys.exit("banks 1 (HDCP keys) and 2 (assets) are not code banks")
    banks = {n: {} for n in outs}
    for a, b in allmem.items():
        n = a >> 16
        if n == 0:
            continue
        if n not in banks:
            sys.exit(f"code at 0x{a:05X} in bank {n}, which has no --bank output")
        banks[n][a & 0xFFFF] = b
    top = max(mem)
    over = [a for a in mem if args.split <= a < INFO_BLOCK_ADDR]
    if over:
        sys.exit(f"common code reaches 0x{max(over):04X}, past the split 0x{args.split:04X}")
    for n, bm in banks.items():
        out = [a for a in bm if not args.split <= a < INFO_BLOCK_ADDR]
        if out:
            sys.exit(f"bank-{n} code at 0x{min(out):04X}, outside its window "
                     f"0x{args.split:04X}-0x{INFO_BLOCK_ADDR - 1:04X}")
    if mem.get(0) != 0x02:
        sys.exit(f"flash offset 0 is 0x{mem.get(0, 0xFF):02X}, expected LJMP "
                 "(0x02) - the boot ROM executes offset 0 raw")
    bad = [a for a in mem if 0xFF00 <= a <= 0xFFEF]
    if bad:
        sys.exit(f"code/data in reserved 0xFF00-0xFFEF: 0x{min(bad):04X}... "
                 "(check --code-size)")

    img = bytearray(b"\xFF" * BANK_SIZE)
    for a, b in mem.items():
        img[a] = b
    # 0xFF00: 'OBFW' + version.  0xFF10: build ID = CRC-32 of the common
    # region, then 0xFF14: mask of the code banks this build has (bit n).
    # Every bank image carries both; the firmware compares each bank's ID with
    # bank 0's (read through the flash controller) before its first banked
    # call, so a half-flashed or blank bank can never be jumped into.
    import zlib
    build_id = zlib.crc32(bytes(img[:args.split])) & 0xFFFFFFFF
    mask = 0
    for n in outs:
        mask |= 1 << n
    info = (INFO_MAGIC + args.version.encode()[:11] + b"\x00" * 12)[:16]
    info += build_id.to_bytes(4, "little") + mask.to_bytes(2, "little")
    img[INFO_BLOCK_ADDR:INFO_BLOCK_ADDR + len(info)] = info

    with open(args.out, "wb") as f:
        f.write(img)
    used = len(mem)
    print(f"{args.out}: common {used} bytes of 0x{args.split:04X} "
          f"({100 * used // args.split}%), top 0x{top:04X}, version '{args.version}', "
          f"build id {build_id:08X}")
    win = INFO_BLOCK_ADDR - args.split
    for n, bm in sorted(banks.items()):
        # every bank = the same COMMON region + its own window: CODE_PAGE maps a
        # whole 64 KB page, so whatever runs across a switch must be at the
        # same address in all of them
        imgn = bytearray(b"\xFF" * BANK_SIZE)
        imgn[:args.split] = img[:args.split]
        for a, b in bm.items():
            imgn[a] = b
        imgn[INFO_BLOCK_ADDR:INFO_BLOCK_ADDR + len(info)] = info
        with open(outs[n], "wb") as f:
            f.write(imgn)
        print(f"{outs[n]}: bank {n} window {len(bm)} bytes of {win} ({100 * len(bm) // win}%)")

if __name__ == "__main__":
    main()
