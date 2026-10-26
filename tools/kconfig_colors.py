"""guiconfig extension: the custom theme colors (THEME_COLOR_* string symbols, #RRGGBB)
get a color swatch on their line and a color picker instead of a text entry.

kconfiglib stays unmodified: install() wraps three of guiconfig's module
functions - the row image, the row text and the value dialog - for those symbols only.
An empty value means "the base palette's color"; the swatch then shows that color and
the line says so."""
import json, os, re

THEMES = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "themes")
PREFIX = "THEME_COLOR_"
HEX = re.compile(r"#?([0-9A-Fa-f]{6})")


def _is_color(sym):
    return getattr(sym, "name", None) and sym.name.startswith(PREFIX)


def _norm(v):
    m = HEX.fullmatch(v.strip()) if v else None
    return "#" + m.group(1).upper() if m else None


def install(guiconfig, kconf):
    import tkinter as tk
    from tkinter import colorchooser

    images = {}                                  # tag -> PhotoImage (kept referenced)

    def palette_color(sym):
        """the base palette's color for this role (what an empty value means)"""
        role = sym.name[len(PREFIX):].lower()
        name = kconf.syms["THEME_COLORS"].str_value if "THEME_COLORS" in kconf.syms else ""
        if not name:
            theme = kconf.syms["THEME"].str_value if "THEME" in kconf.syms else "midnight"
            try:
                name = json.load(open(os.path.join(THEMES, theme + ".json"))).get("colors", "midnight")
            except OSError:
                name = "midnight"
        try:
            pal = json.load(open(os.path.join(THEMES, "colors", name + ".json")))
        except OSError:
            return None
        return _norm(pal.get(role) or (pal.get("sel_fill") if role == "slider" else ""))

    def effective(sym):
        return _norm(sym.str_value) or palette_color(sym)

    def swatch_tag(color):
        tag = "swatch-" + (color[1:] if color else "none")
        if tag not in images:
            img = tk.PhotoImage(width=16, height=16)
            img.put(color or "#FFFFFF", to=(0, 0, 16, 16))
            if not color:                        # unknown: a cross
                for i in range(16):
                    img.put("#C00000", to=(i, i, i + 1, i + 1))
                    img.put("#C00000", to=(15 - i, i, 16 - i, i + 1))
            for x0, y0, x1, y1 in ((0, 0, 16, 1), (0, 15, 16, 16), (0, 0, 1, 16), (15, 0, 16, 16)):
                img.put("#202020", to=(x0, y0, x1, y1))
            images[tag] = img
            guiconfig._tree.tag_configure(tag, image=img)
        return tag

    orig_img_tag = guiconfig._img_tag
    orig_node_str = guiconfig._node_str
    orig_dialog = guiconfig._set_val_dialog

    def img_tag(node):
        if _is_color(node.item):
            return swatch_tag(effective(node.item))
        return orig_img_tag(node)

    def node_str(node):
        s = orig_node_str(node)
        sym = node.item
        if _is_color(sym) and not sym.str_value:
            pc = palette_color(sym)
            if s.endswith(": "):                 # "prompt: " + the empty value
                s = s[:-2]
            s += "  (palette%s)" % (" " + pc if pc else "")
        return s

    def dialog(node, parent):
        sym = node.item
        if not _is_color(sym):
            return orig_dialog(node, parent)
        result = {"value": None}
        d = tk.Toplevel(parent)
        d.title(node.prompt[0] if node.prompt else sym.name)
        d.transient(parent)
        d.resizable(False, False)
        cur = effective(sym)
        tk.Label(d, text=(node.prompt[0] if node.prompt else sym.name), anchor="w").grid(
            row=0, column=0, columnspan=3, sticky="w", padx=8, pady=(8, 4))
        sw = tk.Label(d, width=8, height=2, relief="sunken", bg=cur or "#FFFFFF")
        sw.grid(row=1, column=0, rowspan=2, padx=8, pady=4)
        tk.Label(d, text=(sym.str_value or "(base palette: %s)" % (cur or "?"))).grid(
            row=1, column=1, columnspan=2, sticky="w")

        def pick():
            rgb, hexval = colorchooser.askcolor(color=cur or "#808080", parent=d,
                                                title=node.prompt[0] if node.prompt else sym.name)
            if hexval:
                result["value"] = _norm(hexval)
                d.destroy()

        def clear():
            result["value"] = ""
            d.destroy()

        tk.Button(d, text="Pick color...", command=pick).grid(row=2, column=1, sticky="ew", padx=2)
        tk.Button(d, text="Use palette color", command=clear).grid(row=2, column=2, sticky="ew", padx=2)
        tk.Button(d, text="Cancel", command=d.destroy).grid(row=3, column=2, sticky="ew", padx=2, pady=(4, 8))
        d.bind("<Escape>", lambda e: d.destroy())
        # Modal - once the window manager has mapped it: grab_set() on a window that is
        # not viewable yet raises ("grab failed: window not viewable", seen on KDE),
        # which would abort the edit and leave the dialog behind with nothing to apply
        # its answer.  A grab that still fails only costs modality.
        try:
            d.wait_visibility()
            d.grab_set()
        except tk.TclError:
            pass
        d.focus_set()
        d.wait_window()
        return result["value"]

    guiconfig._img_tag = img_tag
    guiconfig._node_str = node_str
    guiconfig._set_val_dialog = dialog
