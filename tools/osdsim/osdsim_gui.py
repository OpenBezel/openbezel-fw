#!/usr/bin/env python3
"""osdsim GUI - the OpenBezel OSD, live, with virtual front-panel buttons.

Left: theme and its parts, language, menu size, slider style, position, input and
signal, the background to view it over.  Dialog buttons bring a screen up from a
reset; UP / DOWN / ENTER / POWER (or the arrow keys, Return, P) drive the
firmware's own menu code from there.  Every change rebuilds the view by
replaying: reset, settings, the dialog, then the keys pressed since.

Needs Pillow (with ImageTk), numpy, tkinter and gcc.
"""
import os, sys, tkinter as tk
from tkinter import ttk, filedialog, messagebox

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw, ImageTk
from osdsim import build as B
from osdsim.dialogs import DIALOGS, show
from osdsim.signal import parse_signal
from osdsim.sim import Sim

THEME_DEFAULT = "(theme)"
BACKGROUNDS = ("checker", "photo", "black", "gray", "white")


def background(kind, size):
    w, h = size
    if kind == "checker":
        img = Image.new("RGB", size, (200, 200, 200))
        d = ImageDraw.Draw(img)
        for y in range(0, h, 32):
            for x in range(0, w, 32):
                if (x // 32 + y // 32) % 2:
                    d.rectangle((x, y, x + 31, y + 31), fill=(150, 150, 150))
        return img
    if kind == "photo":                          # a stand-in picture: sky, sun, hills
        img = Image.new("RGB", size)
        d = ImageDraw.Draw(img)
        for y in range(h):
            t = y / h
            d.line((0, y, w, y), fill=(int(60 + 120 * t), int(120 + 80 * t), int(200 - 60 * t)))
        d.ellipse((w * 0.7, h * 0.12, w * 0.82, h * 0.33), fill=(255, 220, 120))
        d.polygon([(0, h), (0, h * 0.7), (w * 0.3, h * 0.55), (w * 0.6, h * 0.72), (w, h * 0.6), (w, h)],
                  fill=(40, 110, 50))
        return img
    return Image.new("RGB", size, {"black": (0, 0, 0), "gray": (128, 128, 128), "white": (255, 255, 255)}[kind])


class App:
    def __init__(self, root):
        self.root = root
        root.title("OpenBezel OSD simulator")
        self.sim = None
        self.sim_key = None
        self.dialog = "menu"
        self.keys = []
        self.frame = None
        self._bg_cache = {}

        left = ttk.Frame(root, padding=8)
        left.pack(side=tk.LEFT, fill=tk.Y)
        right = ttk.Frame(root, padding=8)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.v = {}
        row = 0

        def label(text):
            nonlocal row
            ttk.Label(left, text=text, font=("TkDefaultFont", 9, "bold")).grid(row=row, column=0, columnspan=2,
                                                                             sticky="w", pady=(8, 2))
            row += 1

        def combo(name, text, values, default):
            nonlocal row
            ttk.Label(left, text=text).grid(row=row, column=0, sticky="w")
            var = tk.StringVar(value=default)
            cb = ttk.Combobox(left, textvariable=var, values=values, state="readonly", width=14)
            cb.grid(row=row, column=1, sticky="ew")
            cb.bind("<<ComboboxSelected>>", lambda e: self.refresh())
            self.v[name] = var
            row += 1

        def radios(name, text, values, default):
            nonlocal row
            ttk.Label(left, text=text).grid(row=row, column=0, sticky="w")
            f = ttk.Frame(left)
            f.grid(row=row, column=1, sticky="w")
            var = tk.StringVar(value=default)
            for val in values:
                ttk.Radiobutton(f, text=val, value=val, variable=var, command=self.refresh).pack(side=tk.LEFT)
            self.v[name] = var
            row += 1

        label("Theme")
        combo("theme", "theme", B.themes(), "midnight")
        for k in B.PARTS:
            combo(k, k, [THEME_DEFAULT] + B.parts(k), THEME_DEFAULT)
        label("Runtime")
        langs = ["en", "de", "fr", "es", "it"]
        langs += [l for l in B.languages() if l not in langs]
        self.langs = langs
        combo("lang", "language", langs, "en")
        radios("scale", "menu size", ("1x", "2x"), "1x")
        radios("slider", "slider", ("row", "popup"), "row")
        for name, text in (("osd_h", "H position"), ("osd_v", "V position")):
            ttk.Label(left, text=text).grid(row=row, column=0, sticky="w")
            var = tk.IntVar(value=50)
            ttk.Scale(left, from_=0, to=100, variable=var, orient=tk.HORIZONTAL,
                      command=lambda e: self.refresh()).grid(row=row, column=1, sticky="ew")
            self.v[name] = var
            row += 1
        label("Source")
        radios("input", "input", ("hdmi", "usbc"), "hdmi")
        ttk.Label(left, text="signal").grid(row=row, column=0, sticky="w")
        self.v["signal"] = tk.StringVar(value="1920x1080@60")
        e = ttk.Entry(left, textvariable=self.v["signal"], width=16)
        e.grid(row=row, column=1, sticky="ew")
        e.bind("<Return>", lambda ev: self.refresh())
        row += 1
        ttk.Label(left, text="(WxH@Hz or none)", foreground="gray").grid(row=row, column=1, sticky="w")
        row += 1

        label("Dialog (from a reset)")
        df = ttk.Frame(left)
        df.grid(row=row, column=0, columnspan=2, sticky="w")
        for i, name in enumerate(DIALOGS):
            ttk.Button(df, text=name, width=9, command=lambda n=name: self.set_dialog(n)).grid(
                row=i // 3, column=i % 3, padx=1, pady=1)
        row += 1

        label("Front panel")
        kf = ttk.Frame(left)
        kf.grid(row=row, column=0, columnspan=2)
        for i, k in enumerate(("up", "down", "enter", "power")):
            ttk.Button(kf, text=k.upper(), width=7, command=lambda k=k: self.press(k)).grid(row=0, column=i, padx=1)
        ttk.Button(kf, text="wait 30 s (timeouts)", command=self.wait).grid(row=1, column=0, columnspan=4,
                                                                           sticky="ew", pady=2)
        row += 1

        label("View")
        combo("bg", "background", BACKGROUNDS, "photo")
        radios("zoom", "zoom", ("panel", "osd 1:1", "osd 2:1"), "panel")
        sf = ttk.Frame(left)
        sf.grid(row=row, column=0, columnspan=2, pady=6)
        ttk.Button(sf, text="Save OSD PNG", command=lambda: self.save(False)).pack(side=tk.LEFT, padx=2)
        ttk.Button(sf, text="Save panel PNG", command=lambda: self.save(True)).pack(side=tk.LEFT, padx=2)
        row += 1
        self.status = tk.StringVar()
        ttk.Label(left, textvariable=self.status, foreground="gray", wraplength=260).grid(
            row=row, column=0, columnspan=2, sticky="w", pady=4)

        self.canvas = tk.Canvas(right, width=960, height=540, background="#222", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda e: self.draw())
        for key, name in (("<Up>", "up"), ("<Down>", "down"), ("<Return>", "enter"), ("p", "power"),
                          ("<Escape>", "power")):
            root.bind(key, lambda e, n=name: self.press(n))
        self.refresh()

    # ---- model -------------------------------------------------------------------
    def config(self):
        parts = {k: self.v[k].get() for k in B.PARTS if self.v[k].get() != THEME_DEFAULT}
        return B.Config(theme=self.v["theme"].get(), langs=self.langs, **parts)

    def replay(self):
        cfg = self.config()
        if self.sim is None or cfg.key() != self.sim_key:
            self.status.set("building %s ..." % cfg.theme)
            self.root.update_idletasks()
            try:
                self.sim = Sim(cfg)
            except Exception as ex:                     # a theme that does not build
                messagebox.showerror("osdsim", str(ex))
                return False
            self.sim_key = cfg.key()
        s = self.sim
        s.reset()
        try:
            s.set_signal(input=self.v["input"].get(), **parse_signal(self.v["signal"].get().strip() or "none"))
        except ValueError:
            self.status.set("signal: WxH@Hz or none")
        s.set("language", self.v["lang"].get())
        s.set("osd_scale", 1 if self.v["scale"].get() == "2x" else 0)
        s.set("adjust_popup", 1 if self.v["slider"].get() == "popup" else 0)
        s.set("osd_h", int(self.v["osd_h"].get()))
        s.set("osd_v", int(self.v["osd_v"].get()))
        show(s, self.dialog)
        for k in self.keys:
            if k == "wait":
                s.tick(30000)
            else:
                s.key(k)
        return True

    def refresh(self):
        if self.replay():
            self.frame = self.sim.render()
            self.status.set("%s%s  -  OSD %dx%d at %s%s" % (
                self.dialog, (" + " + ",".join(self.keys)) if self.keys else "",
                self.frame.image.width, self.frame.image.height, self.frame.pos,
                "" if self.frame.enabled else "  (OSD off)"))
            self.draw()

    def set_dialog(self, name):
        self.dialog = name
        self.keys = []
        self.refresh()

    def press(self, k):
        self.keys.append(k)
        self.refresh()

    def wait(self):
        self.keys.append("wait")
        self.refresh()

    # ---- view --------------------------------------------------------------------
    def draw(self):
        if not self.frame:
            return
        cw, ch = max(1, self.canvas.winfo_width()), max(1, self.canvas.winfo_height())
        zoom = self.v["zoom"].get()
        bgk = self.v["bg"].get()
        if zoom == "panel":
            W, H = self.sim.chip.PANEL
            key = (bgk, W, H)
            if key not in self._bg_cache:
                self._bg_cache[key] = background(bgk, (W, H))
            img = self.sim.panel_image(self.frame, self._bg_cache[key])
            f = min(cw / W, ch / H)
            img = img.resize((max(1, int(W * f)), max(1, int(H * f))), Image.LANCZOS)
        else:
            osd = self.frame.image
            pad = 24
            base = background(bgk, (osd.width + 2 * pad, osd.height + 2 * pad)).convert("RGBA")
            if self.frame.enabled:
                base.alpha_composite(osd, (pad, pad))
            img = base
            if zoom == "osd 2:1":
                img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
        self.tkimg = ImageTk.PhotoImage(img)
        self.canvas.delete("all")
        self.canvas.create_image(cw // 2, ch // 2, image=self.tkimg)

    def save(self, panel):
        if not self.frame:
            return
        path = filedialog.asksaveasfilename(defaultextension=".png", filetypes=[("PNG", "*.png")],
                                            initialfile=self.dialog + (".panel" if panel else "") + ".png")
        if not path:
            return
        (self.sim.panel_image(self.frame) if panel else self.frame.image).save(path)
        self.status.set("saved " + path)


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
