# inTouch 15.6": RTD2556, eDP 1920x1080 panel, HDMI + USB-C (DP alt-mode) in.
board_inc = [meson.current_source_dir()]
board_asset_base = '0x020000'      # flash: the asset partition
board_srcs = [
  {'file': files('board_init.c'), 'bank': 0},
  {'file': display_tables_c, 'bank': 0, 'name': 'display_tables'},
  {'file': pipeline_tables_c, 'bank': 0, 'name': 'pipeline_tables'},
]
