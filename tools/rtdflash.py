#!/usr/bin/env python3
"""rtdflash - flash an RTD26xx/25xx scaler over DDC/I2C ISP.

Entering ISP halts the 8051 (the display freezes); finishing an operation
exits ISP by default, which resets the chip into whatever is now in flash.
Use --stay to remain halted in ISP mode.

Examples:
  rtdflash.py --bus 9 id
  rtdflash.py --bus 9 dump backup.bin
  rtdflash.py --bus 9 flash build/intouch156b/openbezel.bin --sync
  rtdflash.py --bus 9 verify backup.bin
  rtdflash.py --bus 9 probe-erase            # one-time sector-erase validation
  rtdflash.py --bus 9 --race id              # ISP entry race for recovery
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rtdlib import Bus, XFR, ISP, Scaler, crc8
from rtdlib.crc8 import Crc8
from rtdlib.isp import SECTOR_SIZE, IspError
from rtdlib import flashdb

# The flash's WP# pin is driven by a scaler GPIO on known boards; SRP0 in
# the status register hardware-locks WRSR unless it is high.  Find the pin
# for a new board with tools/experiments/wp_hunt.py.
WP_GPIO_DEFAULT = 0xFE19  # intouch156b: P7.1


def progress(done, total):
    pct = 100 * done // total
    sys.stderr.write(f"\r  {done}/{total} bytes ({pct}%)")
    if done >= total:
        sys.stderr.write("\n")
    sys.stderr.flush()


def enter_isp(xfr, race=False):
    isp = ISP(xfr)
    if race:
        print("Racing ISP entry - power-cycle the display now...")
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            try:
                xfr.enable_isp()
                if xfr.in_isp() and isp.jedec_id() not in (0x000000, 0xFFFFFF):
                    print("ISP entered.")
                    return isp
            except OSError:
                pass
        raise IspError("race entry failed after 60s")
    isp.enter()
    return isp


def identify(isp):
    chip = isp.identify()
    print(f"Flash: {chip.name} ({flashdb.manufacturer_name(chip.jedec_id)}), "
          f"{chip.size_kb} KB, JEDEC 0x{chip.jedec_id:06X}")
    print(f"Status register: 0x{isp.read_status():02X}")
    size = isp.autoprobe_transfer_sizes()
    print(f"FIFO read chunk: {size} bytes")
    return chip


def sector_crcs(isp, length, image=None, base=0):
    """Per-4K-sector (hw_crcs, image_crcs) list over flash [base, base + length);
    the returned offsets are relative to base.

    Each sector is fingerprinted by TWO CRCs (one per half) because a
    single CRC-8 has a very real 1/256 collision rate: it can silently
    skip a changed sector."""
    out = []
    for start in range(0, length, SECTOR_SIZE):
        end = min(start + SECTOR_SIZE, length)
        mid = start + (end - start) // 2
        hw = (isp.hw_crc(base + start, base + mid - 1),
              isp.hw_crc(base + mid, base + end - 1))
        img = ((crc8(image[start:mid]), crc8(image[mid:end]))
               if image is not None else None)
        out.append((start, hw, img))
    return out


def cmd_id(isp, args):
    identify(isp)


def cmd_dump(isp, args):
    chip = identify(isp)
    length = args.length or chip.size_kb * 1024
    print(f"Reading {length} bytes from 0x{args.addr:06X}...")
    data = isp.read(args.addr, length, progress=progress)
    with open(args.file, "wb") as f:
        f.write(data)
    host = crc8(data)
    hw = isp.hw_crc(args.addr, args.addr + length - 1)
    print(f"Host CRC 0x{host:02X}, chip CRC 0x{hw:02X}: "
          f"{'OK' if host == hw else 'MISMATCH'}")
    if host != hw:
        sys.exit(1)


def cmd_verify(isp, args):
    chip = identify(isp)
    image = open(args.file, "rb").read()
    if args.length:
        image = image[:args.length]
    # Byte-exact readback: even two CRC-8s per sector can both collide on
    # corrupt bytes, so CRCs are only a hint here; the readback is the verdict.
    bad = []
    n = len(image)
    for s in range(0, n, SECTOR_SIZE):
        chunk = isp.read(s, min(SECTOR_SIZE, n - s))
        if chunk != image[s:s + len(chunk)]:
            ndiff = sum(1 for a, b in zip(chunk, image[s:s + len(chunk)]) if a != b)
            bad.append(f"0x{s:06X} ({ndiff} bytes)")
        progress(min(s + SECTOR_SIZE, n), n)
    if bad:
        print(f"MISMATCHED sectors (byte-exact): {', '.join(bad)}")
        sys.exit(1)
    print(f"Verify OK, byte-exact ({len(image)} bytes, {len(image)//SECTOR_SIZE} sectors)")


# Board-unique data that no image may overwrite: on intouch156b the HDCP 1.4
# device keys (KSV at 0x01306F, then the private keys) live in sector
# 0x013000.  A sector whose content already matches
# (a stock restore with --sync) is never rewritten, so this only stops writes
# that would CHANGE it.
PROTECTED_SECTORS = {0x013000: "HDCP device keys (board-unique, licensed)"}


def cmd_flash(isp, args):
    """Several images, one ISP session: a banked build's images
    must never boot half-written - each image is FILE or FILE@ADDR."""
    for spec in args.file:
        path, _, at = spec.partition("@")
        sub = argparse.Namespace(**vars(args))
        sub.file = path
        sub.addr = int(at, 0) if at else args.addr
        print(f"== {path} @ 0x{sub.addr:06X}")
        flash_one(isp, sub)


def flash_one(isp, args):
    chip = identify(isp)
    image = open(args.file, "rb").read()
    if chip.sector_erase_op is None:
        raise IspError(f"{chip.name} has no 4K sector erase - flashing this "
                       "chip needs a block/chip-erase flow (not implemented; "
                       "all target boards so far use 4K-capable chips)")
    if args.addr % SECTOR_SIZE:
        raise IspError("--addr must be 4K-aligned")
    if args.addr + len(image) > chip.size_kb * 1024:
        raise IspError("image does not fit in flash")
    # pad to sector boundary so erase/verify cover whole sectors
    if len(image) % SECTOR_SIZE:
        image += b"\xFF" * (SECTOR_SIZE - len(image) % SECTOR_SIZE)

    if args.sync:
        print("Comparing per-sector CRCs...")
        dirty = [(s, img) for s, hw, img in sector_crcs(isp, len(image), image, args.addr)
                 if hw != img]
        # The hardware CRCs are taken at --addr: compared against bank 0, a bank
        # image would skip its common region and keep a stale copy of it.
        print(f"{len(dirty)}/{len(image)//SECTOR_SIZE} sectors differ")
    else:
        dirty = [(s, (crc8(image[s:s + SECTOR_SIZE // 2]),
                      crc8(image[s + SECTOR_SIZE // 2:s + SECTOR_SIZE])))
                 for s in range(0, len(image), SECTOR_SIZE)]

    for s0, _ in dirty:
        a = args.addr + s0
        if a in PROTECTED_SECTORS and not args.allow_key_sector:
            raise IspError(f"refusing to rewrite sector 0x{a:06X}: {PROTECTED_SECTORS[a]} "
                           "(--allow-key-sector to override)")
    if not dirty:
        print("Nothing to do.")
        return

    t0 = time.monotonic()
    isp.unprotect()
    try:
        for i, (s, _) in enumerate(dirty):
            sys.stderr.write(f"\r  sector 0x{args.addr + s:06X} "
                             f"({i + 1}/{len(dirty)})")
            sys.stderr.flush()
            isp.sector_erase(args.addr + s)
            isp.program(args.addr + s, image[s:s + SECTOR_SIZE])
        sys.stderr.write("\n")
    finally:
        isp.protect()

    bad = []
    half = SECTOR_SIZE // 2
    for s, img_crc in dirty:
        a = args.addr + s
        hw = (isp.hw_crc(a, a + half - 1), isp.hw_crc(a + half, a + SECTOR_SIZE - 1))
        if hw != img_crc:
            bad.append(f"0x{a:06X}")
    # CRC-8 is not trustworthy on its own (see cmd_verify): read every
    # written sector back and compare byte-exact.
    for s, _ in dirty:
        a = args.addr + s
        rb = isp.read(a, SECTOR_SIZE)
        if rb != image[s:s + SECTOR_SIZE]:
            ndiff = sum(1 for x, y in zip(rb, image[s:s + SECTOR_SIZE]) if x != y)
            tag = f"0x{a:06X} ({ndiff} bytes byte-exact)"
            if tag not in bad:
                bad.append(tag)
    dt = time.monotonic() - t0
    if bad:
        print(f"VERIFY FAILED for sectors: {', '.join(bad)}")
        sys.exit(1)
    print(f"Flashed and verified (byte-exact) {len(dirty)} sectors in {dt:.1f}s")


def cmd_erase(isp, args):
    chip = identify(isp)
    isp.unprotect()
    try:
        if args.sector is not None:
            isp.sector_erase(args.sector)
            print(f"Sector 0x{args.sector:06X} erased")
        elif args.block is not None:
            isp.block_erase(args.block)
            print(f"Block 0x{args.block:06X} erased")
    finally:
        isp.protect()


# GD25Q16 status register 1: SRP0 + BP4..0 = 01101 = "lower 1/2 protected"
# (0x000000-0x0FFFFF: code, assets, stock's NVRAM at 0x9B000), leaving the
# upper 1 MB writable by the FIRMWARE for its settings log - so a firmware
# flash-write bug can never reach code.  SRP0 keeps the register itself
# hardware-locked while WP# is low (it is, except inside this tool).
SR_PROTECT_LOWER_HALF = 0xB4


def cmd_protect(isp, args):
    """Set the flash's at-rest write protection (status register 1).  Later
    `flash` runs capture and restore whatever this leaves."""
    identify(isp)
    before = isp.read_status()
    value = args.sr if args.sr is not None else SR_PROTECT_LOWER_HALF
    isp._wp_drive(1)
    isp.protect(value)                  # WRSR, then WP# back low
    after = isp.read_status()
    print(f"Status register: 0x{before:02X} -> 0x{after:02X}")
    if (after & ~0x03) != (value & ~0x03):
        raise IspError(f"status register reads 0x{after:02X}, wanted 0x{value:02X}")


def cmd_probe_erase(isp, args):
    """One-time validation that 4K sector erase works via the generic command
    engine (it was never used by the stock tooling).  Uses a scratch sector
    that must be blank, programs a pattern, erases, and checks neighbors."""
    chip = identify(isp)
    addr = args.addr
    if addr % SECTOR_SIZE:
        raise IspError("probe address must be 4K-aligned")
    lo = max(addr - 16, 0)
    before = isp.read(lo, 16 + SECTOR_SIZE + 16 if addr else SECTOR_SIZE + 16)
    sector0 = 16 if addr else 0
    if set(before[sector0:sector0 + SECTOR_SIZE]) != {0xFF}:
        raise IspError(f"scratch sector 0x{addr:06X} is not blank - pick "
                       "another with --addr (default 0x100000 is unused in "
                       "the stock intouch156b image)")
    print(f"Programming test pattern into 0x{addr:06X}...")
    pattern = bytes(range(256)) * (SECTOR_SIZE // 256)
    isp.unprotect()
    try:
        isp.program(addr, pattern)
        if isp.read(addr, SECTOR_SIZE) != pattern:
            raise IspError("test pattern readback failed - program path broken")
        print("Pattern verified. Erasing sector 0x20...")
        isp.sector_erase(addr)
    finally:
        isp.protect()
    after = isp.read(lo, len(before))
    sector_after = after[sector0:sector0 + SECTOR_SIZE]
    neighbors_ok = (after[:sector0] == before[:sector0] and
                    after[sector0 + SECTOR_SIZE:] == before[sector0 + SECTOR_SIZE:])
    if set(sector_after) == {0xFF} and neighbors_ok:
        print("PASS: 4K sector erase works and neighbors are untouched.")
    else:
        print("FAIL: sector erase did not behave "
              f"(sector blank: {set(sector_after) == {0xFF}}, "
              f"neighbors intact: {neighbors_ok})")
        sys.exit(1)


def cmd_crcmap(isp, args):
    chip = identify(isp)
    length = args.length or chip.size_kb * 1024
    for s, hw, _ in sector_crcs(isp, length):
        print(f"0x{s:06X}: 0x{hw[0]:02X} 0x{hw[1]:02X}")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--bus", type=int,
                   default=int(os.environ.get("RTD_I2C_BUS", -1)),
                   help="i2c bus number (or RTD_I2C_BUS env var)")
    p.add_argument("--race", action="store_true",
                   help="race ISP entry across a power cycle (recovery)")
    p.add_argument("--stay", action="store_true",
                   help="stay in ISP mode afterwards (8051 stays halted)")
    p.add_argument("--wp-gpio", type=lambda x: int(x, 0),
                   default=WP_GPIO_DEFAULT, metavar="SREG",
                   help="scaler GPIO reg driving flash WP# "
                        f"(default 0x{WP_GPIO_DEFAULT:04X}; 0 disables)")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("id", help="identify the flash chip")
    d = sub.add_parser("dump", help="read flash to a file")
    d.add_argument("file")
    d.add_argument("--addr", type=lambda x: int(x, 0), default=0)
    d.add_argument("--length", type=lambda x: int(x, 0))
    v = sub.add_parser("verify", help="compare flash against an image file")
    v.add_argument("file")
    v.add_argument("--length", type=lambda x: int(x, 0))
    f = sub.add_parser("flash", help="write images (sector erase + program), one ISP session")
    f.add_argument("file", nargs="+", help="FILE or FILE@ADDR (FILE alone: --addr)")
    f.add_argument("--addr", type=lambda x: int(x, 0), default=0)
    f.add_argument("--sync", action="store_true",
                   help="only rewrite sectors whose CRC differs")
    f.add_argument("--allow-key-sector", action="store_true",
                   help="allow rewriting the board's HDCP key sector (0x013000)")
    e = sub.add_parser("erase", help="erase one sector/block (explicit)")
    e.add_argument("--sector", type=lambda x: int(x, 0))
    e.add_argument("--block", type=lambda x: int(x, 0))
    pe = sub.add_parser("probe-erase", help="validate 4K sector erase once")
    pe.add_argument("--addr", type=lambda x: int(x, 0), default=0x100000)
    pr = sub.add_parser("protect", help="set at-rest write protection "
                        "(default 0xB4: lower 1 MB protected, upper writable)")
    pr.add_argument("--sr", type=lambda x: int(x, 0),
                    help="status register value (0xFC = all protected)")
    c = sub.add_parser("crcmap", help="print per-sector hardware CRCs")
    c.add_argument("--length", type=lambda x: int(x, 0))

    args = p.parse_args()
    if args.bus < 0:
        p.error("--bus is required (or set RTD_I2C_BUS)")

    bus = Bus(args.bus)
    xfr = XFR(bus)
    isp = enter_isp(xfr, race=args.race)
    if args.wp_gpio:
        isp.set_wp_control(Scaler(xfr), args.wp_gpio)
    try:
        {"id": cmd_id, "dump": cmd_dump, "verify": cmd_verify,
         "flash": cmd_flash, "erase": cmd_erase,
         "probe-erase": cmd_probe_erase, "protect": cmd_protect,
         "crcmap": cmd_crcmap}[args.cmd](isp, args)
    finally:
        if args.stay:
            print("Staying in ISP mode (8051 halted). "
                  "Write reg 0x6F=0 to reboot.")
        else:
            isp.reboot()
            print("Left ISP mode - chip reset, booting flash image.")


if __name__ == "__main__":
    main()
