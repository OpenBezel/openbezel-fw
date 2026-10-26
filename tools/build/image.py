#!/usr/bin/env python3
"""The firmware image from the linked .ihx: check_banking.py (cross-bank calls,
XDATA end) then mkimage.py (bank images, recovery-path and window checks).

    image.py MAP IHX VERSION_FILE SPLIT OUT.bin -- REL... -- BANK:FILE...
"""
import os, subprocess, sys

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    mp, ihx, ver, split, out = sys.argv[1:6]
    i = sys.argv.index("--", 6)
    j = sys.argv.index("--", i + 1)
    rels, banks = sys.argv[i + 1:j], sys.argv[j + 1:]
    asms = [os.path.splitext(r)[0] + ".asm" for r in rels]
    py = sys.executable
    r = subprocess.run([py, os.path.join(TOOLS, "check_banking.py"), mp] + asms)
    if r.returncode:
        sys.exit(r.returncode)
    cmd = [py, os.path.join(TOOLS, "mkimage.py"), ihx, out, "--version-file", ver, "--map", mp,
           "--split", split]
    for b in banks:
        cmd += ["--bank", b]
    sys.exit(subprocess.run(cmd).returncode)


if __name__ == "__main__":
    main()
