# OpenBezel Firmware

Open-source 8051 firmware for monitor scaler chips.

# Supported Hardware

Realtek **RTD2556** (HDMI + USB-C input -> eDP 1920x1080 panel, "intouch156b" board).

Built with [SDCC](https://sdcc.sourceforge.net/) (>= 4.2). Flashed in-system
over the display's own DDC/I2C lines - the RTD26xx/25xx family exposes its
SPI-flash controller at DDC address 0x4A, and entering ISP mode halts the
8051 (leaving it resets the chip into the new image), so the whole
build-flash-boot loop needs no extra hardware beyond an I2C master on the
DDC pins.

## Status

A complete replacement for the stock firmware on the intouch156b board:

- Video: HDMI and USB-C (DisplayPort alt-mode) inputs with automatic or fixed
  input selection, any resolution, Full/Fit scaling, clean mode changes; eDP
  panel bring-up from cold and warm boots.
- HDCP 1.4 (with the board's stock keys)
- Audio from both inputs (volume, mute)
- Picture: brightness, contrast, saturation, sharpness, gamma, color
  temperature with user R/G/B gains, low blue light, dynamic contrast, picture
  modes.
- A graphical, themed on-screen menu in five languages (build-time themes,
  `themes/`; languages, `lang/`), popups, hotkeys
- DDC/CI (MCCS 2.2)
- Persistent settings
- Standby/powersave.
- Host tooling (`tools/`): the flasher (`rtdflash.py`), debug-monitor client
  (`rtdmon.py`), an OSD simulator that renders the menu from the firmware's own
  code (`tools/osdsim/`), table generators.

## Before you flash: back up the vendor firmware

> **Warning:** save the monitor's original firmware before flashing OpenBezel for
> the first time. It is the only way back to a working monitor if something goes
> wrong, and no copy of it is distributed here.

Dump the whole flash (2 MB on the intouch156b) over the same DDC/I2C connection,
substituting the i2c bus wired to the display's DDC:

```
tools/docker-run python3 tools/rtdflash.py --bus 6 dump vendor-backup.bin   # CRC-checked against the chip
```

To restore it later, write the dump back (only sectors that differ are rewritten):

```
tools/docker-run python3 tools/rtdflash.py --bus 6 flash --sync vendor-backup.bin
```

Keep the dump private: it holds the board's own HDCP device keys, which are
unique to that monitor and licensed. Do not share or publish it.

## Build

Meson + Ninja, configured with Kconfig - docs/building.md:

```
# in the build container (docker/Dockerfile) - or install the packages in docs/building.md
# and drop the tools/docker-run prefix
tools/docker-run meson setup build      # .config from configs/intouch156b_defconfig
                                        #   (another: -Ddefconfig=<name>_defconfig)
tools/docker-run ninja -C build menuconfig   # optional: theme, languages, features, defaults
tools/docker-run ninja -C build    # firmware images + asset partition
tools/docker-run meson configure build -Di2c_bus=6   # the i2c bus wired to the display's DDC
tools/docker-run ninja -C build flash   # sector-diff flash + verify + reboot into it
```

## License

MIT - see LICENSE.
