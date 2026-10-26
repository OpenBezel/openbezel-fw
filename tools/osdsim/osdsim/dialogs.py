"""Named dialogs: how to bring each OSD screen up from a reset simulator, using
only what a user could do (keys) plus the source events the firmware reacts to."""

# main-page order, core/menu.c page_main[]
PAGES = ["picture", "audio", "display", "osd", "system", "info"]


def open_page(sim, page):
    sim.open_menu()
    for _ in range(PAGES.index(page)):
        sim.key("down")
    sim.key("enter")


def _menu(sim):
    sim.open_menu()


def _page(name):
    return lambda sim: open_page(sim, name)


def _adjust(sim):
    """editing Brightness: in the row or as a popup, by the Slider setting"""
    open_page(sim, "picture")
    sim.key("down")                     # Picture mode -> Brightness
    sim.key("enter")
    sim.key("up")                       # one step, so the value shows a change


def _volume(sim):
    sim.key("up")                       # hotkey: the volume bar


def _input(sim):
    sim.key("power")                    # hotkey: the input picker


def _mode(sim):
    sim.popup_mode()


def _nosignal(sim):
    sim.set_signal(up=False)
    sim.popup_nosignal()


DIALOGS = {
    "menu": _menu,
    "picture": _page("picture"), "audio": _page("audio"), "display": _page("display"),
    "osd": _page("osd"), "system": _page("system"), "info": _page("info"),
    "adjust": _adjust,
    "volume": _volume,
    "input": _input,
    "mode": _mode,
    "nosignal": _nosignal,
}


def show(sim, name):
    DIALOGS[name](sim)
