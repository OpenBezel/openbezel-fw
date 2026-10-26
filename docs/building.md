# Building and configuring

Meson + Ninja, configured with Kconfig.

## Build environment

**The container** (recommended): `docker/Dockerfile` is Ubuntu 26.04 with everything the build
and the tools need, from the distribution's archive - pinned, so the firmware image and the
rendered fonts and logo come out byte for byte the same on every machine.
`tools/docker-run CMD...` builds the image the first time (and whenever the Dockerfile changes)
and runs CMD in it, as you, with the repository mounted at its own path:

```
tools/docker-run meson setup build
tools/docker-run ninja -C build
tools/docker-run ninja -C build menuconfig    # guiconfig too, given an X server (DISPLAY)
tools/docker-run ninja -C build flash         # the host's /dev/i2c-* are passed in
```

(`OB_DOCKER=podman` for podman.)  Keep a build directory on one side: one configured in the
container finds the container's tools.

**On the host** instead - the same packages (Ubuntu / Debian names):

```
sudo apt install sdcc meson ninja-build python3-pil python3-kconfiglib python3-smbus2 \
                 librsvg2-bin fonts-dejavu-core fonts-inter edid-decode
```

* SDCC >= 4.2, Meson >= 1.1, Ninja
* one Python >= 3.9 with Pillow and kconfiglib (the build looks for it among `python3`,
  `python3.1x`, `/usr/bin/python3`); smbus2 for flashing
* `rsvg-convert` and the theme fonts (themes/fonts/*/font.json name the files; installed fonts
  are found under /usr/share/fonts, /usr/local/share/fonts, ~/.local/share/fonts)
* optional: `edid-decode` (checks the generated EDID); for osdsim numpy, tkinter and a C compiler

A different SDCC, FreeType or font version can give a different image or different glyphs.

## Build

```
meson setup build                    # .config seeded from configs/intouch156b_defconfig
ninja -C build                       # build/openbezel.bin + openbezel_bankN.bin, build/gen/assets.bin
```

Another starting configuration: `meson setup build -Ddefconfig=intouch156b_terminal_defconfig`
(an option - Meson reads both positional arguments as directories, so
`meson setup build intouch156b_defconfig` does not work).  In an existing build directory,
every `configs/*_defconfig` is also a target, as in the kernel:
`ninja -C build intouch156b_terminal_defconfig` loads it and the next `ninja` rebuilds.
Several build directories can hold different configurations side by side.

## Configure

The configuration is `build/.config`, made from `Kconfig` at the repository root:

```
ninja -C build menuconfig            # terminal UI
ninja -C build guiconfig             # Tk UI
ninja -C build savedefconfig         # build/defconfig: the minimal form - copy it to configs/
```

The next `ninja` picks up a changed `.config` by itself (Meson reconfigures).  In guiconfig the
custom theme colors show a color swatch on their line and open a color picker
(tools/kconfig_colors.py).  What it
covers:

| menu | what |
|---|---|
| Board | the board (and so the chip); the panel's native gamma, which the gamma presets are computed from; the EDID: OpenBezel's (generated from `boards/<board>/edid.json`, default), the board's stock one (`edid.bin`) or any binary |
| Menu theme | a theme, or `custom`; any part - icons, colors, layout, frame, font - swapped in (themes/README.md); Colors > custom: a base palette with any of its 22 colors replaced |
| Languages | which of de/fr/es/it are built in (English always is), and the default |
| Features | picture modes, dynamic contrast, low blue light, user R/G/B gains - compiled out when off, menu items and MCCS codes included |
| Default settings | what a new board / a factory reset starts from: boot logo, menu size, slider style |

In C a symbol is `CONFIG_<NAME>` in `config.h` (force-included): `#ifdef CONFIG_FEATURE_DCR`.
Strings are quoted; `n` symbols are not defined.  Meson reads `.config` too (the board,
the theme, the languages, which sources build).

### The gamma tables

The four Gamma presets (1.8 / 2.0 / 2.2 / 2.4) are computed by default: per channel
`out = in ^ (preset / native)`, with the panel's native gamma per channel in menuconfig
(Board > Panel native gamma).

### The EDID

`tools/gen_edid.py` builds it from `boards/<board>/edid.json` - identity, size, sRGB
primaries, the modes the input can actually capture (hdmi_rx.c's mode table), LPCM stereo,
the HDMI vendor block - as EDID 1.3 (what HDMI requires) plus a CTA-861 extension, and runs
`edid-decode --check` on it when that is installed: a conformity failure stops the build.
The stock or a custom binary goes through with its checksums recomputed and, by default,
the HDMI deep-color flags cleared (the receiver has no deep-color path); a failed check is
only a warning there - the intouch156b's stock EDID fails it.

## Flash

```
meson configure build -Di2c_bus=6    # once: the monitor's DDC bus (or set RTD_I2C_BUS)
ninja -C build flash                 # assets + every bank + bank 0, one ISP session
ninja -C build flash-assets          # the asset partition only
```

`flash` writes the banks before bank 0 and everything in one ISP session (a reset in
between would boot a mix of two builds - docs/banking.md).  After running the stock
firmware, `tools/rtdflash.py protect` restores the flash write protection.

## How the build is laid out

| file | |
|---|---|
| `meson.build` | reads `.config`; the generators; one compile per source; link; image; flash / config targets |
| `core/meson.build`, `hal/realtek/<chip>/meson.build`, `boards/<board>/meson.build` | sources **in link order**, each with its bank (0 = common) |
| `Kconfig`, `configs/*_defconfig` | the configuration |
| `tools/kconfig.py` | defconfig / olddefconfig / menuconfig / guiconfig / savedefconfig / genconfig |
| `tools/build/sdcc_cc.py` | one SDCC compile + its depfile (`sdcc -MM`) |
| `tools/build/sdcc_link.py` | the two-step link (sdld without its -M check, which rejects banked code) |
| `tools/build/image.py` | check_banking.py, then mkimage.py: bank images, recovery path, XDATA end |

The bank of a source decides everything else: its `--codeseg/--constseg`, the linker's
`-Wl-bBANKn`, mkimage's bank images and the flash addresses (n x 64 KB).  Adding a file
to a bank is one line in the directory's meson.build.

**Link order matters**: internal RAM is full, and sdld places each module's direct-RAM
block in link order (docs/banking.md, "Direct RAM").  Keep new sources at the end of
their list unless there is a reason not to.

**Generated files are never in the source tree.**  Everything a tool produces goes to
`build/gen/` (`gen/meson.build`): config.h, theme.h, the rendered fonts / icons / logo,
lang_ids.h, assets.h + assets.bin, the EDID, the pipeline and display tables, version.txt.
The generators record their inputs, so editing a theme, an icon, a font or a language file
rebuilds exactly what depends on it.
