#!/usr/bin/env python3
"""gen_lang - the menu's languages (lang/<code>.txt) into a string table blob for the
asset partition and the string IDs for the firmware.

    gen_lang.py --lang-dir lang --langs en de fr --themes-json build/<board>/gen/themes.json \\
                --out-header build/<board>/lang_ids.h --out-bin build/<board>/lang.bin

The first language is the reference (English): it defines every key, in order, and
a key another language leaves out falls back to its text.  Files are UTF-8; every
character must be in the OSD font (ASCII 0x20-0x7E or the Latin-1 letters 0xC0-0xFF,
see tools/gen_font.py).  Length limits per key prefix are checked (lang/en.txt lists
them); the footer HINT is also measured in every built-in theme's font.

Blob (all little endian; the firmware reads it with flash_read, core/lang.c):
    0       u8  language count L
    1       u8  string count N
    2       u16 offset[L][N]   from the blob start, of a NUL-terminated Latin-1 string
    2+4LN   the strings (identical ones stored once)

Header: S_<KEY> ids, LANG_COUNT, STR_COUNT, LANG_TAB, LANG_STR_MAX, the languages'
MCCS 0xCC codes (LANG_MCCS_CODES, LANG_MCCS_CAPS).
Outputs are only rewritten when they change.
"""
import argparse, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depfile

LIMITS = {"T_": 13, "I_": 16, "V_": 13, "N_": 13, "IL_": 12, "IV_": 17, "MSG_": 30,
          "HINT": 60, "LANG_NAME": 13}
STR_MAX = 60                    # core/lang.c's buffer (a multiple of 3: it reads 3 at a time)


def die(msg):
    sys.exit("gen_lang: " + msg)


def parse(path):
    meta, keys = {}, []
    strings = {}
    for n, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        m = re.match(r"\s*(@?[A-Z0-9_a-z]+)\s*=\s?(.*)$", line)
        if not m:
            die("%s:%d: expected KEY = text" % (path, n))
        k, v = m.group(1), m.group(2).rstrip()
        if k.startswith("@"):
            meta[k[1:]] = v
            continue
        if k in strings:
            die("%s:%d: %s twice" % (path, n, k))
        strings[k] = v
        keys.append(k)
    return meta, keys, strings


def limit_of(k):
    for p in sorted(LIMITS, key=len, reverse=True):
        if k.startswith(p):
            return LIMITS[p]
    die("key %s: no length rule for its prefix (%s)" % (k, ", ".join(LIMITS)))


def theme_widths(path):
    """-> [(theme, font widths, body columns)] from gen_theme.py's themes.json"""
    return [(t["name"], t["font_widths"], t["body_cols"]) for t in json.load(open(path))["themes"]]


def px(s, widths):
    n = 0
    for b in s:
        n += widths[b - 0x20] if b < 0x80 else widths[95 + b - 0xC0]
    return n


def write_if_changed(path, data):
    mode = "wb" if isinstance(data, bytes) else "w"
    old = None
    if os.path.exists(path):
        old = open(path, "rb" if mode == "wb" else "r").read()
    if old != data:
        open(path, mode).write(data)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--lang-dir", default="lang")
    p.add_argument("--langs", nargs="+", required=True)
    p.add_argument("--themes-json", help="gen_theme.py's themes.json, to measure the HINT in each theme's font")
    p.add_argument("--out-header", required=True)
    p.add_argument("--out-bin", required=True)
    p.add_argument("--default", help="the default language's code (LANG_DEFAULT; default: the first)")
    p.add_argument("--depfile", help="write a Make-style depfile of every input read (Ninja)")
    a = p.parse_args()
    depfile.track()
    if a.default and a.default not in a.langs:
        die("--default %s is not in --langs" % a.default)

    langs = []
    for code in a.langs:
        path = os.path.join(a.lang_dir, code + ".txt")
        if not os.path.exists(path):
            die("no %s (LANGS=%s)" % (path, " ".join(a.langs)))
        langs.append((code, path) + parse(path))
    ref_keys, ref = langs[0][3], langs[0][4]
    if len(ref_keys) > 255:
        die("%d strings: the firmware's ids are u8" % len(ref_keys))
    themes = theme_widths(a.themes_json) if a.themes_json else []

    errors = []
    tables = []
    for code, path, meta, keys, strings in langs:
        for k in keys:
            if k not in ref:
                errors.append("%s: %s is not a key of %s" % (path, k, langs[0][1]))
        if "mccs" not in meta:
            errors.append("%s: no @mccs (the MCCS CCh language code)" % path)
        table = []
        for k in ref_keys:
            s = strings.get(k, ref[k])
            try:
                b = s.encode("latin-1")
            except UnicodeEncodeError:
                errors.append("%s: %s: %r has a character outside Latin-1" % (path, k, s))
                b = b"?"
            if any(c < 0x20 or 0x7F <= c < 0xC0 for c in b):
                errors.append("%s: %s = %r: a character the OSD font lacks (ASCII and the Latin-1 "
                              "letters 0xC0-0xFF only)" % (path, k, s))
            lim = min(limit_of(k), STR_MAX)
            if len(b) > lim:
                errors.append("%s: %s = %r: %d characters, the limit is %d" % (path, k, s, len(b), lim))
            # the footer spans the body columns, less the panel border/inset at the
            # right: one cell of margin (the terminal theme's French hint ran into
            # its double border at the full width - found with tools/osdsim)
            for name, widths, cols in themes if k == "HINT" else ():
                if px(b, widths) > (cols - 1) * 12:
                    errors.append("%s: HINT is %d px wide in the %s theme's font, the footer has %d"
                                  % (path, px(b, widths), name, (cols - 1) * 12))
            table.append(b)
        tables.append(table)
    if errors:
        die("\n  " + "\n  ".join(errors))

    L, N = len(langs), len(ref_keys)
    head = 2 + 2 * L * N
    data, where = bytearray(), {}
    offs = []
    for table in tables:
        for b in table:
            if b not in where:
                where[b] = head + len(data)
                data += b + b"\0"
            offs.append(where[b])
    blob = bytearray([L, N])
    for o in offs:
        blob += bytes([o & 0xFF, o >> 8])
    blob += data
    if len(blob) > 0xFFFF:
        die("blob %d bytes: offsets are u16" % len(blob))

    codes = [int(l[2]["mccs"], 16) for l in langs]
    h = ["/* generated by tools/gen_lang.py from lang/{%s}.txt - do not edit */" % ",".join(a.langs),
         "#ifndef OB_LANG_IDS_H", "#define OB_LANG_IDS_H", "",
         "#define LANG_COUNT %d" % L,
         "#define LANG_DEFAULT %d          /* the default language (setting index) */"
         % (a.langs.index(a.default) if a.default else 0),
         "#define STR_COUNT %d" % N,
         "#define LANG_TAB 2              /* u16 offset[LANG_COUNT][STR_COUNT] */",
         "#define LANG_STR_MAX %d         /* longest string the firmware reads (a multiple of 3) */" % STR_MAX,
         "/* MCCS CCh (OSD language) code of each language, in setting order */",
         "#define LANG_MCCS_CODES { %s }" % ", ".join("0x%02X" % c for c in codes),
         "#define LANG_MCCS_CAPS \"%s\"" % " ".join("%02X" % c for c in codes),
         ""]
    for i, k in enumerate(ref_keys):
        h.append("#define S_%s %d" % (k, i))
    h += ["", "#endif", ""]
    os.makedirs(os.path.dirname(a.out_header) or ".", exist_ok=True)
    write_if_changed(a.out_header, "\n".join(h))
    write_if_changed(a.out_bin, bytes(blob))
    print("  languages %s: %d strings, %d bytes" % (" ".join(a.langs), N, len(blob)))
    depfile.write(a.depfile, [a.out_header, a.out_bin])


if __name__ == "__main__":
    main()
