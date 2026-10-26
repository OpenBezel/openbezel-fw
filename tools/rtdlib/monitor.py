"""Client for the OpenBezel firmware debug monitor.

Transport (RTD2556): OSD FONT SRAM, words 0x1F00-0x1F0F.  This is the only
RAM both the host (via the page-0 OSD port, regs 0x90-0x94) and the
firmware can reach AND read back - the DDC 0x4A slave decodes no XFR
scratch, the indirect port cannot see XDATA RAM, and the window-control
SRAM fails write-readback (all confirmed empirically on 2026-09-06).

Word layout (3 bytes per OSD word) - MUST match core/dbgmon.h:
  word 0x000: MAGIC(0x5A) | HEARTBEAT | CHECKPOINT
  word 0x001: key polls | lowest A0 seen | UI mode (main loop; 0 until it runs)
  words 0x002..0x00F: reserved for a command channel
The firmware's own OSD SRAM use stays clear of these words (MB_WORD_BASE,
core/dbgmon.h).

There is no command channel (peek/poke) over the mailbox: the firmware does
not read OSD SRAM back.  Peek and poke live registers with the DDC/CI debug
VCPs (F0h/F1h, FCh) or the hardware window (rtdmon sreg / sreg-write).

Reading the mailbox requires osd_en to be off (true for skeleton firmware).
Port collisions with the firmware's own writes are possible; status() reads
until two consecutive samples agree on the static MAGIC byte.
"""

import time

from .bus import Bus
from .xfr import XFR
from .scaler import Scaler
from .osdram import OsdRam, SEL_FONT

MB_WORD_BASE = 0x1F00
MAGIC_ALIVE = 0x5A


class MonitorError(IOError):
    pass


class Monitor:
    """Halt-and-inspect monitor.

    The host reaches OSD SRAM only through the DDC 0x4A window, and on the
    RTD2556 that window answers only in ISP mode (every register but
    PROGRAM_INSTRUCTION reads 0 until isp_en is set - a chip rule).  So
    reading the mailbox HALTS the firmware and freezes its state; leaving
    ISP afterwards RESETS the chip, restarting the firmware from zero.  Interpretation: heartbeat ~
    loop rate x uptime since the firmware's boot; checkpoint (and word 1, zero
    until the main loop runs) tell how far it got.  Consecutive status() calls each observe a fresh run.
    """

    def __init__(self, bus: Bus):
        self.xfr = XFR(bus)
        self.osd = OsdRam(Scaler(self.xfr))

    def _read_state(self):
        self.xfr.enable_isp()   # halt firmware; mailbox state freezes
        # The chip reset on every previous exit reverted PAGE_SEL; drop the
        # client-side page cache or reads silently hit the wrong page.
        self.osd.s.invalidate_page_cache()
        try:
            prev = self.osd.enable_readback()
            try:
                for _ in range(5):
                    w0 = self.osd.read_word(MB_WORD_BASE, sram_sel=SEL_FONT)
                    w1 = self.osd.read_word(MB_WORD_BASE + 1, sram_sel=SEL_FONT)
                    if w0[0] == MAGIC_ALIVE:
                        return w0, w1
                return w0, w1
            finally:
                self.osd.restore_readback(prev)
        finally:
            self.xfr.disable_isp()  # resets the chip; firmware restarts

    def alive(self):
        w0, _ = self._read_state()
        return w0[0] == MAGIC_ALIVE

    def status(self):
        w0, w1 = self._read_state()
        return {
            "magic": w0[0],
            "heartbeat": w0[1],
            "checkpoint": w0[2],
            "key_polls": w1[0],
            "key_min_a0": w1[1],
            "ui_mode": w1[2],
        }

    def heartbeat_rate(self, interval=0.5):
        """Approximate heartbeats/second.  Each status() read restarts the
        firmware, so this measures: restart, run for ``interval``, read.
        Only meaningful while interval is shorter than the byte's wrap
        time; treat it as a liveness indicator, not a precise rate."""
        self.status()            # ends with a chip reset -> fresh run
        time.sleep(interval)
        b = self.status()["heartbeat"]
        return b / interval

    # -- command channel: none on the RTD2556 ----------------------------------

    def _no_cmd(self):
        raise MonitorError(
            "no peek/poke over the mailbox: use the DDC/CI debug VCPs "
            "(F0h/F1h peek, FCh poke) or rtdmon sreg / sreg-write.")

    def peek_xdata(self, addr, length=1):
        self._no_cmd()

    def poke_xdata(self, addr, data):
        self._no_cmd()

    def peek_iram(self, addr, length=1):
        self._no_cmd()

    def poke_iram(self, addr, data):
        self._no_cmd()

    def peek_code(self, addr, length=1):
        self._no_cmd()

    def reset(self):
        self._no_cmd()
