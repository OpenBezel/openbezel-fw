"""RTD2556 OSD model: the recorded OSD state -> RGBA pixels.

Implements the RTD2556 OSD as verified on the
intouch156b: windows 0-9 and sub-windows WIN4_1..8 with fill, border, drop
shadow, 3D bevel, vertical/horizontal gradient, blend and rounded corners; row
and character commands with 1-bit (proportional width, disp_zone) and 4-bit
glyphs; the 64-entry color LUT; 2x doubling.  Not modeled (the firmware does
not use them): 2-bit glyphs, grid lines, highlight windows, chessboard fill,
window-above-text priority, 256-level gradients, VLC.

Alpha follows the panel: windows are opaque unless blended (alpha = 1 - level/32,
level = OVERLAY_CTRL1_M1[7:3]); character pixels are opaque; LUT 0 as a 1-bit
background (and as the 4-bit background with f4b_color_sel) is transparent.
Windows are a priority mux, not layers: where windows overlap only the top one
shows, and a blended one blends with the video, never with a window below it.
"""
import numpy as np

NAME = "rtd2556"

# --- what the host build records (host/chips/rtd2556/osd_hal.c) ---------------------
FONT_WORDS = 0x2800
WIN_WORDS = 0x0400
STATE_ARRAYS = {"font": FONT_WORDS * 3, "win": WIN_WORDS * 3, "lut": 64 * 3}

# --- host build: the HAL replaced by recorders / stubs, the firmware's own GUI code ---
HOST_SOURCES = ["chips/rtd2556/osd_hal.c", "chips/rtd2556/hal_stubs.c"]
FIRMWARE_SOURCES = ["core/menu.c", "core/gui.c", "core/console.c", "core/lang.c",
                    "core/picmode.c", "core/settings.c"]
INCLUDE_DIRS = ["core", "hal/realtek/common", "hal/realtek/rtd2556", "hal/realtek/rtd2556/regs"]

PANEL = (1920, 1080)

# --- registers --------------------------------------------------------------------
OVERLAY_CTRL1_M1 = 0x008F
FC = 0x3A00
FRAME_CTRL_02, FRAME_CTRL_07, FRAME_CTRL_0A = FC + 0x02, FC + 0x07, FC + 0x0A
FRAME_CTRL_24, FRAME_CTRL_25, FRAME_CTRL_26 = FC + 0x24, FC + 0x25, FC + 0x26
ROUND_TABLE = {8: (FC + 0x60, 4), 16: (FC + 0x64, 8), 32: (FC + 0x6C, 32)}

CHAR_BASE = 0x0020          # osd.h OSD_CHAR_BASE (fs_base)
FONT_BASE = 0x0400          # osd.h OSD_FONT_BASE
CELL_W, CELL_H = 12, 18
WIN_DX = 10                 # windows sit 10 px left of characters at the same coordinate

MAIN_WINDOWS = [(n, 0x0100 + 4 * n) for n in range(10)]
SUB_WINDOWS = [("4_%d" % (x + 1), 0x0200 + 4 * x) for x in range(8)]
ROUND_ADR10 = {0: 0x0178, 5: 0x019B, "4_1": 0x01BE, "4_2": 0x01C5, "4_3": 0x01CC}

CANVAS = (1024, 1024)       # logical OSD space rendered (origin-relative)


class Frame:
    """A rendered OSD: `image` (RGBA, at the OSD's own scale) placed at `pos` on the panel."""

    def __init__(self, image, pos, scale, enabled):
        self.image, self.pos, self.scale, self.enabled = image, pos, scale, enabled


def _word(buf, addr):
    i = addr * 3
    return buf[i], buf[i + 1], buf[i + 2]


def _lut(lut, i):
    i &= 0x3F
    return lut[3 * i], lut[3 * i + 1], lut[3 * i + 2]


def _over(dst, rgba_layer):
    """alpha-composite a float RGBA layer (H, W, 4; 0..1) over dst in place"""
    a = rgba_layer[..., 3:4]
    da = dst[..., 3:4]
    out_a = a + da * (1 - a)
    rgb = np.where(out_a > 0, (rgba_layer[..., :3] * a + dst[..., :3] * da * (1 - a)) / np.maximum(out_a, 1e-9), 0)
    dst[..., :3] = rgb
    dst[..., 3:4] = out_a


class Window:
    def __init__(self, win, name, base):
        w0, w1, w2, w3 = (_word(win, base + k) for k in range(4))
        self.name = name
        self.enabled = bool(w3[2] & 0x01)
        self.x0 = ((w1[0] >> 4) << 8 | w1[1]) - WIN_DX
        self.y0 = (w1[0] & 0x0F) << 8 | w1[2]
        self.x1 = ((w2[0] >> 4) << 8 | w2[1]) - WIN_DX
        self.y1 = (w2[0] & 0x0F) << 8 | w2[2]
        self.color = ((w3[0] >> 5) & 1) << 5 | (w3[1] & 0x1F)
        self.bcol = ((w3[0] >> 6) & 1) << 5 | ((w0[0] >> 6) & 1) << 4 | (w0[1] & 0x0F)
        self.scol = ((w3[0] >> 7) & 1) << 5 | ((w0[0] >> 7) & 1) << 4 | (w0[1] >> 4)
        self.eff_w = ((w0[0] >> 3) & 7) + 1
        self.eff_h = (w0[0] & 7) + 1
        self.effect = bool(w3[2] & 0x10)
        self.etype = (w3[2] >> 1) & 7
        self.blend = bool(w3[2] & 0x80)
        self.gradient = bool(w3[2] & 0x40)
        self.grad_vertical = bool(w3[2] & 0x20)
        self.grad_en = w0[2] & 0x07            # R G B = bits 2 1 0
        self.grad_pol = (w0[2] >> 5) & 0x07    # same order: 1 = counting up
        self.grad_steps = (w0[2] >> 3) & 0x03
        self.grad_level = (w3[1] >> 5) & 0x07
        self.round = None
        if name in ROUND_ADR10:
            r = win[ROUND_ADR10[name] * 3]
            if r & 1:
                self.round = ({0: 8, 1: 16, 2: 32}.get((r >> 1) & 3, 32), (r >> 3) & 0x0F)


def _round_fill(sr, radius):
    """per line from the outer edge: the number of filled pixels counted from the inner side"""
    base, n = ROUND_TABLE[radius]
    raw = [sr[base + i] for i in range(n)]
    if radius == 32:
        return [v & 0x1F for v in raw]
    out = []
    for b in raw:
        out += [b & 0x0F, b >> 4]
    return out


def _draw_window(canvas, w, lut, sr, alpha):
    H, W = canvas.shape[:2]
    x0, y0, x1, y1 = w.x0, w.y0, w.x1, w.y1
    if x1 < x0 or y1 < y0:
        return
    h, wd = y1 - y0 + 1, x1 - x0 + 1
    layer = np.zeros((h, wd, 4), np.float32)

    def col(i):
        return np.array(_lut(lut, i), np.float32) / 255.0

    # fill (or gradient)
    if w.gradient:
        base = np.array(_lut(lut, w.color), np.float32)
        rate = 12.8 / ((w.grad_steps + 1) * (w.grad_level + 1))   # LUT levels per line / column
        n = h if w.grad_vertical else wd
        steps = np.arange(n, dtype=np.float32)[:, None] * rate
        sign = np.array([1 if w.grad_pol & b else -1 for b in (4, 2, 1)], np.float32)
        en = np.array([1 if w.grad_en & b else 0 for b in (4, 2, 1)], np.float32)
        ramp = np.clip(base + steps * sign * en, 0, 255) / 255.0
        if w.grad_vertical:
            layer[..., :3] = ramp[:, None, :]
        else:
            layer[..., :3] = ramp[None, :, :]
    else:
        layer[..., :3] = col(w.color)
    layer[..., 3] = alpha if w.blend else 1.0

    shadow = None
    if w.effect:
        ew, eh = w.eff_w, w.eff_h
        if w.etype == 7:                                   # border, in its own color
            c = col(w.bcol)
            layer[:eh, :, :3] = c; layer[-eh:, :, :3] = c
            layer[:, :ew, :3] = c; layer[:, -ew:, :3] = c
        elif w.etype in (4, 5):                            # 3D: raised / sunken
            tl, br = (w.scol, w.bcol) if w.etype == 4 else (w.bcol, w.scol)
            layer[-eh:, :, :3] = col(br); layer[:, -ew:, :3] = col(br)
            layer[:eh, :, :3] = col(tl); layer[:, :ew, :3] = col(tl)
        elif w.etype == 0:                                 # drop shadow, bottom right
            shadow = (ew, eh, col(w.scol))
    if w.round:
        radius, corners = w.round
        fill = _round_fill(sr, radius)
        for y in range(min(radius, h)):
            cut = max(0, radius - fill[y])
            if not cut:
                continue
            cut = min(cut, wd)
            if corners & 0x8:                              # [6] upper-left
                layer[y, :cut, 3] = 0
            if corners & 0x2:                              # [4] upper-right
                layer[y, wd - cut:, 3] = 0
            if corners & 0x4:                              # [5] lower-left
                layer[h - 1 - y, :cut, 3] = 0
            if corners & 0x1:                              # [3] lower-right
                layer[h - 1 - y, wd - cut:, 3] = 0

    def put(lyr, x, y):
        """Windows do not blend with each other: the top window's pixel REPLACES what
        is below (its blend is with the video only), so a translucent body shows no
        shadow window behind it - only around it and through cut corners (seen on
        hardware, 2026-10-11)."""
        ys, xs = max(0, y), max(0, x)
        ye, xe = min(H, y + lyr.shape[0]), min(W, x + lyr.shape[1])
        if ye <= ys or xe <= xs:
            return
        src = lyr[ys - y:ye - y, xs - x:xe - x]
        dst = canvas[ys:ye, xs:xe]
        covered = src[..., 3] > 0
        dst[covered] = src[covered]

    if shadow:
        sw, sh, c = shadow
        sl = np.zeros((h, wd, 4), np.float32)
        sl[..., :3] = c
        sl[..., 3] = alpha if w.blend else 1.0
        put(sl, x0 + sw, y0 + sh)
    put(layer, x0, y0)


def _glyph_rows(font, index, plane_words, plane):
    """18 rows x 12 px bit values of one plane of glyph `index` (plane_words words a plane)"""
    base = FONT_BASE + index * plane_words + plane * 9
    rows = []
    for w in range(9):
        b0, b1, b2 = _word(font, base + w)
        v = b0 | b1 << 8 | b2 << 16
        for half in (v >> 12, v & 0xFFF):
            rows.append([(half >> (11 - c)) & 1 for c in range(CELL_W)])
    return np.array(rows, np.uint8)


def _draw_text(canvas, font, lut, sr):
    H, W = canvas.shape[:2]
    fa = sr[FRAME_CTRL_24]
    tx = (fa >> 4) << 8 | sr[FRAME_CTRL_25]
    ty = (fa & 0x0F) << 8 | sr[FRAME_CTRL_26]
    zone = (sr[FRAME_CTRL_02] >> 1) & 3                    # 0 middle, 1 left, 2 right
    f4b = bool(sr[FRAME_CTRL_07] & 0x04)
    cache = {}
    cell = CHAR_BASE
    y = ty
    for r in range(64):
        b0, b1, b2 = _word(font, r)
        if b0 != 0x80:
            break
        rh, ncells = b1 + 1, b2
        x = tx
        for _ in range(ncells):
            c0, c1, c2 = _word(font, cell)
            cell += 1
            t = c0 & 0xC0
            if t == 0x40:                                  # 1-bit, proportional
                wpx = c0 & 0x0F or CELL_W
                key = (1, c1)
                if key not in cache:
                    cache[key] = _glyph_rows(font, c1, 9, 0)
                g = cache[key]
                start = {0: (CELL_W - wpx) // 2, 1: 0, 2: CELL_W - wpx}.get(zone, 0)
                g = g[:, start:start + wpx]
                fg, bg = c2 >> 4, c2 & 0x0F
                px = np.zeros((CELL_H, wpx, 4), np.float32)
                px[g == 1, :3] = np.array(_lut(lut, fg), np.float32) / 255
                px[g == 1, 3] = 1
                if bg:
                    px[g == 0, :3] = np.array(_lut(lut, bg), np.float32) / 255
                    px[g == 0, 3] = 1
            elif t == 0xC0:                                # 4-bit, 12 px
                wpx = CELL_W
                key = (4, c1)
                if key not in cache:
                    cache[key] = sum(_glyph_rows(font, c1, 36, p).astype(np.uint8) << p for p in range(4))
                v = cache[key]
                px = np.zeros((CELL_H, wpx, 4), np.float32)
                for val in np.unique(v):
                    if val == 0:
                        if f4b:
                            bgi = c0 & 0x0F
                            if not bgi:
                                continue
                            rgb = _lut(lut, bgi)
                        else:
                            rgb = _lut(lut, 0) if (c2 & 3) < 2 else (0, 0, 0)
                    else:
                        rgb = _lut(lut, int(val))
                    m = v == val
                    px[m, :3] = np.array(rgb, np.float32) / 255
                    px[m, 3] = 1
            else:                                          # 2-bit / blank: space only
                wpx = c0 & 0x0F or CELL_W
                px = None
            if px is not None:
                ph = min(rh, CELL_H)
                ys, ye, xs, xe = y, min(H, y + ph), x, min(W, x + wpx)
                if ye > ys and xe > xs and xs >= 0 and ys >= 0:
                    _over(canvas[ys:ye, xs:xe], px[:ye - ys, :xe - xs])
            x += wpx
        y += rh


def render(st):
    """st: the recorded state (osdsim.sim.State) -> Frame"""
    sr, lut = st.sr, st.lut
    canvas = np.zeros((CANVAS[1], CANVAS[0], 4), np.float32)
    level = sr[OVERLAY_CTRL1_M1] >> 3
    alpha = 1.0 - level / 32.0
    for name, base in MAIN_WINDOWS + SUB_WINDOWS:          # sub-windows draw over all main ones
        w = Window(st.win, name, base)
        if w.enabled:
            _draw_window(canvas, w, lut, sr, alpha)
    _draw_text(canvas, st.font, lut, sr)
    scale = 2 if sr[FRAME_CTRL_0A] & 0x03 == 0x03 else 1
    img = (np.clip(canvas, 0, 1) * 255 + 0.5).astype(np.uint8)
    from PIL import Image
    im = Image.fromarray(img, "RGBA")
    bbox = im.getbbox()
    if bbox:
        im = im.crop((0, 0, bbox[2], bbox[3]))             # keep the origin at (0, 0)
    else:
        im = Image.new("RGBA", (1, 1))
    if scale == 2:
        im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
    pos = (st.origin[0] * scale, st.origin[1] * scale)     # 2x doubles the origin delay too
    return Frame(im, pos, scale, st.osd_on)
