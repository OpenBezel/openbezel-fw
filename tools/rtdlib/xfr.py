"""The XFR register file: DDC address 0x4A, registers 0x00-0xFF.

From the 8051's point of view this is XDATA 0xFF00-0xFFFF; DDC 0x4A register
NN is XDATA 0xFFNN.  It holds chip-level control including the ISP/SPI-flash
controller (0x60-0x76), the scaler indirect port (0xF3-0xF5), CODE_PAGE
(0xFF), and free scratch space the OpenBezel debug monitor uses as its
mailbox (see monitor.py).

Confirmed hardware behavior (tested on the intouch156b board): setting ISP_EN
halts the 8051 (no watchdog runs while in ISP mode; it can be held
indefinitely), and clearing ISP_EN resets the 8051, rebooting into whatever
is in flash.  Flashing is therefore race-free against running firmware, and
leaving ISP is the normal "reboot into new image" step.
"""

from .bus import Bus


class XFR:
    # ISP / SPI flash controller
    SPI_CMD = 0x60          # [7:5] cmd type, [4:3] wr len, [2:1] rd len, [0] execute/busy
    SPI_OPCODE = 0x61
    SPI_OP_WREN = 0x62      # write-enable opcode (usually 0x06)
    SPI_OP_WRSR = 0x63      # write-status-register opcode
    SPI_WDATA0 = 0x64       # write data / 24-bit address, big-endian 0x64..0x66
    SPI_RDATA0 = 0x67       # read data 0x67..0x69
    SPI_OP_READ = 0x6A
    SPI_OP_FASTREAD = 0x6B
    SPI_OP_PROGRAM = 0x6D
    SPI_OP_RDSR = 0x6E
    # [7] isp_en (halts 8051), [6] prog_mode (0=normal 1=AAI),
    # [5] prog_en (write 1 to start, auto-clears when finished),
    # [4] prog_buf_wr_en (MUST be 1 while filling the 0x70 SRAM buffer),
    # [3] prog_normal_mode (0=page program), [2] crc_start, [1] crc_done,
    # [0] rst_flash_ctrl
    ISP = 0x6F
    ISP_EN = 0x80
    ISP_BUF_WR = 0x90       # isp_en | prog_buf_wr_en
    ISP_PROGRAM = 0xA0      # isp_en | prog_en
    ISP_PROG_BUSY = 0x20    # prog_en still set = program on-going
    ISP_CRC = 0x84          # isp_en | crc_start
    SPI_FIFO = 0x70
    SPI_PROG_LEN = 0x71     # program size - 1 (255 => 256-byte page)
    SPI_CRC_END0 = 0x72     # CRC end address, big-endian 0x72..0x74
    SPI_CRC_RESULT = 0x75

    # Scaler indirect interface
    SCA_INF_CTRL = 0xF3
    CTRL_RD_EN = 0x80
    CTRL_WR_EN = 0x40
    CTRL_ADDR_INC_DIS = 0x20
    SCA_INF_ADDR = 0xF4
    SCA_INF_DATA = 0xF5

    CODE_PAGE = 0xFF

    def __init__(self, bus: Bus):
        self.bus = bus

    def write_reg(self, reg, value):
        self.bus.write_reg(reg, value)

    def read_reg(self, reg):
        return self.bus.read_reg(reg)

    def enable_isp(self):
        """Enter ISP mode.  HALTS THE 8051 until disable_isp()."""
        self.write_reg(self.ISP, self.ISP_EN)

    def disable_isp(self):
        """Leave ISP mode.  RESETS THE 8051 (boots the image in flash).

        Only the 8051 resets: the XFR page keeps its values, CODE_PAGE (0xFF)
        included, and the reset vector is fetched through it.  Halted with any
        page but 0 mapped - a banked call in progress, or the garbage 0x50 seen
        2026-10-10 - the restart ran the wrong flash page and never reached
        main(); the garbage it executed then broke the eDP PLL until a power
        cycle.  So map page 0 first, on every exit."""
        self.write_reg(self.CODE_PAGE, 0x00)
        self.write_reg(self.ISP, 0)

    def in_isp(self):
        return bool(self.read_reg(self.ISP) & self.ISP_EN)
