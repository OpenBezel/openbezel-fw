#!/usr/bin/env python3
"""gen_theme - resolve the build's menu themes into theme.h + glyph assets.

A theme names one of each part:
  icons   themes/icons/<set>/      (rendered by tools/gen_icons.py -> <glyphs-dir>/icons_<set>.c)
  colors  themes/colors/<name>.json
  layout  horizontal | vertical
  frame   themes/frames/<name>.json (borders, dividers, selection, header)
  font    themes/fonts/<name>/     (rendered by tools/gen_font.py -> <glyphs-dir>/font_<name>.c)

  gen_theme.py --themes midnight,classic --default midnight ...      named themes (themes/<name>.json)
  gen_theme.py --themes midnight,custom --custom-icons line --custom-colors light \\
               --custom-layout vertical --custom-frame bevel [--custom-font mono] [--color ROLE=#RRGGBB]

Every build carries the themes it names; the menu switches between them at run time
(core/gui.c reads the current one's descriptor).  Writes:
  theme.h      the descriptor struct, one descriptor per theme, the fonts' advance widths
  themes.bin   each font the themes use (text glyphs, bar glyphs, Latin-1) and each icon
               set, once - tools/gen_assets.py puts it in the asset partition as THEMES
  themes.json  per theme: font widths and body columns, for tools/gen_lang.py's checks
Each is written only when it changes, so a rebuild touches nothing.  Pure python: no Pillow.
"""
import argparse, json, math, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import themelib as tl
import depfile
from gen_assets import bar_glyph, BAR_GLYPHS, ASCII_COUNT, FONT_CHARS, OSD_GLYPHS

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "themes")
# Stable theme ids, kept in the settings record: append new named themes, never reorder.
KNOWN = ["midnight", "classic", "minimal", "terminal"]
CUSTOM_ID = 0x80
LAYOUTS = ("horizontal", "vertical")
# the menu's main page, in order - core/menu.c PAGE_* / page_main
ICON_ORDER = ["picture", "audio", "display", "osd", "system", "info", "exit"]
POPUP_ICONS = ["hdmi", "usbc", "nosignal"]          # the message popups (core/menu.c)
BORDERS = ("none", "line", "bevel", "shadow", "double")
HEADERS = ("solid", "gradient")
DIVIDERS = ("none", "line", "bevel")
SELECTIONS = ("bar", "outline", "sunken")
ICON_SELECTS = ("frame", "brackets", "left", "right")
FONT_GLYPHS = len(FONT_CHARS)                       # ASCII 0x20-0x7E, Latin-1 0xC0-0xFF
GLYPH = 27                                          # bytes per 1-bit 12x18 glyph
# window ADR3 byte 2 effect bits
FX_ON, FX_BLEND = 0x10, 0x80
FX_SHADOW1, FX_BEVEL_UP, FX_BEVEL_DOWN, FX_BORDER = FX_ON | 0, FX_ON | 8, FX_ON | 10, FX_ON | 14
# console rows / columns per layout (core/gui.c layouts[])
ROWS = {"horizontal": 19, "vertical": 17}
BODY_COLS = {"horizontal": 39, "vertical": 33}


def die(msg):
    sys.exit("gen_theme: " + msg)


def load(path, what):
    if not os.path.exists(path):
        have = sorted(os.path.splitext(f)[0] for f in os.listdir(os.path.dirname(path))
                      if f.endswith(".json") or os.path.isdir(os.path.join(os.path.dirname(path), f)))
        die("no %s %s (have: %s)" % (what, os.path.basename(path), ", ".join(have)))
    return json.load(open(path))


def rgb(s, role):
    m = re.fullmatch(r"#?([0-9A-Fa-f]{6})", s or "")
    if not m:
        die("color %s: %r is not #RRGGBB" % (role, s))
    v = int(m.group(1), 16)
    return (v >> 16, (v >> 8) & 0xFF, v & 0xFF)


def c_bytes(path):
    src = re.sub(r"/\*.*?\*/", "", open(path).read(), flags=re.S)
    body = src[src.index("{") + 1:src.rindex("}")]
    return bytes(int(t, 0) for t in re.findall(r"0x[0-9A-Fa-f]+", body))


def write_if_changed(path, data):
    mode = "wb" if isinstance(data, bytes) else "w"
    old = None
    if os.path.exists(path):
        old = open(path, "rb" if mode == "wb" else "r").read()
    if old != data:
        open(path, mode).write(data)
        return True
    return False


def round_table(R):
    """a corner-shape table: per line, the FILLED pixels counted from
    the corner's inner side; 8 and 16 px pack two lines a byte, 32 px one"""
    fill = [min(R - 1, R - int(round(R - math.sqrt(max(0, R * R - (R - y - 0.5) ** 2)))))
            for y in range(R)]
    if R == 32:
        return fill
    return [(fill[i + 1] << 4) | fill[i] for i in range(0, R, 2)]


def resolve(name, a):
    """-> the theme's parts"""
    if name == "custom":
        parts = {k: getattr(a, "custom_" + k) for k in ("icons", "colors", "layout", "frame", "font")}
        missing = [k for k in ("icons", "colors", "layout", "frame") if not parts[k]]
        if missing:
            die("the custom theme needs every part; missing: " + ", ".join(missing))
        if not parts["font"]:
            del parts["font"]
    else:
        parts = load(os.path.join(ROOT, name + ".json"), "theme")
        parts.pop("description", None)
    parts.setdefault("font", "dejavu-bold")         # optional part
    if parts["layout"] not in LAYOUTS:
        die("theme %s: layout %r, one of %s" % (name, parts["layout"], ", ".join(LAYOUTS)))
    return parts


def win(color, fx=0, eff=0, scol=0, bcol=0):
    return (tl.lut_index(color), fx, eff, tl.lut_index(scol) if scol else 0, tl.lut_index(bcol) if bcol else 0)


def style(name, parts, a):
    """-> the descriptor fields of one theme (core/gui.h struct gui_theme)"""
    colors = load(os.path.join(ROOT, "colors", parts["colors"] + ".json"), "color scheme")
    if name == "custom":
        roles = [r for r in tl.CHAR_ROLES + tl.WINDOW_ROLES if not r.startswith(("spare", "osd_"))]
        for spec in a.color:                         # custom colors over the palette
            role, _, value = spec.partition("=")
            if role not in roles:
                die("--color %s: no such role (one of %s)" % (role, ", ".join(roles)))
            rgb(value, role)
            colors[role] = value
    frame = load(os.path.join(ROOT, "frames", parts["frame"] + ".json"), "frame style")
    for key, allowed in (("panel_border", BORDERS), ("header", HEADERS), ("divider", DIVIDERS),
                         ("selection", SELECTIONS)):
        if frame.get(key) not in allowed:
            die("frame %s: %s = %r, one of %s" % (parts["frame"], key, frame.get(key), ", ".join(allowed)))
    isel = frame.get("icon_select", "frame")
    if isel not in ICON_SELECTS:
        die("frame %s: icon_select = %r, one of %s" % (parts["frame"], isel, ", ".join(ICON_SELECTS)))
    radius = int(frame.get("corner_radius", 0))
    if radius not in (0, 8, 16, 32):
        die("frame %s: corner_radius 0, 8, 16 or 32" % parts["frame"])
    pill = bool(frame.get("rounded_selection", False))
    bw, alpha = int(frame.get("border_width", 0)), int(frame.get("alpha", 0))
    if not 0 <= bw <= 8 or not 0 <= alpha <= 31:
        die("frame %s: border_width 0-8, alpha 0-31" % parts["frame"])
    border = frame["panel_border"]
    blend = FX_BLEND if alpha else 0

    pal = []
    for role in tl.CHAR_ROLES + tl.WINDOW_ROLES:
        if role.startswith("spare"):
            pal.append((0, 0, 0))
        elif role.startswith("osd_"):
            pal.append({"osd_orange": (0xFF, 0xB0, 0x20), "osd_dark": (0x10, 0x10, 0x18),
                        "osd_white": (0xFF, 0xFF, 0xFF)}[role])
        elif role == "slider" and role not in colors:
            pal.append(rgb(colors["sel_fill"], "sel_fill"))     # optional: the selection color
        elif role not in colors:
            die("color scheme %s has no %r" % (parts["colors"], role))
        else:
            pal.append(rgb(colors[role], role))

    # the square panel window and the double border's inner one (rounded panels: gui.c)
    panel = {"none": win("panel_fill", blend),
             "line": win("panel_fill", FX_BORDER | blend, bw, bcol="border"),
             "double": win("panel_fill", FX_BORDER | blend, bw, bcol="border"),
             "bevel": win("panel_fill", FX_BEVEL_UP | blend, bw, "border", "border_dark"),
             "shadow": win("panel_fill", FX_SHADOW1 | blend, bw, "shadow")}[border]
    inner = win("panel_fill", FX_BORDER, 1, bcol="border") if border == "double" else (0, 0, 0, 0, 0)
    div = {"none": (0, 0, 0, 0, 0), "line": win("divider"),
           "bevel": win("divider", FX_BEVEL_DOWN, 1, "border_dark", "border")}[frame["divider"]]
    sel = {"bar": win("sel_fill"), "outline": win("panel_fill", FX_BORDER, 1, bcol="sel_border"),
           "sunken": win("sel_fill", FX_BEVEL_DOWN, 2, "border_dark", "border")}[frame["selection"]]
    # header gradient (window ADR0 byte 2): per channel, enabled where the two header
    # colors differ, counting up where the second is brighter.  The hardware steps from
    # the fill color at a fixed rate - it does not stop at header_fill2.
    f1, f2 = rgb(colors["header_fill"], "header_fill"), rgb(colors["header_fill2"], "header_fill2")
    en = pol = 0
    for i, bit in enumerate((4, 2, 1)):             # R, G, B enable bits [2:0]
        if f1[i] != f2[i]:
            en |= bit
            if f2[i] > f1[i]:
                pol |= bit                          # polarity bits [7:5] in the same order
    if border in ("none", "shadow"):
        inset = 0
    elif border == "double":
        inset = bw + 3
    else:
        inset = bw
    return {"border": BORDERS.index(border), "border_w": bw, "inset": inset,
            "gradient": int(frame["header"] == "gradient"), "grad0": (pol << 5) | en,
            "divider": int(frame["divider"] != "none"), "outline_sel": int(frame["selection"] == "outline"),
            "icon_select": ICON_SELECTS.index(isel), "icon_strip": int(bool(frame.get("icon_strip", False))),
            "alpha": alpha, "round_code": {0: 0xFF, 8: 0, 16: 1, 32: 2}[radius], "sel_round": int(pill),
            "panel": panel, "inner": inner, "div": div, "sel": sel, "palette": pal}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--themes", default="midnight", help="comma-separated: named themes and/or custom")
    p.add_argument("--default", help="the theme a factory reset selects (default: the first)")
    for k in ("icons", "colors", "layout", "frame", "font"):
        p.add_argument("--custom-" + k, default="", help="the custom theme's " + k)
    p.add_argument("--color", action="append", default=[], metavar="ROLE=#RRGGBB",
                   help="replace one color of the custom theme's palette (repeatable; roles: tools/themelib.py)")
    p.add_argument("--glyphs-dir", required=True,
                   help="where the build put icons_<set>.c and font_<name>.c (gen_icons.py, gen_font.py)")
    p.add_argument("--out-header", required=True)
    p.add_argument("--out-bin", required=True)
    p.add_argument("--out-json", required=True)
    p.add_argument("--depfile", help="write a Make-style depfile of every input read (Ninja)")
    a = p.parse_args()
    depfile.track()

    names = [n.strip() for n in a.themes.split(",") if n.strip()]
    if not names or len(set(names)) != len(names):
        die("--themes: a list of distinct themes")
    default = a.default or names[0]
    if default not in names:
        die("--default %s is not one of the themes built in (%s)" % (default, ", ".join(names)))
    for n in names:
        if n != "custom" and n not in KNOWN:
            die("theme %s has no id: add it to KNOWN in tools/gen_theme.py" % n)

    blob = bytearray()
    fonts, icon_sets = {}, {}                        # name -> (index, offset)
    font_widths = []
    themes = []
    # the default theme first, so its font starts the blob (assets.h ASSET_OSD_FONT_OFF: the
    # font the debug console and the logo screen load - core/main.c, osd.c)
    for n in [default] + [x for x in names if x != default]:
        parts = resolve(n, a)
        # font: ASCII, the slider's bar glyphs, Latin-1 - the order osd/gui index by
        if parts["font"] not in fonts:
            load(os.path.join(ROOT, "fonts", parts["font"], "font.json"), "font")
            fsrc = re.sub(r"/\*.*?\*/", "", open(os.path.join(a.glyphs_dir, "font_%s.c" % parts["font"])).read(),
                          flags=re.S)
            m = re.search(r"font_glyphs\[\d+\]\s*=\s*\{(.*?)\};", fsrc, re.S)
            fg = bytes(int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]+)", m.group(1)))
            m = re.search(r"font_widths\[\d+\]\s*=\s*\{(.*?)\};", fsrc, re.S)
            fw = [int(t) for t in re.findall(r"\d+", m.group(1))]
            if len(fg) != FONT_GLYPHS * GLYPH or len(fw) != FONT_GLYPHS:
                die("font %s: font_%s.c is not %d glyphs" % (parts["font"], parts["font"], FONT_GLYPHS))
            data = fg[:ASCII_COUNT * GLYPH] + b"".join(bytes(bar_glyph(k)) for k in range(BAR_GLYPHS)) + fg[ASCII_COUNT * GLYPH:]
            fonts[parts["font"]] = (len(fonts), len(blob))
            font_widths.append(fw)
            blob += data
        # icons, in the menu's order
        if parts["icons"] not in icon_sets:
            setdir = os.path.join(ROOT, "icons", parts["icons"])
            iset = load(os.path.join(setdir, "set.json"), "icon set")
            if sorted(iset["icons"]) != sorted(ICON_ORDER + POPUP_ICONS):
                die("icon set %s: needs exactly %s" % (parts["icons"], ", ".join(ICON_ORDER + POPUP_ICONS)))
            glyphs = c_bytes(os.path.join(a.glyphs_dir, "icons_%s.c" % parts["icons"]))
            cw, ch = iset["cells"]
            bpp = iset["bpp"]
            per_icon = cw * ch * GLYPH * (4 if bpp == 4 else 1)
            if len(glyphs) != per_icon * len(iset["icons"]):
                die("icon set %s: icons_%s.c does not match set.json" % (parts["icons"], parts["icons"]))
            data = b"".join(glyphs[iset["icons"].index(i) * per_icon:(iset["icons"].index(i) + 1) * per_icon]
                            for i in ICON_ORDER + POPUP_ICONS)
            # a character command's glyph index is 8 bits: the icons follow the text font
            words = 36 if bpp == 4 else 9
            idx0 = (OSD_GLYPHS * 9 + words - 1) // words + 1
            if idx0 + len(data) // (words * 3) > 256:
                die("icon set %s: its glyphs pass index 255 after the text font" % parts["icons"])
            icon_sets[parts["icons"]] = (bpp, cw, ch, len(blob), len(data))
            blob += data
        st = style(n, parts, a)
        bpp, cw, ch, ioff, ilen = icon_sets[parts["icons"]]
        st.update({"name": n, "parts": parts, "id": CUSTOM_ID if n == "custom" else KNOWN.index(n),
                   "vertical": int(parts["layout"] == "vertical"), "icon_bpp": bpp, "icon_cw": cw,
                   "icon_ch": ch, "icons_off": ioff, "icons_len": ilen,
                   "font": fonts[parts["font"]][0], "font_off": fonts[parts["font"]][1]})
        themes.append(st)
    themes.sort(key=lambda t: names.index(t["name"]))
    cws = {(t["icon_cw"], t["icon_ch"]) for t in themes}
    if len(cws) != 1:
        die("every icon set in a build must have the same cells (have %s)" % sorted(cws))

    labels = ["S_V_THEME_%s" % t["name"].upper() for t in themes]
    h = ["/* generated by tools/gen_theme.py - do not edit */",
         "/* themes: %s */" % "; ".join("%s = icons %s, colors %s, layout %s, frame %s, font %s"
                                         % ((t["name"],) + tuple(t["parts"][k] for k in
                                            ("icons", "colors", "layout", "frame", "font")))
                                         for t in themes),
         "#ifndef OB_THEME_H", "#define OB_THEME_H", "",
         "#define GUI_THEME_COUNT %d" % len(themes),
         "#define GUI_THEME_DEFAULT %d          /* index of the factory-reset theme */" % names.index(default),
         "#define GUI_THEME_DEFAULT_ID %d" % themes[names.index(default)]["id"],
         "#define GUI_THEME_LABELS { %s }      /* lang/en.txt V_THEME_* */" % ", ".join(labels),
         "#define GUI_FONT_COUNT %d" % len(fonts),
         "#define GUI_ICON_CW %d" % themes[0]["icon_cw"], "#define GUI_ICON_CH %d" % themes[0]["icon_ch"],
         "#define GUI_ICON_COUNT %d          /* the menu's; the popup icons follow */" % len(ICON_ORDER)]
    for i, n in enumerate(POPUP_ICONS):
        h.append("#define GUI_ICON_%s %d" % (n.upper(), len(ICON_ORDER) + i))
    h.append("")
    for i, s in enumerate(BORDERS):
        h.append("#define GUI_BORDER_%s %d" % (s.upper(), i))
    for i, s in enumerate(ICON_SELECTS):
        h.append("#define GUI_ISEL_%s %d" % (s.upper(), i))
    h += ["", "/* corner-shape tables, for every size a theme may use */",
          "#define GUI_ROUND8_TABLE { %s }" % ", ".join("0x%02X" % b for b in round_table(8)),
          "#define GUI_ROUND16_TABLE { %s }" % ", ".join("0x%02X" % b for b in round_table(16)),
          "#define GUI_ROUND32_TABLE { %s }" % ", ".join("0x%02X" % b for b in round_table(32)), ""]
    for role in tl.CHAR_ROLES + tl.WINDOW_ROLES:
        if not role.startswith("spare"):
            h.append("#define GUI_C_%s %d" % (role.upper(), tl.lut_index(role)))
    npal = len(themes[0]["palette"])
    h += ["#define GUI_PALETTE_N %d" % npal, "",
          "/* one window's look: fill color, effect (ADR3 byte 2), effect width, shadow and border",
          " * colors - core/gui.c win_put() */",
          "struct gui_win { u8 color, fx, eff, scol, bcol; };",
          "",
          "/* one theme (core/gui.c) */",
          "struct gui_theme {",
          "    u8 id;                      /* stable: kept in the settings record */",
          "    u8 vertical;                /* layout: icon sidebar, else icons in a row */",
          "    u8 icon_bpp;                /* 1 or 4 */",
          "    u8 font;                    /* index into GUI_FONT_WIDTHS */",
          "    u16 font_off;               /* in the THEMES asset: text glyphs, bars, Latin-1 */",
          "    u16 icons_off, icons_len;   /* ... and the icons, the menu's then the popups' */",
          "    u8 border;                  /* GUI_BORDER_*: rounded panels are built from it */",
          "    u8 border_w;                /* border / shadow width, px */",
          "    u8 inset;                   /* how far the border reaches in: the text grid starts below */",
          "    u8 gradient, grad0;         /* header gradient, and its ADR0 byte 2 */",
          "    u8 divider;                 /* dividers drawn */",
          "    u8 outline_sel;             /* an outline selection: its text stays in the text color */",
          "    u8 icon_select;             /* GUI_ISEL_* */",
          "    u8 icon_strip;              /* a solid band behind the icons */",
          "    u8 alpha;                   /* panel translucency 0-31 */",
          "    u8 round_code;              /* largest corner size code (0 8 px, 1 16, 2 32), 0xFF square */",
          "    u8 sel_round;               /* a rounded (8 px) selection pill */",
          "    struct gui_win panel, inner, div, sel;   /* square panel, double border's inner, dividers, selection */",
          "    u8 palette[GUI_PALETTE_N][3];",
          "};", ""]

    def winc(w):
        return "{ %d, 0x%02X, %d, %d, %d }" % w
    h.append("#define GUI_THEMES { \\")
    for t in themes:
        h.append("    { %d, %d, %d, %d, 0x%04X, 0x%04X, 0x%04X, %d, %d, %d, %d, 0x%02X, %d, %d, %d, %d, %d, 0x%02X, %d, \\"
                 % (t["id"], t["vertical"], t["icon_bpp"], t["font"], t["font_off"], t["icons_off"],
                    t["icons_len"], t["border"], t["border_w"], t["inset"], t["gradient"], t["grad0"],
                    t["divider"], t["outline_sel"], t["icon_select"], t["icon_strip"], t["alpha"],
                    t["round_code"], t["sel_round"]))
        h.append("      %s, %s, %s, %s, \\" % (winc(t["panel"]), winc(t["inner"]), winc(t["div"]), winc(t["sel"])))
        h.append("      { %s } }, \\" % ", ".join("{ 0x%02X, 0x%02X, 0x%02X }" % c for c in t["palette"]))
    h += ["}", "",
          "/* each font's advance widths, ASCII 0x20-0x7E then Latin-1 0xC0-0xFF (cell widths, 1-12 px) */",
          "#define GUI_FONT_WIDTHS { \\"]
    h += ["    { %s }, \\" % ", ".join(str(w) for w in fw) for fw in font_widths]
    h += ["}", "", "#endif", ""]

    summary = {"themes": [{"name": t["name"], "body_cols": BODY_COLS[t["parts"]["layout"]],
                           "font_widths": font_widths[t["font"]]} for t in themes]}
    os.makedirs(os.path.dirname(a.out_header) or ".", exist_ok=True)
    write_if_changed(a.out_bin, bytes(blob))
    write_if_changed(a.out_json, json.dumps(summary, indent=1) + "\n")
    if write_if_changed(a.out_header, "\n".join(h)):
        for t in themes:
            print("theme %s%s: icons %s, colors %s, layout %s, frame %s, font %s"
                  % ((t["name"], " (default)" if t["name"] == default else "") +
                     tuple(t["parts"][k] for k in ("icons", "colors", "layout", "frame", "font"))))
        print("theme assets: %d fonts, %d icon sets, %d bytes" % (len(fonts), len(icon_sets), len(blob)))
    depfile.write(a.depfile, [a.out_header, a.out_bin, a.out_json])


if __name__ == "__main__":
    main()
