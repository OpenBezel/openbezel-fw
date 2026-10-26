"""themelib - conventions shared by gen_icons.py and gen_theme.py.

The OSD palette is 64 entries.  Characters can only reach entries 0-15 (1-bit cells take
fg/bg nibbles, 4-bit glyph pixels ARE the entry number), windows reach all 64.  So the
low 16 hold every color text or an icon pixel can need, at FIXED positions - an icon set
is rendered once and works with every color scheme - and the window colors live above.
"""

# Entries 0-15: characters.  0 is the transparency key for 1-bit backgrounds AND the
# color of 4-bit value-0 pixels (drawn opaque): the theme sets it to the panel color,
# so both read as "panel".
CHAR_ROLES = [
    "panel",            # 0
    "text",             # 1  item names
    "text_dim",         # 2  hints, disabled
    "text_sel",         # 3  the selected item
    "title",            # 4  page title (on the header)
    "value",            # 5  item values
    "icon_main",        # 6
    "icon_accent",      # 7
    "icon_shade",       # 8
    "icon_hi",          # 9
    "icon_sel",         # 10 1-bit icons: the selected category's icon color
    "osd_orange",       # 11 the firmware's existing popups/console keep their colors
    "osd_dark",         # 12
    "osd_white",        # 13
    "slider",           # 14 adjustment slider (bar glyphs); defaults to sel_fill
    "spare15",          # 15
]

# Entries 16+: windows only.
WINDOW_ROLES = [
    "panel_fill",       # 16 the menu body
    "header_fill",      # 17 header band (gradient start)
    "header_fill2",     # 18 gradient end, when the frame style asks for one
    "border",           # 19 outer border / 3D light edge
    "border_dark",      # 20 3D dark edge
    "shadow",           # 21 drop shadow
    "divider",          # 22 lines between header / body / footer
    "sel_fill",         # 23 selection bar
    "sel_border",       # 24 selection outline / icon frame
    "footer_fill",      # 25
]
WINDOW_BASE = 16

ICON_ROLES = ["icon_main", "icon_accent", "icon_shade", "icon_hi"]


def lut_index(role):
    if role in CHAR_ROLES:
        return CHAR_ROLES.index(role)
    return WINDOW_BASE + WINDOW_ROLES.index(role)


CELL_W, CELL_H = 12, 18


def pack_plane(rows, bit):
    """rows: CELL_H lists of CELL_W pixel values -> 9 words (3 bytes each) of plane `bit`:
    two rows per 24-bit word, top row in bits 23:12, MSB = leftmost, bytes low first."""
    out = bytearray()
    for w in range(CELL_H // 2):
        val = 0
        for r in (2 * w, 2 * w + 1):
            bits = 0
            for c in range(CELL_W):
                if (rows[r][c] >> bit) & 1:
                    bits |= 1 << (CELL_W - 1 - c)
            val = (val << 12) | bits
        out += bytes((val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF))
    return out


def pack_cell(rows, bpp):
    """One 12x18 cell -> glyph bytes: 1-bit (any non-zero pixel set) or 4-bit planar."""
    if bpp == 1:
        return pack_plane([[1 if p else 0 for p in r] for r in rows], 0)
    return b"".join(pack_plane(rows, b) for b in range(4))
