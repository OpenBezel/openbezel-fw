# Everything the build generates, into <build>/gen/: nothing here is in source control.

# EDID: generated from the board's description, or a binary (stock / custom)
board_dir = meson.project_source_root() / 'boards' / board
if cfg.get('CONFIG_EDID_OPENBEZEL', 'n') == 'y'
  edid_args = ['--json', board_dir / 'edid.json', '--check']
  edid_summary = 'OpenBezel (edid.json)'
else
  edid_bin = cfg.get('CONFIG_EDID_STOCK', 'n') == 'y' ? board_dir / 'edid.bin' : cfg.get('CONFIG_EDID_FILE', '""').strip('"')
  if edid_bin == '' or not fs.is_file(edid_bin)
    error('CONFIG_EDID_FILE "@0@" is not a file'.format(edid_bin))
  endif
  edid_args = ['--bin', edid_bin, '--check']
  if cfg.get('CONFIG_EDID_NO_DEEP_COLOR', 'n') == 'y'
    edid_args += '--no-deep-color'
  endif
  edid_summary = edid_bin
endif
edid_c = custom_target('edid_data.c',
  output: ['edid_data.c', 'edid.bin'],
  depfile: 'edid_data.d',
  command: [python, tools / 'gen_edid.py'] + edid_args +
           ['--out', '@OUTPUT0@', '--out-bin', '@OUTPUT1@', '--depfile', '@DEPFILE@'])

# burst-loaded pipeline SRAM tables (filters, input dither) and the display dither tables
pipeline_tables_c = custom_target('pipeline_tables.c',
  output: 'pipeline_tables.c',
  depend_files: files('../tools/gen_pipeline_tables.py', '../tools/ditherlib.py'),
  command: [python, tools / 'gen_pipeline_tables.py', '--out', '@OUTPUT@'])
display_tables_c = custom_target('display_tables.c',
  output: 'display_tables.c',
  depend_files: files('../tools/gen_display_tables.py', '../tools/ditherlib.py'),
  command: [python, tools / 'gen_display_tables.py', '--out', '@OUTPUT@'])

# fonts, icon sets and the boot logo, rendered from themes/ and images/ (Pillow,
# rsvg-convert, the fonts installed): every font and icon set, so any theme finds its glyphs
find_program('rsvg-convert', required: true)       # gen_icons.py / gen_logo.py call it

themes_dir = meson.project_source_root() / 'themes'
list_dirs = 'import os, sys; d = sys.argv[1]; print(" ".join(sorted(n for n in os.listdir(d) if os.path.isfile(os.path.join(d, n, sys.argv[2])))))'
list_files = 'import os, sys; d = sys.argv[1]; print("\\n".join(sorted(n for n in os.listdir(d) if n.endswith(tuple(sys.argv[2:])))))'
glyph_targets = []
foreach f : run_command(python, '-c', list_dirs, themes_dir / 'fonts', 'font.json', check: true).stdout().strip().split()
  d = themes_dir / 'fonts' / f
  ins = []
  foreach n : run_command(python, '-c', list_files, d, '.json', '.ttf', '.otf', check: true).stdout().strip().split('\n')
    ins += d / n
  endforeach
  glyph_targets += custom_target('font_' + f + '.c',
    input: ins,
    output: 'font_' + f + '.c',
    depfile: 'font_' + f + '.d',
    command: [python, tools / 'gen_font.py', d, '--out', '@OUTPUT@', '--depfile', '@DEPFILE@'])
endforeach
foreach s : run_command(python, '-c', list_dirs, themes_dir / 'icons', 'set.json', check: true).stdout().strip().split()
  d = themes_dir / 'icons' / s
  ins = []
  foreach n : run_command(python, '-c', list_files, d, '.json', '.svg', check: true).stdout().strip().split('\n')
    ins += d / n
  endforeach
  glyph_targets += custom_target('icons_' + s + '.c',
    input: ins,
    output: 'icons_' + s + '.c',
    depend_files: files('../tools/gen_icons.py', '../tools/themelib.py'),
    command: [python, tools / 'gen_icons.py', d, '--out', '@OUTPUT@'])
endforeach
logo = custom_target('logo',
  input: meson.project_source_root() / 'images' / 'openbezel_wordmark.svg',
  output: ['logo.c', 'logo.h'],
  depend_files: files('../tools/gen_logo.py'),
  command: [python, tools / 'gen_logo.py', '--svg', '@INPUT@', '--out', '@OUTPUT0@'])

# theme, languages, asset partition
theme = custom_target('theme',
  output: ['theme.h', 'themes.bin', 'themes.json'],
  depfile: 'theme.d',
  depends: glyph_targets,
  command: [python, tools / 'gen_theme.py'] + theme_args +
           ['--glyphs-dir', meson.current_build_dir(), '--out-header', '@OUTPUT0@', '--out-bin', '@OUTPUT1@',
            '--out-json', '@OUTPUT2@', '--depfile', '@DEPFILE@'])

lang = custom_target('lang',
  input: theme[2],
  output: ['lang_ids.h', 'lang.bin'],
  depfile: 'lang.d',
  command: [python, tools / 'gen_lang.py', '--lang-dir', meson.project_source_root() / 'lang',
            '--langs'] + langs + ['--default', lang_default, '--themes-json', '@INPUT@',
            '--out-header', '@OUTPUT0@', '--out-bin', '@OUTPUT1@', '--depfile', '@DEPFILE@'])

assets = custom_target('assets',
  input: [theme[1], lang[1], logo[0]],
  output: ['assets.bin', 'assets.h'],
  depfile: 'assets.d',
  command: [python, tools / 'gen_assets.py'] + gamma_args +
           ['--logo', '@INPUT2@', '--blob', 'THEMES=@INPUT0@', '--blob', 'LANG=@INPUT1@',
            '--out-bin', '@OUTPUT0@', '--out-header', '@OUTPUT1@', '--depfile', '@DEPFILE@'])

version = vcs_tag(input: '../tools/build/version.in', output: 'version.txt',
                  command: ['git', 'describe', '--always', '--dirty'], fallback: 'dev')
