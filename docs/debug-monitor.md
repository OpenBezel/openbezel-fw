# Debug monitor

The JTAG substitute: everything rides the display's DDC I2C bus.

## What the hardware exposes

The DDC 0x4A slave decodes only:

- the ISP/SPI-flash block, registers 0x60-0x76 (0x6F = ISP entry), and
- the scaler indirect port, registers 0xF3-0xF5.

Every other register (0x00-0x5F, 0x77-0xF2, 0xF6-0xFE) reads 0 and ignores host writes, even
the ones that are real XFR registers for the 8051 (PWM, LSADC, watchdog ...).  The indirect port
reaches only scaler hardware registers - PAGE_SEL pages 0xD7+ do NOT map the 8051's XDATA RAM.
And all of 0x4A except 0x6F needs ISP mode (`isp_en`), which halts the 8051.
So there are three ways in:

## 1. Scaler registers, through ISP - works even with dead firmware

The indirect port (0xF3 ctrl / 0xF4 addr / 0xF5 data + PAGE_SEL 0x9F) reads and writes all 256
scaler register pages with no firmware cooperation.  Tools: `rtdmon.py sreg/sreg-write/autopsy`,
`dump_pages.py`, `diff_pages.py`.

## 2. The mailbox in OSD font SRAM - halt-and-inspect

Words MB_WORD_BASE (0x1F00) .. +0x0F of the **font SRAM** (3 bytes per word; the OSD itself uses
0x0000-0x0460), written by the firmware through the page-0 OSD port (XDATA 0x0090-0x0094) and read
by the host with the OSD readback sequence (sram_read_en, osd_en off, double read, address then
WRITE_OPT).  Font SRAM because it passes write-then-readback; the window-control SRAM does not.
Layout - the same in `core/dbgmon.h` and `tools/rtdlib/monitor.py`:

| Word | B0 | B1 | B2 |
|---|---|---|---|
| 0x000 | MAGIC 0x5A | HEARTBEAT | CHECKPOINT |
| 0x001 | key polls | lowest A0 seen | UI mode (the main loop; 0 until it runs) |
| 0x002-0x00F | scratch diagnostics (dbgmon_diag) |

(Word numbers relative to MB_WORD_BASE.)

* **Reading it halts the firmware**: the host enters ISP to read (freezing the firmware's
  state) and leaves ISP afterwards, which RESETS the chip and restarts the firmware - every
  `rtdmon status` observes a fresh run.  The heartbeat is about loop rate x uptime since that
  run's boot; the checkpoint attributes a hang.
* Never poll it in a tight loop right after a flash or reset: an access during the ISP break-in
  window halts the chip there.
* The OSD port registers are shared between host reads and firmware writes; a collision garbles
  one transfer, so the host client retries until MAGIC reads 0x5A.
* Host readback needs osd_en = 0.

**Checkpoints** (`CP_*` in `core/dbgmon.h`, and the `dbgmon_checkpoint()` calls in main.c) are
written BEFORE each init stage, so a hang is attributable even with the mailbox write-only.

## 3. Live, while the firmware runs

* **DDC/CI debug codes** (`CONFIG_DEBUG_VCP`, docs/ddcci.md): register peek / poke, indirect
  ports, OSD SRAM pokes, flash test, reboot, capture diagnostics.
* **The debug console** (`CONFIG_DEBUG_CONSOLE`): a status page drawn through the OSD
  (`core/console.c`, refreshed by main.c every heartbeat) - loop, timebase, DDC/CI, display
  clock, capture and link status - reached from the menu (OSD > Debug console) or DDC/CI E7h = 2.
