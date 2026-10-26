"""ditherlib - dither SRAM contents that follow a rule (gen_pipeline_tables.py,
gen_display_tables.py)."""


def sequence(nbytes, seed):
    """A dither sequence table: nbytes x four 2-bit pattern selectors, LSB first.  Stock's
    have no discernible rule (linear complexity ~n/2), so this is a shuffle: each of the
    four values exactly nbytes times, in an order fixed by an xorshift32 from `seed`."""
    sym = [v for v in range(4) for _ in range(nbytes)]
    x = seed & 0xFFFFFFFF or 1
    for i in range(len(sym) - 1, 0, -1):          # Fisher-Yates
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        j = x % (i + 1)
        sym[i], sym[j] = sym[j], sym[i]
    return bytes(sym[k] | sym[k + 1] << 2 | sym[k + 2] << 4 | sym[k + 3] << 6
                 for k in range(0, len(sym), 4))
