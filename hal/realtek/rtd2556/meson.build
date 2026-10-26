# RTD2556: 8051 core, code from SPI flash banked through CODE_PAGE
# XRAM 0xD700-0xF6FF (the linker gets 0xD700-0xDFFF).
chip_cflags = ['-mmcs51', '--model-large', '--std-sdcc11',
               '--code-loc', '0x0000', '--code-size', '0x100000',
               '--xram-loc', '0xD700', '--xram-size', '0x0900',
               '--iram-size', '0x100', '--opt-code-size']
# COMMON is 0x0000 - split-1, replicated into every bank image; banked code is
# linked at (n << 16) | split.
chip_bank_split = 'B000'
chip_inc = [meson.current_source_dir(), meson.current_source_dir() / 'regs',
            meson.project_source_root() / 'hal/realtek/common']
# Link order matters (direct RAM): dp_rx_link.c last.
chip_srcs = [
  {'file': files('startup.c'), 'bank': 0},
  {'file': files('ddc.c'), 'bank': 0},
  {'file': files('delay.c'), 'bank': 0},
  {'file': files('edp_tx.c'), 'bank': 0},
  {'file': files('display.c'), 'bank': 0},
  {'file': files('osd.c'), 'bank': 0},
  {'file': files('hdmi_rx.c'), 'bank': 4},
  {'file': files('pipeline.c'), 'bank': 0},
  {'file': files('flash_assets.c'), 'bank': 0},
  {'file': files('backlight.c'), 'bank': 0},
  {'file': files('ddcci_hw.c'), 'bank': 0},
  {'file': files('flash_rw.c'), 'bank': 0},
  {'file': files('audio.c'), 'bank': 0},
  {'file': files('dp_rx.c'), 'bank': 0},
  {'file': files('hdcp.c'), 'bank': 4},
  {'file': files('dp_rx_link.c'), 'bank': 6},
]
