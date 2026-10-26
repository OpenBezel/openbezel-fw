#!/usr/bin/env python3
"""gen_pipeline_tables - emit pipeline_tables.c (build/gen/): the burst-loaded pipeline SRAM
tables.  An unloaded table is not a no-op: the scale-up filter with an
unwritten coefficient SRAM behaves as a high-pass and renders every flat area black.

Everything is computed or a documented constant; a stock image (--stock-bin) is only compared
against.

  scale-up H     phase-invariant 3-tap [0, c, 1024-2c, c] per sharpness preset: only c is
                 emitted (SU_H_SIDE, stock's values), pipeline.c generates the table
  scale-up V     Lanczos-2 windowed sinc, cutoff per preset (v_kernel, SU_V_FC)
  scale-down A   the firmware loads scale-up V's sharpest preset (stock's A was its own)
  scale-down B   2-tap linear (sd_linear) - scale-down is never active on a 1080p panel
  input dither   Latin-square tables (idither_table), temporal 3,2,1,0, sequence table a
                 balanced shuffle (ditherlib.sequence)
  (stock image locations, for the comparisons: scale-up bank5 7A1A / 778B + 0x80 per
  preset; input dither bank1 3659 / 3671 / 3689 / 36A1)

Filter coefficients are N taps x 16 phases of 16-bit little-endian 12-bit-signed values stored
tap-major, the phases at bin centers (p + 1/2)/32 of offsets 0..1/2; every phase must sum to
exactly 1024 (unity DC gain) or the filter rescales the flat field - checked here.

Usage: gen_pipeline_tables.py [--stock-bin PATH] --out build/gen/pipeline_tables.c
"""
import argparse, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ditherlib  # noqa: E402

SU_H, SU_V, SU_STRIDE, SU_PRESETS = 0x57A1A, 0x5778B, 0x80, 5
ID_8BPC, ID_10BPC, ID_SEQ, ID_TEMPORAL = 0x13659, 0x13671, 0x13689, 0x136A1


def s12(x):
    return x - 0x1000 if x >= 0x800 else x


# Scale-up V: Lanczos-2 windowed sinc with cutoff fc per sharpness preset (fc < 1 blurs,
# 1 is plain Lanczos-2, > 1 sharpens), sampled at phase-bin centers (p + 1/2) / 32 - the
# 16 phases cover offsets 0 .. 1/2 - and normalized to exactly 1024 per phase.
SU_V_FC = (0.5, 0.75, 0.875, 1.0, 1.125)


def v_kernel(fc):
    """128 bytes: 4 taps x 16 phases, tap-major, 12-bit two's complement, little-endian"""
    def sinc(x):
        return 1.0 if x == 0 else math.sin(math.pi * x) / (math.pi * x)
    taps = [[0] * 16 for _ in range(4)]
    for ph in range(16):
        d = (ph + 0.5) / 32
        h = [fc * sinc(fc * x) * sinc(x / 2) for x in (2 - d, 1 - d, d, 1 + d)]
        s = sum(h)
        w = [round(v / s * 1024) for v in h]
        w[2] += 1024 - sum(w)                   # the largest tap absorbs the rounding
        for k in range(4):
            taps[k][ph] = w[k]
    out = b""
    for k in range(4):
        for v in taps[k]:
            out += bytes([v & 0xFF, (v >> 8) & 0x0F])
    return out


# Scale-up H side tap c per sharpness preset (stock's values: [0, c, 1024-2c, c])
SU_H_SIDE = (127, 78, 22, -43, -92)


def sd_linear():
    """scale-down table B: 2-tap linear interpolation at the phase-bin centers (p + 1/2) / 32,
    tap 0 the far sample - 12-bit two's complement, little-endian, tap-major"""
    far = [round((ph + 0.5) / 32 * 1024) for ph in range(16)]
    out = b""
    for taps in (far, [1024 - v for v in far]):
        out += b"".join(bytes([v & 0xFF, (v >> 8) & 0x0F]) for v in taps)
    return out


def h_kernel(c):
    """the scale-up H table for side tap c: [0, c, 1024-2c, c] x 16 phases, tap-major,
    12-bit two's complement, little-endian"""
    out = b""
    for v in (0, c, 1024 - 2 * c, c):
        out += bytes([v & 0xFF, (v >> 8) & 0x0F]) * 16
    return out


def check_filter(name, block, ntaps):
    v = [block[i] | (block[i + 1] << 8) for i in range(0, len(block), 2)]
    nph = len(v) // ntaps
    taps = [v[k * nph:(k + 1) * nph] for k in range(ntaps)]
    sums = {sum(s12(taps[k][p]) for k in range(ntaps)) for p in range(nph)}
    if sums != {1024}:
        raise SystemExit("%s: DC gain is not unity for every phase: %s" % (name, sorted(sums)))
    return "%d taps x %d phases, unity DC gain" % (ntaps, nph)


def idither_table(shift):
    """Input dither table: per channel a 4x4 Latin square of 2-bit offsets,
    v(ch, r, c) = (p[c] + r + o[ch]) mod 4 with p = 1,2,0,3 and o = 0,3,1 - each row
    the one above plus one, each channel the same square with its rows rotated.  Two
    values a byte (column 0 in the high nibble), 8 bytes a channel.  `shift` scales it
    to the input precision: 0 for 10 bpc (2 dropped bits), 2 for 8 bpc (4 dropped bits)."""
    p, o = (1, 2, 0, 3), (0, 3, 1)
    out = []
    for ch in range(3):
        for r in range(4):
            v = [((p[c] + r + o[ch]) % 4) << shift for c in range(4)]
            out += [v[0] << 4 | v[1], v[2] << 4 | v[3]]
    return bytes(out)


# input dither temporal table: the frame sequence 3,2,1,0 (2-bit fields, LSB first), x4
IDITHER_TEMPORAL = bytes([0xE4] * 4)


def emit(f, cname, data, comment, per_line=12):
    f.write("/* %s */\n" % comment)
    f.write("const __code u8 %s[%d] = {\n" % (cname, len(data)))
    for i in range(0, len(data), per_line):
        f.write("    " + " ".join("0x%02X," % b for b in data[i:i + per_line]) + "\n")
    f.write("};\n\n")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--stock-bin", help="a stock image to compare against")
    p.add_argument("--out", required=True)
    a = p.parse_args()
    img = open(a.stock_bin, "rb").read() if a.stock_bin else None
    notes = []

    with open(a.out, "w") as f:
        f.write("/* generated by tools/gen_pipeline_tables.py - do not edit */\n")
        f.write('#include "ob_types.h"\n\n')
        f.write("/* Burst-loaded pipeline SRAM tables. */\n\n")

        # ---- scale-up filter, all five sharpness presets -----------------
        # H (1st table): a phase-invariant 3-tap kernel [0, c, 1024-2c, c] per preset;
        # only c is emitted and pipeline.c generates the table.
        for n, c in enumerate(SU_H_SIDE):
            notes.append("scale-up h preset %d: c = %d, %s" % (n, c, check_filter("su_h_%d" % n, h_kernel(c), 4)))
            if img is not None and img[SU_H + n * SU_STRIDE:][:128] != h_kernel(c):
                raise SystemExit("scale-up h preset %d differs from stock" % n)
        f.write("/* scale-up H (1st table): side tap c of the phase-invariant kernel [0, c, 1024-2c, c],\n"
                " * per sharpness preset - pipeline.c generates the table from it */\n")
        f.write("const __code s16 scaleup_h_side[%d] = { %s };\n\n" % (SU_PRESETS, ", ".join(map(str, SU_H_SIDE))))
        # V (2nd table): Lanczos-2 windowed sinc per preset (v_kernel)
        blocks = b"".join(v_kernel(fc) for fc in SU_V_FC)
        for n in range(SU_PRESETS):
            notes.append("scale-up v preset %d (fc %.3f): %s"
                         % (n, SU_V_FC[n], check_filter("su_v_%d" % n, blocks[n * 128:(n + 1) * 128], 4)))
        emit(f, "scaleup_coefs_v", list(blocks),
             "scale-up 2nd table -> vertical, %d sharpness presets of 128 bytes (0035 wcoef_sel2 1):\n"
             " * Lanczos-2 windowed sinc, cutoff fc = %s (gen_pipeline_tables.py v_kernel)"
             % (SU_PRESETS, ", ".join("%g" % x for x in SU_V_FC)))

        # ---- scale-down filter -------------------------------------------
        # A is the scale-up V table's sharpest preset (pipeline.c); B a 2-tap linear kernel
        sdb = sd_linear()
        notes.append("scale-down B (linear): " + check_filter("sd_b", sdb, 2))
        emit(f, "scaledown_coefs_b", list(sdb),
             "scale-down, 0026[7:6] = 3 -> port 0027, 2 taps x 16 phases: linear (sd_linear)")

        # ---- input dither -------------------------------------------------
        d8, d10 = idither_table(2), idither_table(0)
        if img is not None:
            for name, calc, off in (("8 bpc", d8, ID_8BPC), ("10 bpc", d10, ID_10BPC),
                                    ("temporal", IDITHER_TEMPORAL, ID_TEMPORAL)):
                if img[off:off + len(calc)] != calc:
                    raise SystemExit("input dither %s: computed table differs from stock" % name)
            notes.append("input dither: 8/10 bpc and temporal tables equal to stock")
        emit(f, "idither_table_8bpc", d8, "input dither table, 8 bits per component (0089 access = 2): idither_table()")
        emit(f, "idither_table_10bpc", d10, "input dither table, 10 bits per component (0089 access = 2): idither_table()")
        emit(f, "idither_seq_table", ditherlib.sequence(24, 0x1D17E5),
             "input dither sequence table (0089 access = 1): ditherlib.sequence(24, 0x1D17E5)")
        emit(f, "idither_temporal_table", IDITHER_TEMPORAL, "input dither temporal table (0089 access = 3): frames 3,2,1,0")

    print("wrote %s" % a.out)
    for n in notes:
        print("  " + n)


if __name__ == "__main__":
    main()
