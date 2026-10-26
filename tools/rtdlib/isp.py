"""SPI-flash controller behind the XFR (DDC 0x4A regs 0x60-0x76).

Protocol per pyrtd2660i2c/RTD2556-MAP.txt and the proven read path of the
ghent360 RTD-2660 programmer lineage, with the write-path bugs of the old
port fixed: real ready-wait polls with timeouts, flash-WIP polling after
erase, block FIFO transfers, and sector/block erase support.

Entering ISP halts the 8051 and disables the watchdog; leaving ISP resets
the chip (confirmed on hardware).  All methods assume ISP mode is active
(use ``with ISP(xfr):`` or call enter()/exit_isp() yourself).
"""

import time

from .xfr import XFR
from .crc8 import Crc8
from . import flashdb

# Command types (XFR 0x60 bits [7:5])
CC_NOP = 0
CC_WRITE = 1
CC_READ = 2
CC_WRITE_AFTER_WREN = 3
CC_WRITE_AFTER_EWSR = 4
CC_ERASE = 5

SECTOR_SIZE = 4096
PAGE_SIZE = 256

# Standard SPI NOR opcodes (loaded into the controller's opcode registers;
# identical for Winbond/GigaDevice/Macronix/etc.)
OP_WREN = 0x06
OP_EWSR = 0x50
OP_READ = 0x03
OP_FAST_READ = 0x0B
OP_PAGE_PROGRAM = 0x02
OP_RDSR = 0x05
OP_SECTOR_ERASE = 0x20   # 4 KB
OP_BLOCK_ERASE = 0xD8    # 64 KB
OP_CHIP_ERASE = 0xC7
OP_JEDEC_ID = 0x9F


class IspError(IOError):
    pass


class ISP:
    def __init__(self, xfr: XFR):
        self.xfr = xfr
        self.chip = None
        self.protect_status = None  # captured by identify(), restored by protect()
        self._wp_scaler = None
        self._wp_sreg = None

    def set_wp_control(self, scaler, sreg):
        """Board wires the flash's WP# pin to a scaler GPIO (page:offset
        ``sreg``); unprotect() drives it high so WRSR works (SRP0 hardware
        lock), protect() drops it again.  intouch156b: 0xFE19 (P7.1),
        discovered by tools/experiments/wp_hunt.py."""
        self._wp_scaler = scaler
        self._wp_sreg = sreg

    # -- mode management ----------------------------------------------------

    def __enter__(self):
        self.enter()
        return self

    def __exit__(self, *exc):
        # Deliberately do NOT auto-exit ISP: leaving ISP resets the 8051,
        # which the caller must opt into explicitly (reboot()).
        return False

    def enter(self):
        """Enter ISP mode (halts the 8051)."""
        self.xfr.enable_isp()
        if not self.xfr.in_isp():
            raise IspError("ISP entry failed (0x6F bit7 did not stick)")

    def reboot(self):
        """Leave ISP mode - this RESETS the 8051 and boots the flash image.

        The reset is the 8051's only: the XFR page keeps its values, CODE_PAGE
        (0xFF) included, and the reset vector is fetched through it.  A firmware
        halted with any page other than 0 mapped (a banked call, or - seen
        2026-10-10 - a garbage 0x50) then restarts in the wrong flash page and
        never reaches main().  XFR.disable_isp() maps page 0 first."""
        self.xfr.disable_isp()

    # -- low-level command engine -------------------------------------------

    def _wait(self, reg, mask, want_set, timeout, what):
        deadline = time.monotonic() + timeout
        while True:
            v = self.xfr.read_reg(reg)
            if bool(v & mask) == want_set:
                return v
            if time.monotonic() > deadline:
                raise IspError(f"timeout waiting for {what} "
                               f"(reg 0x{reg:02X}=0x{v:02X})")
            time.sleep(0.0005)

    def common_command(self, cmd_type, opcode, read_len=0, write_len=0,
                       write_value=0, timeout=1.0):
        read_len &= 3
        write_len &= 3
        write_value &= 0xFFFFFF
        ctl = (cmd_type << 5) | (write_len << 3) | (read_len << 1)

        self.xfr.write_reg(XFR.SPI_CMD, ctl)
        self.xfr.write_reg(XFR.SPI_OPCODE, opcode)
        if write_len == 3:
            self.xfr.write_reg(XFR.SPI_WDATA0, (write_value >> 16) & 0xFF)
            self.xfr.write_reg(XFR.SPI_WDATA0 + 1, (write_value >> 8) & 0xFF)
            self.xfr.write_reg(XFR.SPI_WDATA0 + 2, write_value & 0xFF)
        elif write_len == 2:
            self.xfr.write_reg(XFR.SPI_WDATA0, (write_value >> 8) & 0xFF)
            self.xfr.write_reg(XFR.SPI_WDATA0 + 1, write_value & 0xFF)
        elif write_len == 1:
            self.xfr.write_reg(XFR.SPI_WDATA0, write_value & 0xFF)

        self.xfr.write_reg(XFR.SPI_CMD, ctl | 1)  # execute
        self._wait(XFR.SPI_CMD, 0x01, False, timeout, f"cmd 0x{opcode:02X}")

        result = 0
        for i in range(read_len):
            result = (result << 8) | self.xfr.read_reg(XFR.SPI_RDATA0 + i)
        return result

    # -- identification -----------------------------------------------------

    def jedec_id(self):
        return self.common_command(CC_READ, OP_JEDEC_ID, read_len=3)

    def identify(self):
        """Read JEDEC ID, look up the chip, and program the controller's
        per-chip opcode registers.  Returns the FlashChip (or raises)."""
        jid = self.jedec_id()
        chip = flashdb.find_chip(jid)
        if chip is None:
            raise IspError(f"unknown flash JEDEC ID 0x{jid:06X} - refusing "
                           "to operate (add it to flashdb.py if it is real; "
                           "0x000000/0xFFFFFF usually means no ISP comms)")
        self.xfr.write_reg(XFR.SPI_OP_WREN, OP_WREN)
        self.xfr.write_reg(XFR.SPI_OP_WRSR, OP_EWSR)
        self.xfr.write_reg(XFR.SPI_OP_READ, OP_READ)
        self.xfr.write_reg(XFR.SPI_OP_FASTREAD, OP_FAST_READ)
        self.xfr.write_reg(XFR.SPI_OP_PROGRAM, OP_PAGE_PROGRAM)
        self.xfr.write_reg(XFR.SPI_OP_RDSR, OP_RDSR)
        self.chip = chip
        # Remember the at-rest protection bits (mask off WIP/WEL) so
        # protect() restores what the board actually runs with - the
        # intouch156b GD25Q16 sits at 0xFC, not the 0x1C of older tools.
        self.protect_status = self.read_status() & ~0x03
        return chip

    def read_status(self):
        return self.common_command(CC_READ, OP_RDSR, read_len=1)

    def _wait_flash_idle(self, timeout, what):
        deadline = time.monotonic() + timeout
        while self.read_status() & 0x01:  # WIP bit
            if time.monotonic() > deadline:
                raise IspError(f"timeout waiting for flash WIP clear ({what})")
            time.sleep(0.002)

    # -- read ---------------------------------------------------------------

    def read(self, addr, length, progress=None):
        """Read ``length`` bytes starting at 24-bit flash address ``addr``."""
        out = bytearray()
        while len(out) < length:
            # Re-issue the read command per chunk so a transfer hiccup can't
            # silently desync the FIFO position.
            chunk = min(length - len(out), 4096)
            self.xfr.write_reg(XFR.SPI_CMD, 0x46)  # READ, rd len 3 (FIFO mode)
            self.xfr.write_reg(XFR.SPI_OPCODE, OP_READ)
            a = addr + len(out)
            self.xfr.write_reg(XFR.SPI_WDATA0, (a >> 16) & 0xFF)
            self.xfr.write_reg(XFR.SPI_WDATA0 + 1, (a >> 8) & 0xFF)
            self.xfr.write_reg(XFR.SPI_WDATA0 + 2, a & 0xFF)
            self.xfr.write_reg(XFR.SPI_CMD, 0x47)  # execute
            self._wait(XFR.SPI_CMD, 0x01, False, 1.0, "flash read setup")
            out += self.xfr.bus.read_fifo(XFR.SPI_FIFO, chunk)
            if progress:
                progress(len(out), length)
        return bytes(out)

    def autoprobe_transfer_sizes(self, addr=0):
        """Find the largest FIFO read chunk the adapter/DDC master supports.
        Compares against a byte-at-a-time reference read of 256 bytes."""
        bus = self.xfr.bus
        saved = bus.max_read
        bus.max_read = 1
        reference = self.read(addr, 256)
        for size in (256, 128, 64, 32, 16, 8):
            try:
                bus.max_read = size
                if self.read(addr, 256) == reference:
                    return size
            except Exception:
                continue
        bus.max_read = saved if saved == 1 else 1
        return bus.max_read

    # -- CRC engine ---------------------------------------------------------

    def hw_crc(self, start, end):
        """Hardware CRC-8 over flash [start, end] inclusive.  Matches
        rtdlib.crc8.crc8() over the same bytes."""
        self.xfr.write_reg(XFR.SPI_WDATA0, (start >> 16) & 0xFF)
        self.xfr.write_reg(XFR.SPI_WDATA0 + 1, (start >> 8) & 0xFF)
        self.xfr.write_reg(XFR.SPI_WDATA0 + 2, start & 0xFF)
        self.xfr.write_reg(XFR.SPI_CRC_END0, (end >> 16) & 0xFF)
        self.xfr.write_reg(XFR.SPI_CRC_END0 + 1, (end >> 8) & 0xFF)
        self.xfr.write_reg(XFR.SPI_CRC_END0 + 2, end & 0xFF)
        self.xfr.write_reg(XFR.ISP, XFR.ISP_CRC)
        self._wait(XFR.ISP, 0x02, True, 30.0, "hardware CRC")
        return self.xfr.read_reg(XFR.SPI_CRC_RESULT)

    # -- erase / program ----------------------------------------------------

    def _wrsr(self, value):
        self.common_command(CC_WRITE_AFTER_EWSR, 0x01, write_len=1, write_value=value)
        self.common_command(CC_WRITE_AFTER_WREN, 0x01, write_len=1, write_value=value)
        self._wait_flash_idle(1.0, "WRSR")

    def _wp_drive(self, level):
        if self._wp_scaler is not None:
            self._wp_scaler.write(self._wp_sreg, [level])

    def unprotect(self):
        self._wp_drive(1)
        self._wrsr(0x00)
        sr = self.read_status()
        if sr & 0x1C:
            raise IspError(
                f"unprotect failed, SR1 still 0x{sr:02X} - status register "
                "is hardware-locked (SRP0 + WP# low).  Configure the WP# "
                "GPIO with set_wp_control() (intouch156b: sreg 0xFE19; find "
                "it on other boards with tools/experiments/wp_hunt.py)")

    def protect(self, status=None):
        """Restore write protection.  Defaults to the value captured from the
        chip by identify() (falling back to 0x1C if never captured)."""
        if status is None:
            status = self.protect_status if self.protect_status is not None else 0x1C
        self._wrsr(status)
        self._wp_drive(0)

    def sector_erase(self, addr):
        """4 KB sector erase.  UNPROVEN on this controller until the M1
        probe-erase test passes - see rtdflash.py probe-erase."""
        self.common_command(CC_ERASE, OP_SECTOR_ERASE, write_len=3,
                            write_value=addr & ~(SECTOR_SIZE - 1))
        self._wait_flash_idle(2.0, f"sector erase @0x{addr:06X}")

    def block_erase(self, addr):
        """64 KB block erase (opcode 0xD8)."""
        self.common_command(CC_ERASE, OP_BLOCK_ERASE, write_len=3,
                            write_value=addr & ~0xFFFF)
        self._wait_flash_idle(5.0, f"block erase @0x{addr:06X}")

    def chip_erase(self):
        """Full chip erase.  Destroys everything, including board-unique
        data.  Last resort only."""
        self.common_command(CC_ERASE, OP_CHIP_ERASE)
        self._wait_flash_idle(120.0, "chip erase")

    def program_page(self, addr, data):
        """Program one page (<= 256 bytes, must not cross a page boundary).

        Enable buffer writes (0x6F bit 4) while filling the 0x70 SRAM, then trigger
        prog_en (bit 5) and wait for it to auto-clear."""
        assert len(data) <= PAGE_SIZE
        assert (addr & (PAGE_SIZE - 1)) + len(data) <= PAGE_SIZE
        # A transfer hiccup during the FIFO fill must NOT be blind-retried
        # at the byte level (a partial+retried chunk shifts the page buffer;
        # this corrupted 2 bytes of a restored sector on 2026-09-07).
        # Instead: reset the flash controller and redo the whole page.
        for attempt in range(3):
            try:
                self.xfr.write_reg(XFR.SPI_PROG_LEN, len(data) - 1)
                self.xfr.write_reg(XFR.SPI_WDATA0, (addr >> 16) & 0xFF)
                self.xfr.write_reg(XFR.SPI_WDATA0 + 1, (addr >> 8) & 0xFF)
                self.xfr.write_reg(XFR.SPI_WDATA0 + 2, addr & 0xFF)
                self.xfr.write_reg(XFR.ISP, XFR.ISP_BUF_WR)
                self.xfr.bus.write_fifo(XFR.SPI_FIFO, data)
                self.xfr.write_reg(XFR.ISP, XFR.ISP_PROGRAM)
                self._wait(XFR.ISP, XFR.ISP_PROG_BUSY, False, 1.0,
                           f"page program @0x{addr:06X}")
                return
            except Exception:
                if attempt == 2:
                    raise
                # rst_flash_ctrl pulse, stay in ISP, retry the page
                self.xfr.write_reg(XFR.ISP, XFR.ISP_EN | 0x01)
                self.xfr.write_reg(XFR.ISP, XFR.ISP_EN)
                self._wait_flash_idle(1.0, "page retry recovery")

    def program(self, addr, data, progress=None):
        """Program an arbitrary-length, page-aligned region (erased first by
        the caller).  Skips all-0xFF pages."""
        assert addr % PAGE_SIZE == 0
        for off in range(0, len(data), PAGE_SIZE):
            page = bytes(data[off:off + PAGE_SIZE])
            if page.count(0xFF) != len(page):
                self.program_page(addr + off, page)
            if progress:
                progress(off + len(page), len(data))
