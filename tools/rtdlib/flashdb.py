"""JEDEC ID table for SPI flash chips seen on RTD26xx/25xx boards.

Erase capabilities are per-chip: ``sector_erase`` (4 KB, opcode 0x20) and
``block_erase`` (32/64 KB, opcode 0xD8/0x52) are what the flasher prefers;
chip erase (0xC7) is the last resort and destroys board-unique data
(HDCP keys, serial, stored settings) if the image being written doesn't
carry it - avoid.
"""
from __future__ import annotations    # `int | None` on Python 3.9 too

from dataclasses import dataclass


@dataclass(frozen=True)
class FlashChip:
    name: str
    jedec_id: int
    size_kb: int
    page_size: int = 256
    sector_erase_op: int | None = 0x20   # 4 KB; None if unsupported
    block_erase_op: int | None = 0xD8    # 64 KB; None if unsupported


FLASH_CHIPS = [
    # Atmel
    FlashChip("AT25DF041A", 0x1F4401, 512),
    FlashChip("AT25DF161", 0x1F4602, 2048),
    FlashChip("AT26DF081A", 0x1F4501, 1024),
    FlashChip("AT26DF0161", 0x1F4600, 2048),
    FlashChip("AT26DF161A", 0x1F4601, 2048),
    FlashChip("AT25DF321", 0x1F4701, 4096),
    FlashChip("AT25DF512B", 0x1F6501, 64, sector_erase_op=0x20, block_erase_op=0x52),
    FlashChip("AT25DF512B", 0x1F6500, 64, sector_erase_op=0x20, block_erase_op=0x52),
    FlashChip("AT25DF021", 0x1F3200, 256),
    FlashChip("AT26DF641", 0x1F4800, 8192),
    # ST
    FlashChip("M25P05", 0x202010, 64, sector_erase_op=None),   # M25P: no 4K sectors
    FlashChip("M25P10", 0x202011, 128, sector_erase_op=None),
    FlashChip("M25P20", 0x202012, 256, sector_erase_op=None),
    FlashChip("M25P40", 0x202013, 512, sector_erase_op=None),
    FlashChip("M25P80", 0x202014, 1024, sector_erase_op=None),
    FlashChip("M25P16", 0x202015, 2048, sector_erase_op=None),
    FlashChip("M25P32", 0x202016, 4096, sector_erase_op=None),
    FlashChip("M25P64", 0x202017, 8192, sector_erase_op=None),
    # Winbond
    FlashChip("W25X10", 0xEF3011, 128),
    FlashChip("W25X20", 0xEF3012, 256),
    FlashChip("W25X40", 0xEF3013, 512),
    FlashChip("W25X80", 0xEF3014, 1024),
    FlashChip("W25Q16", 0xEF4015, 2048),
    FlashChip("W25Q32", 0xEF4016, 4096),
    # Macronix
    FlashChip("MX25L512", 0xC22010, 64),
    FlashChip("MX25L8005", 0xC22014, 1024),
    FlashChip("MX25L1605", 0xC22015, 2048),     # MX25L1605/1606E (AOC U4008B, 2026-10-10)
    FlashChip("MX25L3205", 0xC22016, 4096),
    FlashChip("MX25L6405", 0xC22017, 8192),
    # GigaDevice
    FlashChip("GD25Q16", 0xC84015, 2048),
    FlashChip("GD25Q32", 0xC84016, 4096),
    # Microchip/SST
    FlashChip("SST25VF512", 0xBF4800, 64),
    FlashChip("SST25VF032", 0xBF4A00, 4096),
]

MANUFACTURERS = {
    0x1F: "Atmel",
    0x20: "ST",
    0xBF: "Microchip/SST",
    0xC2: "Macronix",
    0xC8: "GigaDevice",
    0xEF: "Winbond",
}


def find_chip(jedec_id):
    for chip in FLASH_CHIPS:
        if chip.jedec_id == jedec_id:
            return chip
    return None


def manufacturer_name(jedec_id):
    return MANUFACTURERS.get(jedec_id >> 16, "Unknown")
