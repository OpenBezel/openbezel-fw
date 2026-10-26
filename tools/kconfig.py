#!/usr/bin/env python3
"""kconfig - the firmware configuration (Kconfig at the repo root), through the
kconfiglib package (apt python3-kconfiglib / pip kconfiglib; the build container has it).

    kconfig.py --config build/.config defconfig configs/intouch156b_defconfig
    kconfig.py --config build/.config olddefconfig       # new symbols -> defaults
    kconfig.py --config build/.config menuconfig         # terminal UI
    kconfig.py --config build/.config guiconfig          # Tk UI
    kconfig.py --config build/.config savedefconfig configs/foo_defconfig
    kconfig.py --config build/.config genconfig config.h # CONFIG_* for the C code

The build runs these as `ninja menuconfig` etc.; .config lives in the build
directory, so several build directories can hold different configurations.
Files are only rewritten when their contents change.
"""
import argparse, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
try:
    import kconfiglib
except ImportError:
    sys.exit("kconfig.py: kconfiglib is not installed (apt install python3-kconfiglib, "
             "pip install kconfiglib, or build in the container: tools/docker-run)")


def load(config, kconfig):
    os.environ["srctree"] = REPO
    os.environ["KCONFIG_CONFIG"] = config
    kconf = kconfiglib.Kconfig(kconfig, warn_to_stderr=True)
    return kconf


def write_if_changed(path, text):
    if os.path.exists(path) and open(path).read() == text:
        return False
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    open(path, "w").write(text)
    return True


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--config", required=True, help="the .config (in the build directory)")
    p.add_argument("--kconfig", default=os.path.join(REPO, "Kconfig"))
    sub = p.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("defconfig")
    d.add_argument("defconfig")
    sub.add_parser("olddefconfig")
    sub.add_parser("menuconfig")
    sub.add_parser("guiconfig")
    s = sub.add_parser("savedefconfig")
    s.add_argument("out", nargs="?", help="default: defconfig next to .config")
    g = sub.add_parser("genconfig")
    g.add_argument("header")
    m = sub.add_parser("mesonconfig", help="KEY=value for meson's keyval module")
    m.add_argument("out")
    a = p.parse_args()
    config = os.path.abspath(a.config)
    kconf = load(config, a.kconfig)

    if a.cmd == "defconfig":
        kconf.load_config(a.defconfig)
        kconf.write_config(config)
    elif a.cmd == "olddefconfig":
        if os.path.exists(config):
            kconf.load_config(config)
        kconf.write_config(config)
    elif a.cmd in ("menuconfig", "guiconfig"):
        if os.path.exists(config):
            kconf.load_config(config)
        mod = __import__(a.cmd)
        if a.cmd == "guiconfig":                # color swatches + a picker for THEME_COLOR_*
            import kconfig_colors
            kconfig_colors.install(mod, kconf)
        mod.menuconfig(kconf)
    elif a.cmd == "savedefconfig":
        kconf.load_config(config)
        out = a.out or os.path.join(os.path.dirname(config), "defconfig")
        print(kconf.write_min_config(out))
    elif a.cmd == "mesonconfig":
        # keyval takes '#' as a comment even inside quotes ("#FF4080" read as '"'):
        # values unquoted, with % and # escaped as %25 / %23 (meson.build decodes)
        kconf.load_config(config)
        lines = []
        for sym in kconf.unique_defined_syms:
            v = sym.str_value
            if sym.type in (kconfiglib.BOOL, kconfiglib.TRISTATE) and v == "n":
                continue
            lines.append("CONFIG_%s=%s" % (sym.name, v.replace("%", "%25").replace("#", "%23")))
        write_if_changed(a.out, "\n".join(lines) + "\n")
    elif a.cmd == "genconfig":
        kconf.load_config(config)
        os.makedirs(os.path.dirname(os.path.abspath(a.header)), exist_ok=True)
        tmp = a.header + ".tmp"
        kconf.write_autoconf(tmp)
        text = open(tmp).read()
        os.remove(tmp)
        write_if_changed(a.header, text)


if __name__ == "__main__":
    main()
