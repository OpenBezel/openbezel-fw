#!/usr/bin/env python3
"""gen_edid - the EDID the firmware serves (I2C 0x50 on the HDMI DDC, and over DP AUX),
as a C array for the build.

Two sources:
  --json FILE   build it from a description (boards/<board>/edid.json): an EDID 1.4 base
                block and a CTA-861 extension - see the encoder below for the keys
  --bin FILE    an existing binary (a stock capture, or anyone's), with optional edits:
                --no-deep-color clears DC_48/36/30/Y444 in the HDMI vendor block (CTA ext,
                OUI 00-0C-03, byte 6) so sources send 24-bit color - the receiver has no
                deep-color packing path

Block checksums are always recomputed.  --check runs edid-decode --check on the result when
it is installed; with --json a conformity failure is an error, with --bin a warning.

  gen_edid.py --json boards/intouch156b/edid.json --out build/edid_data.c --check
  gen_edid.py --bin boards/intouch156b/edid.bin --no-deep-color --out build/edid_data.c
"""
import argparse, json, os, shutil, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depfile  # noqa: E402


# ---- EDID base block (1.3, as HDMI requires, or 1.4) -------------------------------------

ESTABLISHED = {             # "WxH@Hz" -> (byte 0x23..0x25 index, bit)
    "720x400@70": (0, 7), "720x400@88": (0, 6), "640x480@60": (0, 5), "640x480@67": (0, 4),
    "640x480@72": (0, 3), "640x480@75": (0, 2), "800x600@56": (0, 1), "800x600@60": (0, 0),
    "800x600@72": (1, 7), "800x600@75": (1, 6), "832x624@75": (1, 5), "1024x768@87i": (1, 4),
    "1024x768@60": (1, 3), "1024x768@70": (1, 2), "1024x768@75": (1, 1), "1280x1024@75": (1, 0),
    "1152x870@75": (2, 7),
}
STD_ASPECT = {(16, 10): 0, (4, 3): 1, (5, 4): 2, (16, 9): 3}
INTERFACE = {"undefined": 0, "DVI": 1, "HDMI-a": 2, "HDMI-b": 3, "MDDI": 4, "DisplayPort": 5}
BPC = {6: 1, 8: 2, 10: 3, 12: 4, 14: 5, 16: 6}


def mode(s):
    """'1280x800@60' -> (1280, 800, 60)"""
    wh, hz = s.split("@")
    w, h = wh.split("x")
    return int(w), int(h), int(hz)


def mfg_id(code):
    if len(code) != 3 or not code.isalpha() or not code.isupper():
        raise SystemExit("vendor: three upper-case letters (a PNP ID), not %r" % code)
    v = 0
    for c in code:
        v = (v << 5) | (ord(c) - 64)
    return bytes([v >> 8, v & 0xFF])


def std_timing(s):
    w, h, hz = mode(s)
    for (aw, ah), code in STD_ASPECT.items():
        if w * ah == h * aw:
            break
    else:
        raise SystemExit("standard timing %s: no EDID aspect ratio (16:10, 4:3, 5:4, 16:9)" % s)
    if w % 8 or not 256 <= w <= 2288 or not 60 <= hz <= 123:
        raise SystemExit("standard timing %s: not encodable" % s)
    return bytes([w // 8 - 31, code << 6 | (hz - 60)])


def dtd(t, size_mm):
    """18-byte detailed timing.  t: pixel_khz, h/v = [active, front, sync, back], hsync/vsync
    polarity '+'/'-'"""
    ha, hf, hs, hb = t["h"]
    va, vf, vs, vb = t["v"]
    hbl, vbl = hf + hs + hb, vf + vs + vb
    clk = t["pixel_khz"] // 10
    if t["pixel_khz"] % 10:
        raise SystemExit("detailed timing: pixel clock must be a multiple of 10 kHz")
    hmm, vmm = size_mm
    flags = 0x18 | (0x04 if t.get("vsync", "+") == "+" else 0) | (0x02 if t.get("hsync", "+") == "+" else 0)
    return bytes([
        clk & 0xFF, clk >> 8,
        ha & 0xFF, hbl & 0xFF, (ha >> 8) << 4 | hbl >> 8,
        va & 0xFF, vbl & 0xFF, (va >> 8) << 4 | vbl >> 8,
        hf & 0xFF, hs & 0xFF, (vf & 0xF) << 4 | (vs & 0xF),
        (hf >> 8) << 6 | (hs >> 8) << 4 | (vf >> 4) << 2 | vs >> 4,
        hmm & 0xFF, vmm & 0xFF, (hmm >> 8) << 4 | vmm >> 8,
        0, 0, flags])


def text_descriptor(tag, s):
    if len(s) > 13 or not s.isascii():
        raise SystemExit("descriptor text %r: at most 13 ASCII characters" % s)
    s = s.encode() + (b"\n" + b" " * 12 if len(s) < 13 else b"")
    return bytes([0, 0, 0, tag, 0]) + s[:13]


def range_descriptor(r, v14):
    vmin, vmax = r["v_hz"]
    hmin, hmax = r["h_khz"]
    # 1.4: 0x01 = range limits only; 1.3 has no such code - 0x00, default GTF
    return bytes([0, 0, 0, 0xFD, 0, vmin, vmax, hmin, hmax, (r["max_mhz"] + 9) // 10,
                  0x01 if v14 else 0x00, 0x0A] + [0x20] * 6)


def base_block(d, n_ext):
    e = bytearray(b"\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00")
    e += mfg_id(d["vendor"])
    e += d["product"].to_bytes(2, "little") + d.get("serial", 0).to_bytes(4, "little")
    if d.get("week", 0) == 0xFF:
        e += bytes([0xFF, d["year"] - 1990])           # model year
    else:
        e += bytes([d.get("week", 0), d["year"] - 1990])
    v14 = d.get("edid_version", "1.3") == "1.4"
    e += bytes([1, 4 if v14 else 3])
    # digital input; 1.4 adds color depth and interface type, 1.3 has only the DFP bit
    e += bytes([0x80 | BPC[d["bpc"]] << 4 | INTERFACE[d["interface"]]] if v14 else [0x80])
    hmm, vmm = d["size_mm"]
    e += bytes([round(hmm / 10), round(vmm / 10)])
    e += bytes([round(d["gamma"] * 100) - 100])
    dpms = d.get("dpms", [])
    e += bytes([("standby" in dpms) << 7 | ("suspend" in dpms) << 6 | ("off" in dpms) << 5
                | d.get("srgb", False) << 2 | 1 << 1         # RGB 4:4:4; preferred timing (native)
                | (0 if v14 else 1 << 3 | d.get("gtf", False))])  # 1.3: RGB color display type; GTF
    c = d["chromaticity"]
    xy = [round(v * 1024) for k in ("red", "green", "blue", "white") for v in c[k]]
    if any(not 0 <= v <= 1023 for v in xy):
        raise SystemExit("chromaticity out of range")
    rx, ry, gx, gy, bx, by, wx, wy = xy
    e += bytes([(rx & 3) << 6 | (ry & 3) << 4 | (gx & 3) << 2 | (gy & 3),
                (bx & 3) << 6 | (by & 3) << 4 | (wx & 3) << 2 | (wy & 3),
                rx >> 2, ry >> 2, gx >> 2, gy >> 2, bx >> 2, by >> 2, wx >> 2, wy >> 2])
    est = bytearray(3)
    for s in d.get("established", []):
        if s not in ESTABLISHED:
            raise SystemExit("established timing %s: not one of %s" % (s, ", ".join(ESTABLISHED)))
        i, bit = ESTABLISHED[s]
        est[i] |= 1 << bit
    e += est
    std = [std_timing(s) for s in d.get("standard", [])]
    if len(std) > 8:
        raise SystemExit("at most 8 standard timings")
    e += b"".join(std) + b"\x01\x01" * (8 - len(std))
    descs = [dtd(d["preferred"], d["size_mm"]), range_descriptor(d["range"], v14),
             text_descriptor(0xFC, d["name"])]
    if d.get("serial_text"):
        descs.append(text_descriptor(0xFF, d["serial_text"]))
    while len(descs) < 4:
        descs.append(bytes([0, 0, 0, 0x10]) + bytes(14))      # dummy descriptor
    e += b"".join(descs)
    e += bytes([n_ext, 0])
    assert len(e) == 128
    return e


# ---- CTA-861 extension ------------------------------------------------------------------

AUDIO_RATES = {32: 0, 44.1: 1, 48: 2, 88.2: 3, 96: 4, 176.4: 5, 192: 6}
AUDIO_BITS = {16: 0, 20: 1, 24: 2}
SPEAKERS = {"FL/FR": 0, "LFE": 1, "FC": 2, "RL/RR": 3, "RC": 4, "FLC/FRC": 5, "RLC/RRC": 6}


def data_block(tag, payload):
    if len(payload) > 31:
        raise SystemExit("CTA data block too long")
    return bytes([tag << 5 | len(payload)]) + payload


def cta_block(c, size_mm):
    blocks = b""
    blocks += data_block(2, bytes(c["vics"]))
    a = c.get("audio")
    if a:
        lp = a["lpcm"]
        rates = sum(1 << AUDIO_RATES[r] for r in lp["rates_khz"])
        bits = sum(1 << AUDIO_BITS[b] for b in lp["bits"])
        blocks += data_block(1, bytes([1 << 3 | (lp["channels"] - 1), rates, bits]))
    if c.get("speakers"):
        blocks += data_block(4, bytes([sum(1 << SPEAKERS[s] for s in c["speakers"]), 0, 0]))
    h = c.get("hdmi")
    if h:
        pa = [int(x) for x in h["physical_address"].split(".")]
        blocks += data_block(3, bytes([0x03, 0x0C, 0x00, pa[0] << 4 | pa[1], pa[2] << 4 | pa[3],
                                       0x00, h["max_tmds_mhz"] // 5]))
    q = c.get("video_capability")
    if q is not None:               # scan behavior: 2 = always underscanned
        blocks += data_block(7, bytes([0x00, q.get("qy", 0) << 7 | q.get("qs", 0) << 6
                                       | q.get("s_pt", 0) << 4 | q.get("s_it", 0) << 2 | q.get("s_ce", 0)]))
    dtds = b"".join(dtd(t, size_mm) for t in c.get("detailed", []))
    off = 4 + len(blocks)
    e = bytearray([0x02, 0x03, off,
                   c.get("underscan", 0) << 7 | (1 if a else 0) << 6 | c.get("ycbcr444", 0) << 5
                   | c.get("ycbcr422", 0) << 4 | c.get("native_dtds", 0)])
    e += blocks + dtds
    if len(e) > 127:
        raise SystemExit("CTA extension: %d bytes, more than fit in 127" % len(e))
    e += bytes(128 - len(e))
    return e


def from_json(path):
    d = json.load(open(path))
    ext = [cta_block(d["cta"], d["size_mm"])] if "cta" in d else []
    return bytes(base_block(d, len(ext)) + b"".join(ext))


# ---- binary input ----------------------------------------------------------------------

def no_deep_color(e, notes):
    if len(e) >= 256 and e[128] == 0x02:
        i, dtd_off = 4, e[130]
        while i < dtd_off:
            tag, ln = e[128 + i] >> 5, e[128 + i] & 0x1F
            if tag == 3 and e[129 + i:132 + i] == bytes.fromhex("030c00") and ln >= 6:
                old = e[128 + i + 6]
                e[128 + i + 6] = old & 0x87          # keep Supports_AI (bit7) and DVI_Dual (bit0)
                notes.append("HDMI VSDB byte6 0x%02X -> 0x%02X (deep color off)" % (old, e[128 + i + 6]))
            i += 1 + ln


def fix_checksums(e):
    e = bytearray(e)
    for b in range(0, len(e), 128):
        e[b + 127] = (-sum(e[b:b + 127])) & 0xFF
    return bytes(e)


def check(e, strict):
    tool = shutil.which("edid-decode")
    if not tool:
        print("gen_edid: edid-decode not installed - EDID not checked", file=sys.stderr)
        return
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        f.write(e)
        f.flush()
        r = subprocess.run([tool, "--check", f.name], capture_output=True, text=True)
    out = r.stdout + r.stderr
    if "conformity: PASS" in out:
        return
    tail = out[out.find("Warnings:"):] if "Warnings:" in out else out[-2000:]
    if strict:
        raise SystemExit("gen_edid: edid-decode --check failed:\n" + tail)
    print("gen_edid: warning, edid-decode --check:\n" + tail, file=sys.stderr)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--json", help="EDID description")
    src.add_argument("--bin", help="EDID binary")
    p.add_argument("--no-deep-color", action="store_true", help="(--bin) clear the HDMI deep-color flags")
    p.add_argument("--out", required=True, help="C file")
    p.add_argument("--out-bin", help="also write the binary")
    p.add_argument("--check", action="store_true", help="run edid-decode --check")
    p.add_argument("--depfile")
    a = p.parse_args()
    depfile.track()
    notes = []
    if a.json:
        e = bytearray(from_json(a.json))
        what = "generated from %s" % os.path.basename(a.json)
    else:
        e = bytearray(open(a.bin, "rb").read())
        if len(e) % 128 or not e:
            raise SystemExit("%s: not a whole number of 128-byte EDID blocks" % a.bin)
        if a.no_deep_color:
            no_deep_color(e, notes)
        what = "from %s" % os.path.basename(a.bin)
    e = fix_checksums(e)
    if len(e) > 256:
        raise SystemExit("EDID is %d bytes; the DDC RAM holds 256" % len(e))
    if a.check:
        check(e, strict=bool(a.json))
    out = ["/* The EDID served at I2C 0x50 (HDMI DDC RAM, XDATA 0xE680) and over DP AUX,",
           " * %s by tools/gen_edid.py - do not edit." % what]
    out += [" * edit: " + n for n in notes]
    out += [" */", '#include "ob_types.h"', "", "const __code u8 board_edid[256] = {"]
    e256 = e + bytes(256 - len(e))
    for i in range(0, 256, 12):
        out.append("    " + ", ".join("0x%02X" % b for b in e256[i:i + 12]) + ",")
    out.append("};")
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(a.out, "w").write("\n".join(out) + "\n")
    if a.out_bin:
        open(a.out_bin, "wb").write(e)
    if a.depfile:
        depfile.write(a.depfile, [a.out] + ([a.out_bin] if a.out_bin else []))
    print("gen_edid: %s, %d bytes%s" % (what, len(e), "; " + "; ".join(notes) if notes else ""))


if __name__ == "__main__":
    main()
