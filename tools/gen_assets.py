#!/usr/bin/env python3
"""gen_assets - build the flash asset partition and its offset header.

Large tables do not belong in the 64 KB code bank.
They live in the asset partition at 0x020000 and are
streamed into their destination port by the burst engine, which reads flash
directly with no CPU involvement - the same mechanism stock uses.

Emits:
  assets.bin      raw partition image, flashed at ASSET_BASE
  assets.h        compile-time offsets/lengths for the firmware

The firmware cannot read this partition itself (its code window is the first
64 KB), so there is no runtime magic check.  The
header is written anyway so a host can identify a dumped partition.

Usage: gen_assets.py --gamma-native-x100 213,213,213 --logo logo.c --blob THEMES=themes.bin \
                     --out-bin build/gen/assets.bin --out-header build/gen/assets.h
"""
import argparse, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depfile

ASSET_BASE = 0x020000
MAGIC = b"OBAS"

# gamma: pointer table bank4 0x829C, presets 1..4, 3 channels x 0x804 each
GAMMA_CH, GAMMA_N = 0x804, 4
GAMMA_TARGETS = (1.8, 2.0, 2.2, 2.4)    # presets 1..4 (core/mccs.c gamma_mccs, the menu's labels)
GAMMA_FULL = 0x3FF0                     # 14-bit full scale, as stock

# OSD character command: B0 = 1bpp | glyph width, B1 = glyph index, B2 = fg<<4 | bg.
# Precomputing the stream here turns two __code arrays (tilemap + palette) into
# one burstable blob - slightly larger in flash, but out of the code bank and
# with no per-cell CPU work at draw time.
CHAR_CMD_1BPP, OSD_FONT_W, OSD_COL_TRANSPARENT = 0x40, 12, 0
# logo.c's palette indices (gen_logo.py: 1 orange, 7 white) -> osd.h OSD_COL_ORANGE /
# OSD_COL_WHITE, which moved to 11 / 13 to leave 0-15 to the menu theme
LOGO_FG_MAP = {1: 11, 7: 13}


def c_array(path, name):
    """-> bytes of a generated C array, by its declared name."""
    src = open(path).read()
    m = re.search(r"\b" + re.escape(name) + r"\s*(?:\[[^\]]*\]\s*)+=\s*\{", src)
    if not m:
        raise SystemExit("%s: array %s not found" % (path, name))
    depth, i = 1, m.end()
    while depth:
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
        i += 1
    body = src[m.end():i - 1]
    # generated files annotate rows with comments like /* 'A' */ or /* glyph 12 */;
    # their digits are not data
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    return bytes(int(t, 0) for t in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", body))


def gamma_computed(target, native):
    """One preset, computed: per channel out = in ^ (target / native[ch]) - the shape of
    stock's tables, which fit this within 2.2 % (red) / 1 % (green, blue) with the
    intouch156b panel's native R/G/B gammas 2.39 / 2.13 / 2.07 (fitted to stock's tables).
    Layout as stock: per channel 1025 big-endian words for inputs i/1023 (the last
    two both full scale) and a zero word; channels R, G, B."""
    out = bytearray()
    for nat in native:
        e = target / nat
        v = [int(round(GAMMA_FULL * (min(i, 1023) / 1023.0) ** e)) for i in range(1025)] + [0]
        for w in v:
            out += bytes((w >> 8, w & 0xFF))
    assert len(out) == 3 * GAMMA_CH
    return bytes(out)


# Bar-gauge glyphs, appended to the text font as characters 0x7F + k: a
# framed trough (rows 3 and 14 across the whole cell) with its left k of 12
# columns filled (rows 5..12).  k = 0..12, so a gauge of N cells has 12*N
# steps - the menu's edit mode (core/menu.c).  Same packing as fontgen.py: two
# rows per 24-bit word, top row in bits 23:12, MSB = leftmost, bytes low..high.
BAR_W, BAR_H = 12, 18
BAR_GLYPHS = BAR_W + 1


def bar_glyph(k):
    def row(r):
        if r in (3, 14):
            return (1 << BAR_W) - 1
        if 5 <= r <= 12:
            return ((1 << k) - 1) << (BAR_W - k) if k else 0
        return 0
    out = []
    for w in range(BAR_H // 2):
        val = (row(2 * w) << 12) | row(2 * w + 1)
        out += [val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF]
    return out


# the OSD character set: ASCII 0x20-0x7E, then (after the bar glyphs) the Latin-1 letters 0xC0-0xFF
ASCII_FIRST, ASCII_COUNT = 0x20, 0x7F - 0x20
LATIN1_FIRST, LATIN1_COUNT = 0xC0, 0x100 - 0xC0
FONT_CHARS = list(range(ASCII_FIRST, ASCII_FIRST + ASCII_COUNT)) + list(range(LATIN1_FIRST, LATIN1_FIRST + LATIN1_COUNT))
OSD_GLYPHS = ASCII_COUNT + BAR_GLYPHS + LATIN1_COUNT   # a theme font as gen_theme.py lays it out
ASSET_PARTITION_MAX = 0x10000     # 0x020000 - 0x02FFFF; bank 3 code from 0x030000


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--gamma-native", metavar="R,G,B",
                   help="compute the gamma LUTs for a panel with these native per-channel gammas "
                        "(intouch156b: 2.39,2.13,2.07)")
    p.add_argument("--gamma-native-x100", metavar="R,G,B", help="the same, x 100 (239,213,207)")
    p.add_argument("--no-gamma", action="store_true",
                   help="zero-filled gamma LUTs of the same size, so every offset matches a real "
                        "build (tools/osdsim)")
    p.add_argument("--logo", required=True, help="logo.c (gen_logo.py)")
    p.add_argument("--out-bin", required=True)
    p.add_argument("--out-header", required=True)
    p.add_argument("--blob", action="append", default=[], metavar="NAME=FILE",
                   help="append a prebuilt blob as ASSET_<NAME> (THEMES from gen_theme.py, LANG from gen_lang.py)")
    p.add_argument("--depfile", help="write a Make-style depfile of every input read (Ninja)")
    a = p.parse_args()
    depfile.track()
    if a.gamma_native_x100:
        a.gamma_native = ",".join(str(int(v) / 100.0) for v in a.gamma_native_x100.split(","))
    if sum(map(bool, (a.gamma_native, a.no_gamma))) != 1:
        p.error("exactly one of --gamma-native(-x100), --no-gamma")
    native = [float(v) for v in a.gamma_native.split(",")] if a.gamma_native else None
    if native and (len(native) != 3 or not all(1.0 <= v <= 4.0 for v in native)):
        p.error("--gamma-native: three gammas R,G,B between 1 and 4")

    blob = bytearray(MAGIC + struct.pack("<BBH", 1, GAMMA_N, 0))   # 8-byte header
    entries = []
    for n in range(1, GAMMA_N + 1):
        if native:
            g = gamma_computed(GAMMA_TARGETS[n - 1], native)
        else:
            g = bytes(3 * GAMMA_CH)
        entries.append(("GAMMA_%d" % n, len(blob), len(g)))
        blob += g

    # --- OSD: the logo's glyphs and its character-command stream, burst into font SRAM ---
    logo_c = a.logo
    glyphs = c_array(logo_c, "logo_glyphs")
    tilemap = c_array(logo_c, "logo_tilemap")
    tile_fg = c_array(logo_c, "logo_tile_fg")
    if len(glyphs) % 27 or len(tilemap) != len(tile_fg):
        raise SystemExit("logo.c: glyph/tilemap sizes inconsistent")
    entries.append(("LOGO_GLYPHS", len(blob), len(glyphs)))
    blob += glyphs

    cmds = bytearray()
    for cell in range(len(tilemap)):
        fg = LOGO_FG_MAP.get(tile_fg[cell], tile_fg[cell])
        cmds += bytes([CHAR_CMD_1BPP | OSD_FONT_W, tilemap[cell],
                       ((fg << 4) | OSD_COL_TRANSPARENT) & 0xFF])
    entries.append(("LOGO_CMDS", len(blob), len(cmds)))
    blob += cmds
    print("  logo %d glyphs, %d cells precomputed" % (len(glyphs) // 27, len(tilemap)))

    for spec in a.blob:
        name, path = spec.split("=", 1)
        data = open(path, "rb").read()
        entries.append((name, len(blob), len(data)))
        blob += data
    themes_off = [off for name, off, ln in entries if name == "THEMES"]
    if len(themes_off) != 1:
        raise SystemExit("gen_assets: needs exactly one --blob THEMES=... (gen_theme.py)")

    # The partition ends where code bank 3 begins (flash 0x030000)
    if len(blob) > ASSET_PARTITION_MAX:
        raise SystemExit("asset partition %d bytes > %d: it would overwrite code bank 3 - "
                         "build in fewer menu themes or languages (menuconfig)"
                         % (len(blob), ASSET_PARTITION_MAX))
    open(a.out_bin, "wb").write(bytes(blob))
    with open(a.out_header, "w") as f:
        f.write("/* generated by tools/gen_assets.py - do not edit */\n")
        f.write("#ifndef OB_ASSETS_H\n#define OB_ASSETS_H\n\n")
        f.write("/* Offsets are relative to the asset partition base; the burst engine is given\n"
                " * ASSET_BASE + offset as a 24-bit flash address. */\n")
        f.write("#define ASSET_BASE 0x%06lXUL\n" % ASSET_BASE)
        f.write("#define ASSET_PARTITION_LEN %d\n\n" % len(blob))
        for name, off, ln in entries:
            f.write("#define ASSET_%s_OFF 0x%04X\n" % (name, off))
            f.write("#define ASSET_%s_LEN 0x%04X\n" % (name, ln))
        f.write("\n#define ASSET_GAMMA_CH_LEN 0x%04X   /* per channel; 3 back to back */\n" % GAMMA_CH)
        f.write("#define ASSET_GAMMA_COUNT %d\n" % GAMMA_N)
        f.write("\n/* a theme font (gen_theme.py): ASCII 0x20-0x7E, the bar glyphs, Latin-1 0xC0-0xFF;\n"
                " * the default theme's is first in THEMES - the debug console and the logo screen's */\n")
        f.write("#define ASSET_OSD_GLYPHS %d\n" % OSD_GLYPHS)
        f.write("#define ASSET_OSD_FONT_OFF ASSET_THEMES_OFF\n")
        f.write("#define ASSET_OSD_FONT_LEN 0x%04X\n" % (OSD_GLYPHS * 27))
        f.write("#define ASSET_OSD_ASCII_FIRST 0x%02X   /* first character with a glyph (glyph 0) */\n" % ASCII_FIRST)
        f.write("#define ASSET_OSD_ASCII_COUNT %d      /* 0x20-0x7E */\n" % ASCII_COUNT)
        f.write("#define ASSET_OSD_LATIN1_FIRST 0x%02X  /* the Latin-1 letters 0xC0-0xFF */\n" % LATIN1_FIRST)
        f.write("#define ASSET_OSD_LATIN1_COUNT %d\n" % LATIN1_COUNT)
        f.write("#define ASSET_OSD_CHAR_BAR0 0x%02X      /* + k: a cell with k of %d columns filled */\n"
                % (ASCII_FIRST + ASCII_COUNT, BAR_W))
        f.write("#define ASSET_OSD_CHAR_END 0x%02X     /* chars 0x20 - END-1: glyph c - 0x20 */\n"
                % (ASCII_FIRST + ASCII_COUNT + BAR_GLYPHS))
        f.write("#define ASSET_OSD_LATIN1 %d           /* glyph of char 0xC0 (0xC0-0xFF follow) */\n"
                % (ASCII_COUNT + BAR_GLYPHS))
        f.write("\n#endif\n")
    print("wrote %s (%d bytes) and %s" % (a.out_bin, len(blob), a.out_header))
    depfile.write(a.depfile, [a.out_bin, a.out_header])
    for name, off, ln in entries:
        print("  %-10s offset 0x%04X  len 0x%04X  (flash 0x%06X)" % (name, off, ln, ASSET_BASE + off))


if __name__ == "__main__":
    main()
