"""Host-side access to the OSD window-control SRAM (the debug mailbox home).

The RTD2556's DDC 0x4A slave decodes only the ISP block (0x60-0x76) and the
scaler indirect port (0xF3-0xF5); there is no host-visible XFR scratch, and
the indirect port does not reach the 8051's XDATA RAM (both confirmed
empirically on intouch156b).  The one true RAM both the host and the
firmware can access is the OSD SRAM, via the page-0 OSD port regs
0x90-0x94.  Word addresses are 15-bit; each word is 3 bytes (B0,B1,B2).

Mechanics ported faithfully from pyrtd2660i2c/osd.py (hardware-validated):
- OSD_WRITE_OPT is read-modify-written (sram_sel bits [3:2], bytesel [7:5]),
  set BEFORE the address;
- OSD_ADDR_H preserves its bit 7;
- reads return 0 unless FC_SRAM_CTRL (0x3A33) bit 4 sram_read_en is set and
  osd_en (0x3A02 bit 0) is off; a freshly addressed read returns stale data,
  so every word is read twice and the second result used;
- the byte pointer auto-advances B0 -> B1 -> B2 on each OSD_DATA access, but
  cross-word auto-increment is unreliable over DDC: address every word.

NOTE the port registers are shared with the firmware's own mailbox writes;
a colliding access garbles one transfer.  Callers should retry (see
monitor.py).
"""

from .scaler import Scaler

OSD_ADDR_H = 0x90       # bits [6:0] = word address [14:8]; bit 7 preserved
OSD_ADDR_L = 0x91
OSD_DATA = 0x92         # byte pointer auto-advances B0 -> B1 -> B2
OSD_MISC = 0x93         # bit 7: global double-buffer trigger
OSD_WRITE_OPT = 0x94    # [7:5] byte-select mask, [3:2] SRAM select
BYTESEL_ALL = 0xE0
SEL_WINDOW = 0x00       # window-control SRAM
SEL_FONT = 0x08

FC_SRAM_CTRL = 0x3A33   # bit 4 = sram_read_en
FC_CTRL_02 = 0x3A02     # bit 0 = osd_en (double-buffered)


class OsdRam:
    def __init__(self, scaler: Scaler):
        self.s = scaler

    def _rd(self, reg):
        return self.s.read_raw(reg, 1, increment=False)[0]

    def _wr(self, reg, val):
        self.s.write_raw(reg, [val], increment=False)

    def _setup(self, word, bytesel=BYTESEL_ALL, sram_sel=SEL_WINDOW):
        self.s.set_page(0)
        opt = self._rd(OSD_WRITE_OPT)
        opt = (opt & 0xF3) | (sram_sel & 0x0C)
        opt = (opt & 0x1F) | (bytesel & 0xE0)
        self._wr(OSD_WRITE_OPT, opt)
        addr_h = self._rd(OSD_ADDR_H)
        self._wr(OSD_ADDR_H, (addr_h & 0x80) | ((word >> 8) & 0x7F))
        self._wr(OSD_ADDR_L, word & 0xFF)

    def write_word(self, word, b0, b1, b2, sram_sel=SEL_WINDOW):
        self._setup(word, sram_sel=sram_sel)
        self._wr(OSD_DATA, b0)
        self._wr(OSD_DATA, b1)
        self._wr(OSD_DATA, b2)

    def trigger_double_buffer(self):
        self.s.set_page(0)
        self._wr(OSD_MISC, 0x80)
        self._wr(OSD_MISC, 0x00)

    def enable_readback(self):
        """Set sram_read_en (requires osd_en off).  Returns previous values
        so the caller can restore them.  osd_en is double-buffered, so
        clearing it is pushed live with the double-buffer trigger.
        FC_SRAM_CTRL is written as absolute 0x10/0x00, matching the
        hardware-validated osd.py enable_sram_read()."""
        prev_ctrl = self.s.read1(FC_SRAM_CTRL)
        prev_en = self.s.read1(FC_CTRL_02)
        if prev_en & 0x01:
            self.s.write(FC_CTRL_02, [prev_en & ~0x01])
            self.trigger_double_buffer()
        self.s.write(FC_SRAM_CTRL, [0x10])
        return prev_ctrl, prev_en

    def restore_readback(self, prev):
        prev_ctrl, prev_en = prev
        self.s.write(FC_SRAM_CTRL, [prev_ctrl])
        if prev_en & 0x01:
            self.s.write(FC_CTRL_02, [prev_en])
            self.trigger_double_buffer()

    def _read_word_once(self, word, sram_sel):
        # Read setup order differs from writes and matters: address first,
        # then WRITE_OPT (0xE0 | sram_sel) - ported from scaler.py osd_read.
        self.s.set_page(0)
        self._wr(OSD_ADDR_H, (word >> 8) & 0x7F)
        self._wr(OSD_ADDR_L, word & 0xFF)
        self._wr(OSD_WRITE_OPT, BYTESEL_ALL | (sram_sel & 0x0C))
        vals = self.s.read_raw(OSD_DATA, 3, increment=False)
        return tuple(vals)

    def read_word(self, word, sram_sel=SEL_WINDOW):
        """Read one 3-byte word.  enable_readback() must be active.  The
        first pass after addressing returns stale data - read twice, use
        the second (matches osd.py's validated behavior)."""
        self._read_word_once(word, sram_sel)
        return self._read_word_once(word, sram_sel)

    def read_words(self, word, count, sram_sel=SEL_WINDOW):
        return [self.read_word(word + i, sram_sel) for i in range(count)]
