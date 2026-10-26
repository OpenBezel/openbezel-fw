# Core sources, in link order. bank 0 = common; n = area BANKn.
core_srcs = [
  {'file': files('main.c'), 'bank': 0},
  {'file': files('dbgmon.c'), 'bank': 0},
  {'file': files('console.c'), 'bank': 0},
  {'file': files('keys.c'), 'bank': 0},
  {'file': files('settings.c'), 'bank': 0},
  {'file': files('menu.c'), 'bank': 3},
  {'file': files('source.c'), 'bank': 4},
  {'file': files('timebase.c'), 'bank': 0},
  {'file': files('mccs.c'), 'bank': 3},
  {'file': files('settings_store.c'), 'bank': 0},
  {'file': files('gui.c'), 'bank': 5},
  {'file': files('lang.c'), 'bank': 0},
]
if feature_dcr
  core_srcs += {'file': files('dcr.c'), 'bank': 5}
endif
if feature_picture_modes
  core_srcs += {'file': files('picmode.c'), 'bank': 3}
endif
