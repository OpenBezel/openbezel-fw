"""Raw I2C transport for the DDC 0x4A register file.

Uses smbus2's i2c_rdwr for multi-byte transfers.  DDC controllers vary in the
transfer sizes they support (the historical i2c-tiny-usb adapter maxed out
around 63 bytes; GPU DDC masters differ), so read/write chunk sizes are
autoprobe-able and clamp-able.

All register access goes through here so retry policy and transfer limits
live in exactly one place.
"""

import time

DEFAULT_DEV_ADDR = 0x4A


class BusError(IOError):
    pass


class Bus:
    def __init__(self, bus_number, dev_addr=DEFAULT_DEV_ADDR,
                 max_read=64, max_write=32, retries=3):
        # Imported here so rtdlib's pure-computation modules (crc8, snapshot
        # parsing) work on machines without smbus2 installed.
        global i2c_msg
        try:
            from smbus2 import SMBus, i2c_msg
        except ImportError:
            raise SystemExit("smbus2 is not installed (apt install python3-smbus2, "
                             "pip install smbus2, or run in the container: tools/docker-run)")
        self.bus = SMBus(bus_number)
        self.addr = dev_addr
        self.max_read = max_read
        self.max_write = max_write
        self.retries = retries

    def close(self):
        self.bus.close()

    def _rdwr(self, *msgs):
        last = None
        for attempt in range(self.retries):
            try:
                self.bus.i2c_rdwr(*msgs)
                return
            except OSError as e:
                last = e
                time.sleep(0.001 * (attempt + 1))
        raise BusError(f"i2c transfer to 0x{self.addr:02X} failed: {last}")

    def write_reg(self, reg, value):
        """Write one byte to one register."""
        self._rdwr(i2c_msg.write(self.addr, bytes([reg, value & 0xFF])))

    def read_reg(self, reg):
        """Read one byte from one register."""
        w = i2c_msg.write(self.addr, bytes([reg]))
        r = i2c_msg.read(self.addr, 1)
        self._rdwr(w, r)
        return bytes(r)[0]

    def write_fifo(self, reg, data):
        """Stream bytes into a FIFO-style register (repeated data bytes in one
        transaction all land on the same register, which accumulates them -
        this is how the 0x70 flash FIFO behaves)."""
        data = bytes(data)
        off = 0
        while off < len(data):
            chunk = data[off:off + self.max_write]
            self._rdwr(i2c_msg.write(self.addr, bytes([reg]) + chunk))
            off += len(chunk)

    def read_fifo(self, reg, count):
        """Read ``count`` bytes from a FIFO-style register. A multi-byte read
        transaction pops successive bytes from the FIFO."""
        out = bytearray()
        while len(out) < count:
            n = min(count - len(out), self.max_read)
            w = i2c_msg.write(self.addr, bytes([reg]))
            r = i2c_msg.read(self.addr, n)
            self._rdwr(w, r)
            out += bytes(r)
        return bytes(out)

# Transfer-size autoprobing lives in ISP.autoprobe_transfer_sizes(), which can
# reposition the flash-read FIFO between attempts and compare against a
# byte-at-a-time reference read.  Bus only holds the chosen limits.
