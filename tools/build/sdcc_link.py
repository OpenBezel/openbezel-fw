#!/usr/bin/env python3
"""Link the firmware with SDCC/sdld for Ninja.

    sdcc_link.py SDCC SDLD OUT.ihx -- <sdcc link flags> -- REL...

Two steps, as before: sdld's memory summary (-M, which SDCC always adds) rejects
code above 0xFFFF - any banked code - and fails the link.  So SDCC only writes
the linker script (its own sdld run is expected to fail) and sdld runs on it
without -M.  The DATA/XDATA checks that remain are not enough on their own:
check_banking.py and mkimage.py check the result."""
import os, subprocess, sys


def main():
    sdcc, sdld, out = sys.argv[1:4]
    i = sys.argv.index("--", 4)
    j = sys.argv.index("--", i + 1)
    flags, rels = sys.argv[i + 1:j], sys.argv[j + 1:]
    base = os.path.splitext(out)[0]
    subprocess.run([sdcc] + flags + rels + ["-o", out], capture_output=True)
    lk = base + ".lk"
    if not os.path.exists(lk):
        sys.exit("sdcc did not write %s" % lk)
    link = base + "_link.lk"
    with open(lk) as f, open(link, "w") as g:
        g.writelines(l for l in f if l.strip() != "-M")
    r = subprocess.run([sdld, "-nf", link])
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
