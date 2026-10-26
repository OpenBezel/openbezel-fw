"""rtdlib - host-side access to Realtek RTD26xx/RTD25xx scalers over DDC/I2C.

Modules:
  bus      - raw I2C transport (i2c_rdwr based, block transfers, size autoprobe)
  xfr      - the DDC 0x4A register file (ISP enable, scaler indirect port)
  scaler   - paged scaler register access (pages 0x00-0xFF via PAGE_SEL)
  isp      - SPI flash controller behind the XFR (read/erase/program/CRC)
  flashdb  - JEDEC flash chip table
  crc8     - the Realtek CRC-8 (matches the chip's hardware CRC engine)
  monitor  - OpenBezel firmware debug-monitor mailbox client
"""

from .bus import Bus
from .xfr import XFR
from .scaler import Scaler
from .isp import ISP
from .crc8 import crc8
