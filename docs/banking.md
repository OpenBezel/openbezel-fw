# Code banking

The RTD2556 maps one 64 KB flash page into the
8051 code space at a time: CODE_PAGE (XDATA 0xFFFF) selects page N = flash
N * 64 KB, once BANK_SWITCH_CONTROL (0xFFFC) has bank_en + sw_mode set (0x1F,
without them CODE_PAGE writes do nothing).  The WHOLE window
switches - there is no fixed common area in hardware - so every
bank image carries an identical copy of the common region.

## Layout

| code address | contents |
|---|---|
| 0x0000 - BANK_SPLIT-1 (0xAFFF) | COMMON, same bytes in every bank image |
| BANK_SPLIT - 0xFEFF | the bank's own window (banked code) |
| 0xFF00 - 0xFF0F | build-info block (mkimage), every bank |

| flash | bank |
|---|---|
| 0x000000 | bank 0: common + (empty window) - the boot ROM runs this |
| 0x010000 | stock bank 1: HDCP keys - never written, not used as a code bank |
| 0x020000 | asset partition (capped at 0x02FFFF) |
| 0x030000 | bank 3: DDC/CI (mccs.c), menu (menu.c) |
| 0x040000 | bank 4: source / input state machines (source.c), HDMI receiver (hdmi_rx.c), HDCP (hdcp.c) |
| 0x050000 | bank 5: the graphical menu renderer (gui.c) |
| 0x060000 | bank 6: DP receiver outside the INT0 handler - bring-up, post-training, stream, audio (dp_rx_link.c) |

## What MUST be common

* the recovery path: boot up to the ISP break-in window - DDC, EDID, HPD,
  board init, `delay_ms` (mkimage.py checks SAFE_BOOT_SYMBOLS).  A blank
  or corrupt bank can then never block ISP recovery: nothing banked runs
  before `bringup()`, which is after the window;
* the INT0 handler and everything it calls (dp_rx.c's training code) - the
  vector lands in whatever bank is mapped.  The rest of the DP receiver is in
  bank 6 (dp_rx_link.c); the two share dp_rx_int.h;
* the trampolines (`__sdcc_banked_call` / `__sdcc_banked_ret`, startup.c) and
  SDCC's library helpers;
* anything called through a function pointer (the menu's settings_apply_*
  callbacks) - a pointer is a 16-bit address in the CURRENT bank;
* `const` data that more than one bank reads.  A banked module's own constants
  compile with `--constseg BANKn` and live in its window, so
  they are only valid while that bank is mapped: they must stay `static`, and
  no pointer to one may be handed to another bank (common code called FROM
  the bank is fine - the bank is still mapped).  Statics are not in the map,
  so check_banking.py cannot see a violation.

## Mechanics

* Sources tagged `'bank': n` in their directory's meson.build compile with `--codeseg BANKn --constseg BANKn` and link
  at (n << 16) | BANK_SPLIT.  Their public functions are declared and defined
  `__banked`; static helpers are plain calls within the bank.
* A banked call: r0:r1:r2 = target (lo, hi, bank); the trampoline saves the
  caller's bank (XDATA shadow `code_page_shadow` - direct RAM is full),
  writes CODE_PAGE and `ret`s into the target, which returns through
  `__sdcc_banked_ret`.  A few us per call: keep banked calls out of the
  tightest loops.
* sdld's memory summary (-M) rejects code above 0xFFFF, so the link runs
  sdld on SDCC's linker script without -M (tools/build/sdcc_link.py).
* tools/check_banking.py fails the build on a direct lcall/ljmp into another
  bank or a banked function's address being taken (= a missing __banked).
* tools/mkimage.py --split / --bank N:file builds every bank image as common +
  window and checks the windows and the common-only symbols.
* If a bank misbehaves, `rtdflash.py dump --addr` it and cmp against its .bin:
  a bank whose common copy is stale runs that stale code on the first call into it.
* Flash the banks BEFORE bank 0 (`ninja -C build flash` does): a new bank 0
  must never boot against an old bank.  All images of one build go together -
  the common copies differ between builds.

## Direct RAM

Internal RAM (0x00-0x7F) is full: register bank, 2 bytes of bits at 0x20, ~100 bytes
of DSEG (mostly SDCC spill temporaries, `*_sloc*`), and hdmi_rx's 11-byte overlay
(OSEG), which needs 11 CONSECUTIVE bytes.  sdld places each module's DSEG in link
order, so the fit depends on the order of the source lists (core/, hal/realtek/<chip>/,
boards/<board>/ meson.build) - dp_rx_link.c fits only last.  When "Could not get N consecutive bytes ... OSEG" appears: rewrite the
new code's compound expressions on __xdata temporaries (no `&&`/`||`/`?:` producing
values), then try the module's position in the list.

## Other rules

* Big constant data goes in the asset partition, not `__code`.  `board_edid[]` stays
  `__code`: serving it IS the recovery path.
* No function pointers across modules.
