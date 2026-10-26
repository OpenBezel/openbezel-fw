# osdsim - OSD simulator

Renders OpenBezel's on-screen display (the menu, its pages, the edit slider, the
hotkey bars, the mode and "No signal" popups) on the host, in any theme and language,
as PNGs or live in a GUI with virtual front-panel buttons.

```
python3.14 tools/osdsim/osdsim_gui.py                      # the GUI
python3.14 tools/osdsim/osdsim_cli.py list                 # themes, parts, languages, dialogs, settings
python3.14 tools/osdsim/osdsim_cli.py render --dialog picture --lang de -o p.png
python3.14 tools/osdsim/osdsim_cli.py render --theme classic --frame rounded --scale 2 \
        --dialog adjust --slider popup --panel -o adj.png
python3.14 tools/osdsim/osdsim_cli.py all --theme terminal --lang fr -o out/   # every dialog
python3.14 tools/osdsim/osdsim_cli.py render --themes midnight,classic --set theme=1 \
    --dialog osd -o p.png       # several themes in one build, switched by the theme setting
python3.14 tools/osdsim/osdsim_cli.py render --defconfig build/.config --color title=#FF4080 -o t.png
```

The Kconfig features and default settings come from `--defconfig` (a configs/ file or a
build directory's `.config`; default configs/<board>_defconfig), compiled in exactly as in
the firmware build.

Needs Pillow, numpy, tkinter (GUI), rsvg-convert, the theme fonts and a host C compiler
(`gcc`, or `$CC`) - the build container has them all: `tools/docker-run python3
tools/osdsim/osdsim_cli.py ...` (the GUI needs an X server, DISPLAY).

## What the PNG is

The OSD plane as the panel blends it: opaque windows and characters, blended windows at
the theme's blend level (alpha = 1 - level/32), transparent everywhere else (LUT 0 as a
character background, cut-off rounded corners, outside the windows).  By default the
image is the OSD alone, from its origin; `--panel` places it on the whole 1920x1080
panel where the firmware puts it; `--background #rrggbb` or `--background photo.jpg`
flattens it onto a picture.

## How it works

It does not re-implement the menu.  The firmware's own `core/menu.c`, `gui.c`,
`console.c`, `lang.c`, `picmode.c` and `settings.c` are compiled with the host compiler
(`host/include/host_sdcc.h` turns SDCC's `__xdata`/`__code`/`__banked`/... into nothing,
`host/include/sreg.h` makes the scaler registers a 64 KB array) against stubs of the
chip's OSD HAL that **record** what the firmware writes - font SRAM, window SRAM, the
color LUT, the origin, the frame-control registers.  A per-chip Python model then turns
that state into pixels.  Layout, text, colors, the theme and the language all come from
the same code and generated tables as the real build, so a picture here is what the
monitor shows - and a layout bug here is one on the monitor (the Information page's
two-color values were found this way).

```
osdsim/build.py      Config (board, theme + part overrides, LANGS) -> build/osdsim/<key>/:
                     gen_theme, gen_lang, gen_assets --no-gamma, gcc -> libosdsim.so
osdsim/sim.py        ctypes wrapper: reset, keys, popups, settings by name, render()
osdsim/dialogs.py    each named dialog as the key presses / source events that open it
osdsim/signal.py     "1920x1080@60" -> totals for the Information page and popups
osdsim/chips/        chip models (registry in __init__.py; BOARDS maps a board to a chip)
host/sim_api.c       the C API, assets behind flash_read(), time, the input signal
host/chips/<chip>/   the chip's OSD HAL as recorders + no-op stubs for the rest of its HAL
```

A rebuild happens only when an input (firmware sources, themes, lang, tools) is newer
than the cached library; a configuration builds in well under a second.

## The RTD2556 model (`osdsim/chips/rtd2556.py`)

Windows 0-9 and the WIN4_x sub-windows (drawn over every main window), fill, border,
drop shadow, raised/sunken 3D, vertical/horizontal gradient, blend, rounded corners
from the 3A60-3A8B tables; the 10-px window offset; row and character commands,
1-bit glyphs at their cell width with disp_zone, 4-bit glyphs with f4b_color_sel;
2x doubling (origin included).  Approximations: the gradient's rate
(~12.8 LUT levels a line, slowed by repeat steps and level) is fitted to the probing
notes, not measured exactly; 2-bit glyphs, grid lines, highlight windows, chessboard and
256-level gradients are not modeled because the firmware does not use them.

## Adding a chip

1. `host/chips/<chip>/`: the chip's OSD HAL functions, recording into exported arrays,
   and no-op stubs for whatever else the GUI code links against (`gcc -Wl,--no-undefined`
   lists them).
2. `osdsim/chips/<chip>.py`: `NAME`, `PANEL`, `HOST_SOURCES`, `FIRMWARE_SOURCES`,
   `INCLUDE_DIRS`, `STATE_ARRAYS` (exported as `osdsim_<name>`) and `render(state)` ->
   `Frame`.  Register it in `chips/__init__.py` with its board(s).
3. Anything chip-independent (settings, keys, signal) stays in `host/sim_api.c`.
