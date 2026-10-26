"""Input signals as text: WxH@Hz[/HTOTALxVTOTAL] or none."""


def parse_signal(text):
    if text == "none":
        return dict(up=False)
    mode, _, totals = text.partition("/")
    wh, _, hz = mode.partition("@")
    w, h = (int(v) for v in wh.lower().split("x"))
    d = dict(up=True, w=w, h=h, hz=float(hz or 60))
    if totals:
        d["htotal"], d["vtotal"] = (int(v) for v in totals.lower().split("x"))
    else:
        d["htotal"], d["vtotal"] = STD_TOTALS.get((w, h), (w + max(160, w // 7), h + max(45, h // 24)))
    return d


# CEA-861 / DMT totals for the common modes (others: CVT-ish blanking)
STD_TOTALS = {(640, 480): (800, 525), (800, 600): (1056, 628), (1024, 768): (1344, 806),
              (1280, 720): (1650, 750), (1280, 1024): (1688, 1066), (1440, 900): (1904, 934),
              (1680, 1050): (2240, 1089), (1920, 1080): (2200, 1125), (2560, 1440): (2720, 1481),
              (3840, 2160): (4400, 2250)}
