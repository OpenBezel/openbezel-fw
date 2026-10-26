"""ctypes wrapper around one built configuration (osdsim.build)."""
import ctypes

from .build import Config, build
from .chips import chip_for

KEYS = {"up": 1, "down": 2, "enter": 3, "power": 4}


class State:
    """the recorded OSD state, copied out of the library"""


class Sim:
    def __init__(self, cfg=None, **kw):
        self.cfg = cfg or Config(**kw)
        self.chip = chip_for(self.cfg.board)
        lib_path, assets = build(self.cfg)
        self.lib = ctypes.CDLL(lib_path)
        if self.lib.osdsim_load_assets(assets.encode()):
            raise RuntimeError("cannot load " + assets)
        self.lib.osdsim_setting_name.restype = ctypes.c_char_p
        self.reset()

    # ---- control -------------------------------------------------------------
    def reset(self):
        self.lib.osdsim_reset()
        self.set_signal()

    def key(self, name):
        self.lib.osdsim_key(KEYS[name.lower()])

    def open_menu(self):
        self.lib.osdsim_open_menu()

    def close(self):
        self.lib.osdsim_close()

    def popup_mode(self):
        self.lib.osdsim_popup_mode()

    def popup_nosignal(self):
        self.lib.osdsim_popup_nosignal()

    def tick(self, ms):
        self.lib.osdsim_tick(int(ms))

    def menu_active(self):
        return bool(self.lib.osdsim_menu_active())

    def set_signal(self, input="hdmi", up=True, w=1920, h=1080, hz=60.0, htotal=2200, vtotal=1125, pclk_mhz=None):
        if pclk_mhz is None:
            pclk_mhz = htotal * vtotal * hz / 1e6
        self.lib.osdsim_set_signal(1 if input == "usbc" else 0, 1 if up else 0, w, h, int(round(hz * 10)),
                                   htotal, vtotal, int(round(pclk_mhz * 10)))

    # ---- settings ------------------------------------------------------------
    def settings(self):
        out, i = {}, 0
        while True:
            n = self.lib.osdsim_setting_name(i)
            if not n:
                return out
            out[n.decode()] = self.get(n.decode())
            i += 1

    def get(self, name):
        v = ctypes.c_int()
        if self.lib.osdsim_setting_get(name.encode(), ctypes.byref(v)):
            raise KeyError(name)
        return v.value

    def set(self, name, value):
        if name == "language" and isinstance(value, str):
            value = self.cfg.langs.index(value)
        if self.lib.osdsim_setting_set(name.encode(), int(value)):
            raise KeyError(name)

    # ---- output --------------------------------------------------------------
    def state(self):
        st = State()
        st.sr = bytes((ctypes.c_uint8 * 65536).in_dll(self.lib, "osdsim_sr"))
        for name, size in self.chip.STATE_ARRAYS.items():
            setattr(st, name, bytes((ctypes.c_uint8 * size).in_dll(self.lib, "osdsim_" + name)))
        st.origin = (ctypes.c_uint16.in_dll(self.lib, "osdsim_origin_x").value,
                     ctypes.c_uint16.in_dll(self.lib, "osdsim_origin_y").value)
        st.osd_on = bool(ctypes.c_uint8.in_dll(self.lib, "osdsim_osd_on").value)
        return st

    def render(self):
        return self.chip.render(self.state())

    def panel_image(self, frame=None, background=None):
        """the whole panel: the OSD placed where it sits on screen; transparent where
        there is no OSD unless `background` (a PIL image or an RGB tuple) is given"""
        from PIL import Image
        frame = frame or self.render()
        W, H = self.chip.PANEL
        if background is None:
            base = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        elif isinstance(background, tuple):
            base = Image.new("RGBA", (W, H), background + (255,))
        else:
            base = background.convert("RGBA").resize((W, H))
        if frame.enabled:
            layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
            layer.paste(frame.image, frame.pos)
            base = Image.alpha_composite(base, layer)
        return base
