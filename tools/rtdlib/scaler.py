"""Paged scaler register access via the XFR indirect port (0xF3/0xF4/0xF5).

The scaler register space is 256 pages of 256 bytes; the current page is
selected by writing PAGE_SEL (offset 0x9F, present in every page).  Pages
0x00-0x2F correspond to the 8051's XDATA 0x0000-0x2FFF.  This works on a live
chip regardless of firmware state - it is "Channel A" of the debug story.

NOTE: this port reaches the scaler pages only.
"""

from .xfr import XFR

PAGE_SEL = 0x9F


class Scaler:
    def __init__(self, xfr: XFR):
        self.xfr = xfr
        self._page = None  # cached current page; None = unknown

    def set_page(self, page):
        if self._page != page:
            self.write_raw(PAGE_SEL, [page])
            self._page = page

    def invalidate_page_cache(self):
        """Call after anything else (firmware, another tool) may have changed
        PAGE_SEL behind our back."""
        self._page = None

    def write_raw(self, offset, data, increment=True):
        """Write bytes at an offset within the CURRENT page (no page set)."""
        ctrl = XFR.CTRL_WR_EN | (0 if increment else XFR.CTRL_ADDR_INC_DIS)
        self.xfr.write_reg(XFR.SCA_INF_CTRL, ctrl)
        self.xfr.write_reg(XFR.SCA_INF_ADDR, offset)
        for b in data:
            self.xfr.write_reg(XFR.SCA_INF_DATA, b)

    def read_raw(self, offset, count=1, increment=True):
        """Read bytes from an offset within the CURRENT page."""
        ctrl = XFR.CTRL_RD_EN | (0 if increment else XFR.CTRL_ADDR_INC_DIS)
        self.xfr.write_reg(XFR.SCA_INF_CTRL, ctrl)
        self.xfr.write_reg(XFR.SCA_INF_ADDR, offset)
        return [self.xfr.read_reg(XFR.SCA_INF_DATA) for _ in range(count)]

    # sreg addressing: 16-bit page:offset, matching the firmware's sreg_t
    def read(self, sreg, count=1):
        self.set_page((sreg >> 8) & 0xFF)
        return self.read_raw(sreg & 0xFF, count)

    def read1(self, sreg):
        return self.read(sreg)[0]

    def write(self, sreg, data):
        self.set_page((sreg >> 8) & 0xFF)
        self.write_raw(sreg & 0xFF, data if hasattr(data, "__iter__") else [data])

    def read_page(self, page):
        """Read all 256 bytes of one page."""
        self.set_page(page)
        return bytes(self.read_raw(0x00, 256))

    def snapshot(self, pages=range(256), progress=None):
        """Read-only snapshot of the given pages -> {page: bytes(256)}.
        (Unlike the old page_dump() this never writes probe values.)"""
        out = {}
        for p in pages:
            out[p] = self.read_page(p)
            if progress:
                progress(p)
        return out
