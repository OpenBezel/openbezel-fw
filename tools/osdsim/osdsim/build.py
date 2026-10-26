"""Build (and cache) the host library for one configuration: board, theme parts,
languages.  Runs the firmware's own generators (gen_theme, gen_lang, gen_assets)
into build/osdsim/<key>/ and compiles the firmware GUI code there with the chip's
host stubs."""
import hashlib, json, os, subprocess, sys

from .chips import BOARDS, chip_for

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
HOST = os.path.join(REPO, "tools", "osdsim", "host")
THEMES = os.path.join(REPO, "themes")
PARTS = ("icons", "colors", "layout", "frame", "font")
CC = os.environ.get("CC", "gcc")


def themes():
    return sorted(f[:-5] for f in os.listdir(THEMES) if f.endswith(".json"))


def parts(kind):
    """available choices for a theme part"""
    if kind == "layout":
        return ["horizontal", "vertical"]           # built into gen_theme.py (LAYOUTS)
    d = {"icons": "icons", "colors": "colors", "layout": "layouts", "frame": "frames", "font": "fonts"}[kind]
    path = os.path.join(THEMES, d)
    if not os.path.isdir(path):
        path = os.path.join(THEMES, kind)
    out = []
    for f in sorted(os.listdir(path)) if os.path.isdir(path) else []:
        out.append(f[:-5] if f.endswith(".json") else f)
    return out


def languages():
    return sorted(f[:-4] for f in os.listdir(os.path.join(REPO, "lang")) if f.endswith(".txt"))


def theme_parts(theme):
    return json.load(open(os.path.join(THEMES, theme + ".json")))


class Config:
    def __init__(self, board="intouch156b", theme="midnight", langs=None, color_overrides=None,
                 defconfig=None, themes=None, **overrides):
        self.board = board
        # the firmware's Kconfig features / defaults (config.h): this defconfig or .config,
        # else configs/<board>_defconfig
        self.defconfig = os.path.abspath(defconfig) if defconfig else os.path.join(REPO, "configs", board + "_defconfig")
        self.theme = theme
        self.themes = list(themes or [])            # several named themes, switched at run time
        self.langs = list(langs or ["en", "de", "fr", "es", "it"])
        self.parts = {k: v for k, v in overrides.items() if k in PARTS and v}
        self.colors = dict(color_overrides or {})   # role -> "#RRGGBB" over the palette

    def key(self):
        s = json.dumps([self.board, self.theme, self.themes, self.langs, sorted(self.parts.items()),
                        sorted(self.colors.items()), os.path.abspath(self.defconfig)])
        return hashlib.sha1(s.encode()).hexdigest()[:12]

    def theme_args(self):
        """one theme: a named one as it is, else gen_theme.py's custom theme - the named
        theme's parts with the swapped ones (and any colors) over them; or several named
        themes, the setting "theme" (their ids, tools/gen_theme.py KNOWN) picking one"""
        if self.themes:
            return ["--themes", ",".join(self.themes)]
        if self.theme != "custom" and not self.parts and not self.colors:
            return ["--themes", self.theme]
        parts = {} if self.theme == "custom" else theme_parts(self.theme)
        parts.update(self.parts)
        a = ["--themes", "custom"]
        for k in PARTS:
            if parts.get(k):
                a += ["--custom-" + k, parts[k]]
        for role, v in sorted(self.colors.items()):
            a += ["--color", "%s=%s" % (role, v)]
        return a


def _run(cmd):
    r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("%s failed:\n%s%s" % (" ".join(cmd[:3]), r.stdout, r.stderr))


def _newest(paths):
    t = 0
    for p in paths:
        if os.path.isdir(p):
            for root, _, files in os.walk(p):
                for f in files:
                    t = max(t, os.path.getmtime(os.path.join(root, f)))
        elif os.path.exists(p):
            t = max(t, os.path.getmtime(p))
    return t


def _glyphs(py, tools):
    """Fonts, icon sets and the logo, rendered as in the firmware build (gen/meson.build),
    into build/osdsim/glyphs/ - shared by every configuration, redone when a source is newer."""
    gdir = os.path.join(REPO, "build", "osdsim", "glyphs")
    os.makedirs(gdir, exist_ok=True)
    jobs = []
    for kind, tool, marker in (("fonts", "gen_font.py", "font.json"), ("icons", "gen_icons.py", "set.json")):
        base = os.path.join(THEMES, kind)
        for name in sorted(os.listdir(base)):
            src = os.path.join(base, name)
            if os.path.isfile(os.path.join(src, marker)):
                outc = os.path.join(gdir, "%s_%s.c" % ("font" if kind == "fonts" else "icons", name))
                jobs.append(([src, os.path.join(tools, tool)], outc, [py, os.path.join(tools, tool), src, "--out", outc]))
    svg = os.path.join(REPO, "images", "openbezel_wordmark.svg")
    logo = os.path.join(gdir, "logo.c")
    jobs.append(([svg, os.path.join(tools, "gen_logo.py")], logo,
                 [py, os.path.join(tools, "gen_logo.py"), "--svg", svg, "--out", logo]))
    for srcs, outc, cmd in jobs:
        if not os.path.exists(outc) or os.path.getmtime(outc) < _newest(srcs):
            _run(cmd)
    return gdir


def build(cfg, verbose=False):
    """-> (library path, assets path); rebuilt only when an input is newer"""
    chip = chip_for(cfg.board)
    bdir = os.path.join(REPO, BOARDS[cfg.board]["dir"])
    out = os.path.join(REPO, "build", "osdsim", cfg.key())
    os.makedirs(out, exist_ok=True)
    lib = os.path.join(out, "libosdsim.so")
    assets = os.path.join(out, "assets.bin")
    srcs = [os.path.join(REPO, s) for s in chip.FIRMWARE_SOURCES] + \
           [os.path.join(HOST, s) for s in chip.HOST_SOURCES] + [os.path.join(HOST, "sim_api.c")]
    inputs = srcs + [HOST, THEMES, os.path.join(REPO, "lang"), os.path.join(REPO, "core"),
                     os.path.join(REPO, "hal"), bdir, os.path.join(REPO, "tools"),
                     os.path.join(REPO, "Kconfig"), cfg.defconfig]
    if os.path.exists(lib) and os.path.getmtime(lib) >= _newest(inputs):
        return lib, assets
    py = sys.executable
    tools = os.path.join(REPO, "tools")
    gdir = _glyphs(py, tools)
    _run([py, os.path.join(tools, "gen_theme.py")] + cfg.theme_args() +
         ["--glyphs-dir", gdir, "--out-header", os.path.join(out, "theme.h"), "--out-bin", os.path.join(out, "themes.bin"),
          "--out-json", os.path.join(out, "themes.json")])
    _run([py, os.path.join(tools, "gen_lang.py"), "--lang-dir", "lang", "--langs"] + cfg.langs +
         ["--themes-json", os.path.join(out, "themes.json"), "--out-header", os.path.join(out, "lang_ids.h"),
          "--out-bin", os.path.join(out, "lang.bin")])
    _run([py, os.path.join(tools, "gen_assets.py"), "--no-gamma", "--logo", os.path.join(gdir, "logo.c"),
          "--blob", "THEMES=" + os.path.join(out, "themes.bin"),
          "--blob", "LANG=" + os.path.join(out, "lang.bin"),
          "--out-bin", assets, "--out-header", os.path.join(out, "assets.h")])
    kc = [py, os.path.join(tools, "kconfig.py"), "--config", os.path.join(out, ".config")]
    _run(kc + ["defconfig", cfg.defconfig])
    _run(kc + ["genconfig", os.path.join(out, "config.h")])
    cmd = [CC, "-shared", "-fPIC", "-O1", "-Wall", "-Wno-unused", "-o", lib,
           "-include", os.path.join(HOST, "include", "host_sdcc.h"),
           "-include", os.path.join(out, "config.h"),          # as the firmware build: Kconfig
           "-I" + os.path.join(HOST, "include"), "-I" + HOST, "-I" + out, "-I" + gdir] + \
          ["-I" + os.path.join(REPO, d) for d in chip.INCLUDE_DIRS] + ["-I" + bdir] + srcs + ["-Wl,--no-undefined"]
    if verbose:
        print(" ".join(cmd))
    _run(cmd)
    return lib, assets
