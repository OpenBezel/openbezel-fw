# OpenBezel firmware
#
#   meson setup build                         # seeds build/.config from configs/intouch156b_defconfig
#   meson setup build -Ddefconfig=intouch156b_terminal_defconfig
#   ninja -C build intouch156b_terminal_defconfig   # switch an existing build directory
#   ninja -C build                            # firmware images + asset partition
#   ninja -C build menuconfig                 # (or guiconfig) change the configuration
#   ninja -C build savedefconfig              # build/defconfig: the minimal form, for configs/
#   ninja -C build flash                      # write it over DDC (-Di2c_bus=6, or RTD_I2C_BUS)
#
# Needs SDCC >= 4.2, Meson >= 1.1, Ninja, Python >= 3.9 with Pillow and kconfiglib,
# rsvg-convert and the theme fonts - or tools/docker-run (docker/Dockerfile) has them all.
# Generated files go to <build>/gen/.
project('openbezel', meson_version: '>=1.1.0', license: 'MIT')

fs = import('fs')
keyval = import('keyval')
# one Python for every tool: it needs Pillow and kconfiglib (the build container has both)
python = disabler()
foreach cand : ['python3', 'python3.14', 'python3.13', 'python3.12', 'python3.11', 'python3.10', '/usr/bin/python3']
  p = import('python').find_installation(cand, modules: ['PIL', 'kconfiglib'], required: false)
  if p.found() and p.language_version().version_compare('>=3.9')
    python = p
    break
  endif
endforeach
if not python.found()
  error('a Python >= 3.9 with Pillow and kconfiglib is needed (apt install python3-pil ' +
        'python3-kconfiglib), or build in the container: tools/docker-run')
endif

sdcc = find_program('sdcc')
sdld = find_program('sdld')
sdcc_version = run_command(python, '-c',
  'import re, subprocess, sys; o = subprocess.run([sys.argv[1], "--version"], capture_output=True, text=True).stdout; ' +
  'm = re.search(r"(\\d+\\.\\d+\\.\\d+)", o); print(m.group(1) if m else "0")', sdcc.full_path(),
  check: true).stdout().strip()
if sdcc_version.version_compare('<4.2.0')
  error('SDCC >= 4.2 needed (found @0@)'.format(sdcc_version))
endif

tools = meson.project_source_root() / 'tools'

# ---- configuration: Kconfig -> build/.config -> gen/config.h + the choices below --
# Everything the build generates goes to <build>/gen/ (gen/meson.build).
gen_dir = meson.current_build_dir() / 'gen'
kconfig_py = files('tools/kconfig.py')
dotconfig = meson.current_build_dir() / '.config'
if not fs.exists(dotconfig)
  run_command(python, kconfig_py, '--config', dotconfig, 'defconfig',
              meson.project_source_root() / 'configs' / get_option('defconfig'), check: true)
endif
# new Kconfig symbols take their defaults; the file is rewritten only if that changes it
run_command(python, kconfig_py, '--config', dotconfig, 'olddefconfig', check: true)
run_command(python, kconfig_py, '--config', dotconfig, 'genconfig',
            gen_dir / 'config.h', check: true)
run_command(python, kconfig_py, '--config', dotconfig, 'mesonconfig',
            gen_dir / 'config.meson', check: true)
# config.meson: .config unquoted, % and # escaped (keyval reads '#' as a comment)
cfg = keyval.load(gen_dir / 'config.meson')
keyval.load(dotconfig)                # loaded only so a .config edit reconfigures ...
fs.read('Kconfig')                    # ... and so does a Kconfig edit

board = cfg.get('CONFIG_BOARD').strip('"')
chip = cfg.get('CONFIG_CHIP').strip('"')
feature_dcr = cfg.get('CONFIG_FEATURE_DCR', 'n') == 'y'
feature_picture_modes = cfg.get('CONFIG_FEATURE_PICTURE_MODES', 'n') == 'y'

themes = []
foreach t : ['midnight', 'classic', 'minimal', 'terminal', 'custom']
  if cfg.get('CONFIG_THEME_' + t.to_upper(), 'n') == 'y'
    themes += t
  endif
endforeach
if themes.length() == 0
  error('menuconfig: build in at least one menu theme')
endif
theme_args = ['--themes', ','.join(themes), '--default', cfg.get('CONFIG_THEME_DEFAULT').strip('"')]
if themes.contains('custom')
  foreach part : ['icons', 'colors', 'layout', 'frame', 'font']
    theme_args += ['--custom-' + part, cfg.get('CONFIG_THEME_' + part.to_upper(), '""').strip('"')]
  endforeach
  # custom colors: one --color per role that is set
  foreach role : ['panel', 'text', 'text_dim', 'text_sel', 'title', 'value', 'icon_main', 'icon_accent', 'icon_shade', 'icon_hi', 'icon_sel', 'slider', 'panel_fill', 'header_fill', 'header_fill2', 'border', 'border_dark', 'shadow', 'divider', 'sel_fill', 'sel_border', 'footer_fill']
    v = cfg.get('CONFIG_THEME_COLOR_' + role.to_upper(), '').replace('%23', '#').replace('%25', '%').strip()
    if v != ''
      theme_args += ['--color', role + '=' + v]
    endif
  endforeach
endif

langs = ['en']
foreach l : ['de', 'fr', 'es', 'it']
  if cfg.get('CONFIG_LANG_' + l.to_upper(), 'n') == 'y'
    langs += l
  endif
endforeach
lang_default = cfg.get('CONFIG_LANG_DEFAULT', '"en"').strip('"')

# gamma LUTs: computed from the panel's native per-channel gamma
gamma_native = '@0@,@1@,@2@'.format(cfg.get('CONFIG_PANEL_GAMMA_R', '220'),
  cfg.get('CONFIG_PANEL_GAMMA_G', '220'), cfg.get('CONFIG_PANEL_GAMMA_B', '220'))
gamma_args = ['--gamma-native-x100', gamma_native]
gamma_summary = 'computed, native R,G,B x100 = ' + gamma_native

# ---- generated sources and data (<build>/gen/) ---------------------------------------
subdir('gen')

# ---- sources: core, chip, board - in link order, each with its bank ------------------
subdir('core')
subdir('hal/realtek' / chip)
subdir('boards' / board)
srcs = core_srcs + chip_srcs + board_srcs + [{'file': edid_c[0], 'bank': 0, 'name': 'edid_data'}]

# ---- compile ------------------------------------------------------------------------
inc_args = ['-I' + gen_dir]
foreach d : [meson.project_source_root() / 'core'] + chip_inc + board_inc
  inc_args += '-I' + d
endforeach
cflags = chip_cflags + inc_args + ['--include', gen_dir / 'config.h']

rels = []
rel_paths = []
banks = []
foreach s : srcs
  name = s.get('name', '')
  if name == ''
    name = fs.stem(s['file'][0])
  endif
  extra = []
  if s['bank'] > 0
    extra = ['--codeseg', 'BANK@0@'.format(s['bank']), '--constseg', 'BANK@0@'.format(s['bank'])]
    if s['bank'] not in banks
      banks += s['bank']
    endif
  endif
  rels += custom_target(name + '.rel',
    input: s['file'],
    output: name + '.rel',
    depfile: name + '.rel.d',
    depends: [theme, lang, assets, logo],
    command: [python, tools / 'build/sdcc_cc.py', sdcc, '@OUTPUT@', '@DEPFILE@', '--'] + cflags + extra +
             ['@INPUT@'])
  rel_paths += meson.current_build_dir() / name + '.rel'
endforeach

# ---- link + image -------------------------------------------------------------------
ldflags = chip_cflags
bank_args = []
bank_outputs = []
foreach b : banks
  ldflags += '-Wl-bBANK@0@=0x@0@@1@'.format(b, chip_bank_split)
  bank_outputs += 'openbezel_bank@0@.bin'.format(b)
  bank_args += '@0@:@1@'.format(b, meson.current_build_dir() / 'openbezel_bank@0@.bin'.format(b))
endforeach

ihx = custom_target('openbezel.ihx',
  input: rels,
  output: ['openbezel.ihx'],
  command: [python, tools / 'build/sdcc_link.py', sdcc, sdld, '@OUTPUT0@', '--'] + ldflags + ['--', '@INPUT@'])

image = custom_target('image',
  input: [ihx, version],
  output: ['openbezel.bin'] + bank_outputs,
  build_by_default: true,
  command: [python, tools / 'build/image.py', meson.current_build_dir() / 'openbezel.map', '@INPUT0@',
            '@INPUT1@', '0x' + chip_bank_split, '@OUTPUT0@', '--'] + rel_paths + ['--'] + bank_args)

alias_target('firmware', image, assets)

# ---- configuration UI -----------------------------------------------------------------
foreach t : ['menuconfig', 'guiconfig', 'savedefconfig']
  run_target(t, command: [python, kconfig_py, '--config', dotconfig, t])
endforeach
# `ninja <name>_defconfig`: load configs/<name>_defconfig into this build directory
# (the kernel's `make <name>_defconfig`); the next build reconfigures from it
configs_dir = meson.project_source_root() / 'configs'
defconfigs = run_command(python, '-c',
  'import os, sys; print(" ".join(sorted(f for f in os.listdir(sys.argv[1]) if f.endswith("_defconfig"))))',
  configs_dir, check: true).stdout().strip().split()
foreach d : defconfigs
  run_target(d, command: [python, kconfig_py, '--config', dotconfig, 'defconfig', configs_dir / d])
endforeach

# ---- flash ---------------------------------------------------------------------------
flash_args = []
if get_option('i2c_bus') != ''
  flash_args = ['--bus', get_option('i2c_bus')]
endif
flash_images = [assets[0].full_path() + '@' + board_asset_base]
foreach b : banks
  flash_images += meson.current_build_dir() / 'openbezel_bank@0@.bin@0x0@0@0000'.format(b)
endforeach
flash_images += meson.current_build_dir() / 'openbezel.bin@0'
# Code and assets in ONE ISP session, banks before bank 0: a reset
# between them would boot a mix of two builds.
run_target('flash', depends: [image, assets],
  command: [python, tools / 'rtdflash.py'] + flash_args + ['flash', '--sync'] + flash_images)
run_target('flash-assets', depends: [assets],
  command: [python, tools / 'rtdflash.py'] + flash_args +
           ['flash', assets[0].full_path(), '--addr', board_asset_base, '--sync'])

summary({'board': board, 'chip': chip, 'themes': ', '.join(themes) + ' (default ' + cfg.get('CONFIG_THEME_DEFAULT').strip('"') + ')', 'languages': ' '.join(langs),
         'default language': lang_default, 'banks': banks,
         'gamma': gamma_summary, 'EDID': edid_summary},
        section: 'Configuration (.config)')
