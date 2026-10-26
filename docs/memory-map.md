# RTD2556 memory map (as used by OpenBezel)

## CODE (per 64 KB bank, banked by CODE_PAGE = XDATA 0xFFFF)

| Range | Use |
|---|---|
| 0x0000-0xAFFF | COMMON region, identical in every bank image (docs/banking.md) |
| 0xB000-0xFEFF | the mapped bank's own window (banked code) |
| 0xFF00-0xFF0F | build-info block, stamped by mkimage.py: `OBFW` + version |
| 0xFF10-0xFFFF | reserved (stock firmware leaves the bank tail erased) |

Flash offset 0 is executed raw by the boot ROM: no header, no checksum.
The reset vector must be an LJMP (mkimage.py enforces this).

## Flash partitions (2 MB part on intouch156b)

| Range | Use |
|---|---|
| 0x000000-0x00FFFF | code bank 0 (the firmware) |
| 0x010000-0x01FFFF | **stock bank 1 - DO NOT WRITE.** This board's HDCP 1.4 device keys (sector 0x013000; rtdflash refuses to change it), inside one of stock's code banks - see below |
| 0x020000-0x02FFFF | **asset partition**: tables too big for the code bank |
| 0x030000-0x03FFFF | code bank 3 (DDC/CI, menu, picture modes) |
| 0x040000-0x04FFFF | code bank 4 (source state machines, HDMI receiver, HDCP) |
| 0x050000-0x05FFFF | code bank 5 (menu renderer, dynamic contrast) |
| 0x060000-0x06FFFF | code bank 6 (DP receiver outside its interrupt handler) |
| 0x09B000-0x09CFFF | stock's settings NVRAM - left untouched, so a stock restore keeps its settings |
| 0x100000-0x101FFF | **settings log** (in use): two sectors, 64-byte CRC'd records |
| 0x1FF000-0x1FFFFF | scratch sector for the firmware flash test (DDC/CI F3h/F4h) |

Everything else is free.

**Write protection: SR1 = 0xB4** (`rtdflash.py protect`, once per board): the
lower 1 MB (code, assets, stock's NVRAM) is protected in the flash itself, the
upper 1 MB is writable by the firmware.  flash_rw.c additionally refuses any
write below 0x100000.  A factory board sits at 0xFC (everything protected);
there, settings saves fail their read-back and simply do not persist.

### Stock bank 1 (0x010000)

One of stock's code banks: its first 0x2170 bytes are the common copy every stock bank
carries, 0x14000 on is code (DP receiver, power sequencing), and the data between is:

| flash | contents |
|---|---|
| 0x01226F-0x01306E | 14 EDID templates, 256 bytes each: 7 panel sizes (1920x1080, 1366x768, 1280x800, 1600x900, 1024x768, 1920x1200) for DP (0-6) and HDMI (7-13); "TCH" product 0x0056.  Stock copies the one an EDID index selects (DP from 0x01236F, HDMI from 0x012A6F) and patches the product, date and name from the tables below |
| 0x01306F-0x0131B3 | **HDCP device keys** stock and OpenBezel use: the KSV (5 bytes, public), then the private key block - never read out, copied or logged |
| 0x0131B4-0x0132F8 | **a second HDCP key set** of the same shape (a valid KSV, then 320 bytes) that stock never reads - perhaps transmitter or spare receiver keys; the same rules apply |
| 0x0132F9-0x01335B | the EDID patch tables: manufacturer, the "DP" / "TYPE-C" / "HDMI" names, product and date |
| 0x01335C, 0x0133DC | two empty EDID slots (128 and 256 bytes, all zero) |

The two key sets are the only board-unique data here, and are the only data used by the OpenBezel firmware.

### Asset partition (0x020000)

Contents: four gamma LUTs, the OSD text font, the logo glyphs and the logo's per-cell
character-command stream (precomputed at build time, so the CPU does no per-cell work at
draw time), the theme's icons, and the menu languages' strings.  47 KB of the 64 KB
partition; gen_assets.py fails the build past it.

Built by `tools/gen_assets.py` into `build/<board>/assets.bin`, flashed with
`ninja -C build flash` (or `flash-assets`), offsets emitted as
`assets.h` in the build directory. Layout: 8-byte header (`OBAS`, version, count) then
the blobs back to back.

Read by the **burst engine**, not by the CPU: `asset_burst_to_port()` points
`SCA_INF_*` at a scaler page:port, sets the 24-bit flash address and length,
clears `BANK_SWITCH_CONTROL` bit 5 (**0 = source from external flash**, 1 =
internal XRAM) and starts `REG_BURDAT_WR | ADDR_NON_INC`. The MCU halts until
the transfer finishes. This is exactly stock's `sca_burst_transfer`.

Two consequences worth knowing:

* **The firmware cannot read this partition.** Its code window is the first
  64 KB and the burst engine never exposes the bytes to the CPU, so there is no
  runtime integrity check. Callers must be safe against a blank partition -
  gamma defaults to bypass for exactly this reason.
* **Nothing here may be touched before the ISP break-in window.** The recovery
  path (DDC up, EDID served, HPD asserted, the ISP window) must not depend on the
  asset partition, or a bad partition costs us DDC reflashing.
  `assets_enable()` is called after the window and `asset_burst_to_port()` is a
  no-op before it; `mkimage.py --map` additionally fails the build if any
  recovery-path symbol leaves the first code page.

## XDATA

| Range | Use |
|---|---|
| 0x0000-0x2FFF | scaler register pages (window; page select via PAGE_SEL 0x9F) |
| 0xD700-0xDFFF | data/bss (`--xram-loc 0xD700 --xram-size 0x0900`) - **XSEG + XISEG must end by 0xE000** (the flash program buffer follows): sdld runs without its memory check (banked code), so tools/check_banking.py enforces it |
| 0xE000-0xE0FF | flash program buffer: the controller takes PROG data from XRAM here (flash_rw.h) |
| 0xE100-0xE1FF | fixed scratch buffers, 64 B each (ob_types.h XRAM_*): settings record, saved copy, scan temp, language string |
| 0xE200-0xE27F | free (stock kept variables at 0xE1xx: plain RAM) |
| 0xE280-0xE37F | DP (DDC1) EDID RAM |
| 0xE680-0xE77F | HDMI DDC RAM (EDID, served at I2C 0x50) |
| 0xF600-0xF6FF | reserved: debug log ring (fixed address, host-known) |
| 0xFE00-0xFE5F | GPIO pin regs (1 byte/pin; 0xFE19 = flash WP#), mux at 0xFE60+ |
| 0xFF00-0xFFFF | XFR page (8051-side only; the DDC 0x4A slave decodes just 0x60-0x76 + 0xF3-0xF5) |
| 0xFFEA | WATCHDOG_TIMER (enables: 0xFFE9[7]/0xFFEA[7]/0xFF3A[6]) |
| 0xFFEE | bit1 sof_rst = software chip reset |
| 0xFFFF | CODE_PAGE bank register |

Debug-monitor mailbox: OSD window-SRAM words 0x0300+ (not XDATA) - see
docs/debug-monitor.md.

## IRAM

128 B data + 128 B idata (`--iram-size 0x100`). Register bank 0 = main
loop; ISRs will use `__using(1)`. SP set by SDCC's crt0 after DSEG.

The scaler register space itself is 256 pages x 256 B, addressed as
page:offset (`sreg_t`, high byte = page). Host-side access to the same
space: XFR regs 0xF3/0xF4/0xF5 + PAGE_SEL, see tools/rtdlib/scaler.py.
