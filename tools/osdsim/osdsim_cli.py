#!/usr/bin/env python3
"""osdsim - render OpenBezel OSD screens to PNG, from the firmware's own menu code.

  osdsim_cli.py render --dialog menu -o menu.png
  osdsim_cli.py render --theme classic --lang de --dialog picture --keys down,down -o p.png
  osdsim_cli.py render --dialog adjust --slider popup --scale 2 --panel -o adj.png
  osdsim_cli.py all -o out/ --theme minimal --lang fr          # every dialog
  osdsim_cli.py list                                          # themes, parts, languages, settings

PNG alpha is what the panel blends: opaque OSD, translucent (blended) windows at
their blend level, transparent elsewhere.  --panel writes the whole panel with the
OSD where it sits on screen; --background flattens onto a color or an image.
Needs Pillow + numpy and a host C compiler (gcc).
"""
import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from osdsim import build as B
from osdsim.dialogs import DIALOGS, show
from osdsim.signal import parse_signal
from osdsim.sim import Sim


def add_config_args(p):
    g = p.add_argument_group("build (theme / languages)")
    g.add_argument("--board", default="intouch156b")
    g.add_argument("--theme", default="midnight", help="a theme from themes/*.json")
    g.add_argument("--themes", help="several named themes in one build, comma separated "
                   "(pick one with --set theme=ID; ids: tools/gen_theme.py KNOWN)")
    for k in B.PARTS:
        g.add_argument("--" + k, help="override the theme's %s part" % k)
    g.add_argument("--color", action="append", default=[], metavar="ROLE=#RRGGBB",
                   help="replace one palette color (repeatable; roles: tools/themelib.py)")
    g.add_argument("--langs", default="en,de,fr,es,it", help="the build's LANGS, comma separated")
    g.add_argument("--defconfig", help="features / default settings from this defconfig or a build's "
                   ".config (default: configs/<board>_defconfig)")
    r = p.add_argument_group("runtime (settings)")
    r.add_argument("--lang", help="menu language (a code from --langs)")
    r.add_argument("--scale", type=int, choices=(1, 2), help="Menu size 1x / 2x")
    r.add_argument("--slider", choices=("row", "popup"), help="edit slider in the row or as a popup")
    r.add_argument("--pos", help="menu position H,V in %% (e.g. 50,50)")
    r.add_argument("--set", action="append", default=[], metavar="NAME=VALUE",
                   help="any setting (osdsim_cli.py list shows them); repeatable")
    s = p.add_argument_group("source")
    s.add_argument("--input", choices=("hdmi", "usbc"), default="hdmi")
    s.add_argument("--signal", default="1920x1080@60",
                   help="WxH@Hz[/HTOTALxVTOTAL] or 'none'")


def make_sim(a):
    cfg = B.Config(board=a.board, theme=a.theme, langs=a.langs.split(","),
                   themes=a.themes.split(",") if a.themes else None,
                   color_overrides=dict(c.split("=", 1) for c in a.color), defconfig=a.defconfig,
                   **{k: getattr(a, k) for k in B.PARTS})
    sim = Sim(cfg)
    apply_runtime(sim, a)
    return sim


def apply_runtime(sim, a):
    sim.reset()
    sim.set_signal(input=a.input, **parse_signal(a.signal))
    if a.lang:
        sim.set("language", a.lang)
    if a.scale:
        sim.set("osd_scale", a.scale - 1)
    if a.slider:
        sim.set("adjust_popup", 1 if a.slider == "popup" else 0)
    if a.pos:
        h, v = (int(x) for x in a.pos.split(","))
        sim.set("osd_h", h)
        sim.set("osd_v", v)
    for kv in a.set:
        k, _, v = kv.partition("=")
        sim.set(k, int(v, 0))


def write(sim, a, path):
    frame = sim.render()
    bg = None
    if a.background:
        if a.background.startswith("#"):
            bg = tuple(int(a.background[i:i + 2], 16) for i in (1, 3, 5))
        else:
            from PIL import Image
            bg = Image.open(a.background)
    if a.panel or bg is not None:
        img = sim.panel_image(frame, bg)
    else:
        img = frame.image
    img.save(path)
    print("%s: %dx%d%s" % (path, img.width, img.height, "" if frame.enabled else " (OSD off)"))


def cmd_render(a):
    sim = make_sim(a)
    show(sim, a.dialog)
    for k in filter(None, (a.keys or "").split(",")):
        sim.key(k)
    write(sim, a, a.output)


def cmd_all(a):
    os.makedirs(a.output, exist_ok=True)
    sim = make_sim(a)
    for name in DIALOGS:
        apply_runtime(sim, a)
        show(sim, name)
        write(sim, a, os.path.join(a.output, name + ".png"))


def cmd_list(a):
    print("themes:   ", " ".join(B.themes()))
    for k in B.PARTS:
        print("%-9s " % (k + ":"), " ".join(B.parts(k)))
    print("languages:", " ".join(B.languages()))
    print("dialogs:  ", " ".join(DIALOGS))
    sim = Sim(B.Config())
    print("settings (default):")
    for k, v in sim.settings().items():
        print("   %-14s %d" % (k, v))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("render", help="one dialog to one PNG")
    add_config_args(r)
    r.add_argument("--dialog", default="menu", choices=sorted(DIALOGS))
    r.add_argument("--keys", help="keys after the dialog is up: up,down,enter,power")
    r.add_argument("-o", "--output", required=True)
    al = sub.add_parser("all", help="every dialog into a directory")
    add_config_args(al)
    al.add_argument("-o", "--output", required=True)
    for q in (r, al):
        q.add_argument("--panel", action="store_true", help="the whole panel, OSD in place (default: OSD only)")
        q.add_argument("--background", help="#rrggbb or an image file to flatten onto (implies --panel)")
    sub.add_parser("list", help="themes, parts, languages, dialogs, settings")
    a = p.parse_args()
    {"render": cmd_render, "all": cmd_all, "list": cmd_list}[a.cmd](a)


if __name__ == "__main__":
    main()
