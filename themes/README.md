# Menu themes

The settings menu is drawn by `core/gui.c` in one of the themes built into the firmware,
switched at run time (OSD > Theme; the setting is saved).  A theme is one choice of each of
four parts:

| part | where | what it decides |
|---|---|---|
| icons | `icons/<set>/` | the category icons: artwork, 1-bit (one color) or 4-bit (multi-color) |
| colors | `colors/<name>.json` | every color: panel, header, text, selection, borders, icon colors |
| layout | `horizontal` or `vertical` | icons in a row under the header, or in a sidebar |
| frame | `frames/<name>.json` | panel border, header fill, dividers, selection style, icon highlight, translucency |
| font (optional) | `fonts/<name>/` | the menu's typeface; default `dejavu-bold` |

## Building themes in

```
ninja -C build menuconfig                  # Menu themes: which named themes (themes/<name>.json)
                                           # to build in, a custom theme with every part picked,
                                           # and the default (what a factory reset selects)
ninja -C build flash                       # writes code AND the asset partition (fonts and icons live there)
python3.14 tools/osdsim/osdsim_gui.py      # preview any theme / part / language without flashing
```

The custom theme's colors start from a base palette, and any of them can be replaced
(#RRGGBB per role - the roles are tools/themelib.py CHAR_ROLES / WINDOW_ROLES); on the command
line, `gen_theme.py --color ROLE=#RRGGBB` (repeatable), and in osdsim
`osdsim_cli.py render --color title=#FF4080 ...`.

`tools/gen_theme.py` resolves the themes into `build/<board>/gen/theme.h` - one descriptor
each: palette, frame style, layout, the window looks it precomputes - and their fonts and
icons, and says exactly what is missing or misspelled.  The menu lists the themes by their
`V_THEME_<NAME>` strings (lang/en.txt); the Information page shows the current one.  A named
theme needs a stable id: its place in `KNOWN` in tools/gen_theme.py (the settings record keeps
the id, so a build with other themes still finds it, or falls back to the default).

**How many fit**: the themes' fonts and icons share the 64 KB asset partition with the gamma
tables, the logo and the languages - the build stops if it overflows.  A font is 4.6 KB and an
icon set 6.5 KB (4-bit) or 1.6 KB (1-bit), each stored once however many themes use it; the
rest of a theme is ~120 bytes of code.  The four named themes share two fonts and two icon sets:
53.6 KB with all five languages, leaving room for about two more fonts or one icon set and a
font.

| theme | icons | colors | layout | frame |
|---|---|---|---|---|
| `midnight` (default) | flat | midnight | vertical | rounded (shadow, 16-px corners, pill selection) |
| `classic` | flat | classic | horizontal | bevel |
| `minimal` | line | light | horizontal | outline |
| `terminal` | line | terminal | vertical | double |

A theme file is just `{"description": "...", "icons": "...", "colors": "...", "layout": "...",
"frame": "..."}`.  The menu's size (1x / 2x) is a setting on the OSD page, not part of the theme.

## Icon sets

One SVG per category, 36 x 36 (3 x 2 character cells of 12 x 18): `picture`, `audio`, `display`,
`osd`, `system`, `info`, `exit` - and three for the message popups: `hdmi`, `usbc`, `nosignal`.  Draw in **placeholder colors**, listed in `set.json`, that
stand for icon roles; the color scheme gives the roles their real colors, so every icon set
works with every scheme:

```json
{"description": "...", "bpp": 4, "cells": [3, 2],
 "roles": {"#FF0000": "icon_main", "#00FF00": "icon_accent", "#0000FF": "icon_shade", "#FFFF00": "icon_hi"},
 "icons": ["picture", "audio", "display", "osd", "system", "info", "exit"]}
```

* `bpp: 4` - up to the four roles above, transparent elsewhere.
* `bpp: 1` - one color (`icon_main`; the selected category's icon is drawn in `icon_sel`).

There is no anti-aliasing: each pixel takes the role covering most of it.  The build renders
every set (`tools/gen_icons.py`, into `build/gen/icons_<set>.c`), so an SVG edit is picked up by
the next `ninja`.  To look at a set before flashing:

```
python3 tools/gen_icons.py themes/icons/<set> --out /tmp/icons.c --preview /tmp/preview.png
```

## Fonts

`fonts/<name>/` holds the TrueType/OpenType file itself (with its `LICENSE`), and
`font.json` names it, a pixel size and the baseline row (of the 18-row cell):

```json
{"description": "DejaVu Sans Bold, 14 px", "ttf": "DejaVuSans-Bold.ttf",
 "size": 14, "baseline": 13, "license": "...", "tracking": 0, "monospace": false}
```

The build renders every font with `tools/gen_font.py` (Pillow; hinted, 1-bit, one baseline)
to `build/gen/font_<name>.c` - glyphs plus each one's advance width; `--preview FILE.png`
draws sample text for review.  The
menu sets text proportionally (each OSD character cell has its own width, up to 12 px);
`monospace` keeps every cell 12 px.  Fonts: `dejavu-bold` (default), `dejavu`, `inter`,
`mono` (the terminal theme).  Check a new font's license allows redistribution (it goes
into the repository) and embedding.

## Color schemes

Every key is a `#RRGGBB` color:

| key | used for |
|---|---|
| `panel` | the color 4-bit icons and text sit on - keep it equal to `panel_fill` |
| `text`, `text_dim`, `text_sel`, `title`, `value` | item names, hints and previews, the selected item, the header title, values |
| `slider` (optional) | the adjustment slider; defaults to `sel_fill` |
| `icon_main`, `icon_accent`, `icon_shade`, `icon_hi`, `icon_sel` | icon roles (see above) |
| `panel_fill`, `footer_fill` | the menu body; the band behind the icons (`icon_strip`) |
| `header_fill`, `header_fill2` | the header; `header_fill2` gives a gradient's direction |
| `border`, `border_dark`, `shadow` | outline / 3D light edge, 3D dark edge, drop shadow |
| `divider`, `sel_fill`, `sel_border` | divider lines, the selection bar, outlines and the icon highlight |

## Frame styles

```json
{"description": "...", "panel_border": "bevel", "border_width": 3, "header": "solid",
 "divider": "bevel", "divider_width": 2, "selection": "sunken", "alpha": 0,
 "icon_select": "frame", "icon_strip": false}
```

| key | values |
|---|---|
| `panel_border` | `none`, `line`, `bevel` (raised 3D), `shadow` (drop shadow), `double` |
| `border_width` | 0-8 px |
| `header` | `solid`, `gradient` (steps from `header_fill` toward `header_fill2`) |
| `divider` | `none`, `line`, `bevel` (a groove) |
| `selection` | `bar`, `outline`, `sunken` |
| `icon_select` | `frame`, `brackets` (a bar each side), `left`, `right` |
| `icon_strip` | `true`: a solid band (`footer_fill`) behind the icons |
| `alpha` | 0 (opaque) - 31: panel translucency over the picture |
| `corner_radius` | 0 (square), 8, 16, 32: rounded panel and header (a rounded header is solid - no gradient; a bevel border is drawn as a line) |
| `rounded_selection` | `true`: the selection bar is a rounded pill |

## How it maps onto the hardware

The panel, header, dividers, selection and icon highlight are OSD windows (fill, border, 3D,
shadow, gradient and blend are all window features).  Text isi 1-bit glyphs; icons are
1 or 4-bit glyphs with transparent backgrounds.  Characters can only use palette entries 0-15,
so `tools/themelib.py` fixes which color role lives where; the window colors live in entries 16 and up.
