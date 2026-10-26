/* Front-panel keys on the KEY-V1.0 board, read through the RTD2556 LSADC.
 *
 * Two resistor-ladder channels, two keys each:
 *
 *   channel        key                pressed   idle
 *   A1 (0xFF0A)    UP (K1)            0x08      0xF8
 *   A1 (0xFF0A)    DOWN (K3)          0xE8      0xF8
 *   A0 (0xFF09)    PWR (K4)           0x08      0xF8
 *   A0 (0xFF09)    ENTER (K2)         0xE8      0xF8
 *
 * DOWN/ENTER sit only 16 counts below idle, so a press needs several
 * consecutive in-window samples before it counts.  Events fire on PRESS
 * (see keys.c for why not release), then auto-repeat for UP/DOWN.
 */
#ifndef OB_KEYS_H
#define OB_KEYS_H

#include "ob_types.h"

#define KEY_NONE 0
#define KEY_UP 1
#define KEY_DOWN 2
#define KEY_ENTER 3
#define KEY_POWER 4

void keys_init(void);

/* Poll the ladder and return one KEY_* per press, on release.  Returns
 * KEY_NONE when a conversion is still running, so it is cheap to call often;
 * an ADC_ACONTROL read mid-conversion stalls the MCU ~1.5 ms, which is why
 * the caller should still rate-limit it. */
u8 keys_poll(void);

/* Auto-repeat for UP/DOWN while held: the first repeat after `delay`, then
 * every `rate`, both in Timer0 overflows (tb_ms()), NOT in poll samples -
 * keys_poll() runs at whatever rate the main loop allows, and a busy loop
 * (a gamma reload) must not slow the repeat down.  delay 0 = off. */
void keys_set_repeat(u16 delay, u16 rate);

/* Raw last-sampled ladder values, for the debug console. */
extern u8 keys_a0, keys_a1;

#endif /* OB_KEYS_H */
