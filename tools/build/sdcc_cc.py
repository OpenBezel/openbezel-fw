#!/usr/bin/env python3
"""Compile one C file with SDCC for Ninja: the .rel (SDCC leaves .asm/.lst/.sym
next to it - check_banking.py reads the .asm) and a depfile from `sdcc -MM`.

    sdcc_cc.py SDCC OUT.rel DEPFILE -- <sdcc flags> SOURCE.c
"""
import os, subprocess, sys


def main():
    sdcc, out, dep = sys.argv[1:4]
    assert sys.argv[4] == "--"
    args = sys.argv[5:]
    src = args[-1]
    r = subprocess.run([sdcc] + args[:-1] + ["-c", src, "-o", out])
    if r.returncode:
        sys.exit(r.returncode)
    # dependencies: sdcc -MM prints "<obj>: <src> <headers...>" with continuation lines
    m = subprocess.run([sdcc, "-MM"] + args, capture_output=True, text=True)
    if m.returncode:
        sys.stderr.write(m.stderr)
        sys.exit(m.returncode)
    text = m.stdout.replace("\\\n", " ")
    deps = text.split(":", 1)[1].split() if ":" in text else [src]
    with open(dep, "w") as f:
        f.write("%s: %s\n" % (out, " ".join(os.path.abspath(d) for d in deps)))


if __name__ == "__main__":
    main()
