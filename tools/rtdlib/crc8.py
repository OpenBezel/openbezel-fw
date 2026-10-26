"""Realtek CRC-8, as computed by the RTD26xx/25xx hardware CRC engine.

The chip's SPI-flash controller can compute this over an arbitrary flash
address range (start in XFR 0x64-0x66, end in 0x72-0x74, trigger 0x6F=0x84,
result in 0x75).
"""

_POLY = 0x1070 << 3  # 0x8380


class Crc8:
    """Incremental CRC. The internal state is 16 bits; only the high byte is
    the result, so continuation must go through this class, not crc8()."""

    def __init__(self):
        self._state = 0

    def update(self, data):
        crc = self._state
        for b in data:
            crc ^= b << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc ^= _POLY
                crc = (crc << 1) & 0xFFFF
        self._state = crc
        return self

    @property
    def value(self):
        return self._state >> 8


def crc8(data):
    """One-shot CRC over an iterable of byte values."""
    return Crc8().update(data).value
