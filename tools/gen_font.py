#!/usr/bin/env python3
"""gen_font - render a theme font (themes/fonts/<name>/font.json) to OSD glyphs.

Run by the build (gen/meson.build; needs Pillow): writes the font as C - 159 glyphs (ASCII 0x20-0x7E, then the Latin-1 letters
0xC0-0xFF for the menu's translations, lang/) in the OSD's 12 x 18, 1-bit format plus each
glyph's advance width - and, with --preview, a PNG of sample text.  Not 0xA0-0xBF (symbols): a glyph index is 8 bits,
and with them the 1-bit icon themes' icons would run past 255.

Glyphs are drawn at their real pixel size in 1-bit mode, so FreeType's hinting puts the
stems on the pixel grid, and all of them sit on one baseline.  The advance width becomes the character cell's width (the OSD takes a
width per cell): the menu sets text proportionally.

font.json: {"ttf": font file name (installed, or next to font.json), "size": px, "baseline": row (0-17), "description": ...,
            "license": ..., optional "bold": 0/1 (embolden by 1 px),
            optional "monospace": true (every advance 12)}
Usage: gen_font.py themes/fonts/<name> --out font_<name>.c [--preview preview.png]
"""
import argparse, json, os, sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depfile  # noqa: E402
import themelib as tl
from gen_assets import FONT_CHARS  # noqa: E402

W, H = tl.CELL_W, tl.CELL_H
CHARS = FONT_CHARS
COUNT = len(CHARS)
SAMPLES = ["Ä Ö Ü ä ö ü ß  É È Ê À Â Ç Î Ô Û  é è ê à â ç î ô û ë ï",
           "Ñ ñ Á Í Ó Ú á í ó ú  Ì Ò ì ò  Ø ø Å å Æ æ ÿ",
           "Helligkeit  Farbtemperatur  Sättigung  Schärfe",
           "Luminosité  Température  Énergie  Paramètres",
           "Configuración  Señal  Información  Ítem",
"Picture  Brightness  [ +12]", "Color temp  Warm   Gamma 2.2",
           "Input  USB-C  1920x1080 60.0Hz", "The quick brown fox jumps over",
           "the lazy dog.  0123456789 %/()?"]


FONT_DIRS = ["/usr/share/fonts", "/usr/local/share/fonts", os.path.expanduser("~/.local/share/fonts")]


def find_font(name, base):
    """The font file: next to font.json, or installed (fonts-dejavu-core, fonts-inter - the
    build container has them, docker/Dockerfile)."""
    for d in [base] + FONT_DIRS:
        if os.path.isfile(os.path.join(d, name)):
            return os.path.join(d, name)
        for root, _, files in os.walk(d):
            if name in files:
                return os.path.join(root, name)
    sys.exit("gen_font: %s not found in %s (install the font, or build in the container: "
             "tools/docker-run)" % (name, ", ".join(FONT_DIRS)))


def render(meta, base):
    path = find_font(meta["ttf"], base)
    open(path, "rb").close()                 # into the depfile: a font package update re-renders
    font = ImageFont.truetype(path, meta["size"])
    glyphs, widths, rows_all = bytearray(), [], {}
    clipped = []
    TOP = 6                                  # headroom: ink above the cell = clipped
    for c in CHARS:
        ch = chr(c)
        img = Image.new("1", (W + 8, H + 2 * TOP), 0)
        d = ImageDraw.Draw(img)
        d.fontmode = "1"                     # no anti-aliasing: hinted 1-bit glyphs
        adv = W if meta.get("monospace") else int(round(font.getlength(ch) + meta.get("tracking", 0))) + int(meta.get("bold", 0))
        adv = max(1, min(W, adv))
        # A cell narrower than 12 px shows the LEFT `adv` columns: FRAME_CTRL_02
        # disp_zone = left (osd_init); its reset value centers them.
        x0 = 1 if meta.get("monospace") else 0
        d.text((x0, meta["baseline"] + TOP), ch, font=font, fill=1, anchor="ls")
        if meta.get("bold"):
            d.text((x0 + 1, meta["baseline"] + TOP), ch, font=font, fill=1, anchor="ls")
        if meta.get("monospace"):           # center the ink in the 12-px cell
            bb = img.getbbox()
            if bb:
                ink = bb[2] - bb[0]
                shift = (W - ink) // 2 - bb[0]
                img = img.transform(img.size, Image.AFFINE, (1, 0, -shift, 0, 1, 0))
        px = img.load()
        if any(px[x, y] for x in range(W) for y in list(range(TOP)) + list(range(H + TOP, H + 2 * TOP))):
            clipped.append(ch)
        rows = [[1 if px[x, y + TOP] else 0 for x in range(W)] for y in range(H)]
        rows_all[ch] = (rows, adv)
        glyphs += tl.pack_cell(rows, 1)
        widths.append(adv)
    if clipped:
        print("note: clipped by the 18-row cell: %s" % " ".join(clipped))
    return glyphs, widths, rows_all


def preview(rows_all, path, zoom=3, mono=False):
    lines = SAMPLES
    wpx = max(sum(rows_all[c][1] for c in s) for s in lines) + 8
    img = Image.new("RGB", (wpx * zoom, (len(lines) * (H + 2) + 4) * zoom), (28, 32, 40))
    y = 2
    for s in lines:
        x = 4
        for c in s:
            rows, adv = rows_all[c]
            for ry in range(H):
                for rx in range(adv):
                    if rows[ry][rx]:
                        for dy in range(zoom):
                            for dx in range(zoom):
                                img.putpixel(((x + rx) * zoom + dx, (y + ry) * zoom + dy), (230, 232, 238))
            x += adv
        y += H + 2
    img.save(path)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("fontdir")
    p.add_argument("--out", required=True, help="the C file")
    p.add_argument("--preview", help="also render sample text to this PNG")
    p.add_argument("--depfile")
    a = p.parse_args()
    depfile.track()
    meta = json.load(open(os.path.join(a.fontdir, "font.json")))
    glyphs, widths, rows_all = render(meta, a.fontdir)
    name = os.path.basename(os.path.normpath(a.fontdir))
    with open(a.out, "w") as f:
        f.write("/* generated by tools/gen_font.py from themes/fonts/%s - do not edit */\n" % name)
        f.write("/* %s, %d px, baseline row %d.  License: %s */\n"
                % (os.path.basename(meta["ttf"]), meta["size"], meta["baseline"], meta.get("license", "?")))
        f.write("const unsigned char font_glyphs[%d] = {\n" % len(glyphs))
        for i in range(0, len(glyphs), 27):
            c = CHARS[i // 27]
            f.write("    /* %s */ %s,\n" % ("0x%02X" % c if c >= 0x80 else repr(chr(c)).replace("*/", "* /"),
                                          ", ".join("0x%02X" % b for b in glyphs[i:i + 27])))
        f.write("};\n")
        f.write("const unsigned char font_widths[%d] = {\n    %s\n};\n"
                % (len(widths), ", ".join(str(w) for w in widths)))
    if a.preview:
        preview(rows_all, a.preview)
    depfile.write(a.depfile, [a.out] + ([a.preview] if a.preview else []))


if __name__ == "__main__":
    main()
