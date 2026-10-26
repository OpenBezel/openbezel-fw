/* Persistent settings: a two-sector append-only log in the upper, writable
 * half of the SPI flash (hal/realtek/rtd2556/flash_rw.h).
 *
 * A record is appended per change, and a sector is only erased when the
 * other one fills and the live record moves across.  There is one record
 * type - the whole settings struct - so "move the live records" is just
 * "write the newest one into the freshly erased sector".
 *
 * Record (64 bytes, never crossing a flash page):
 *   0-1  'O' 'B'          magic
 *   2    STORE_VERSION    layout of the payload; a mismatch loads defaults
 *   3    payload length   = sizeof(struct ob_settings)
 *   4-5  sequence         little endian, +1 per save, newest wins (serial
 *                         arithmetic, so it may wrap)
 *   6..  payload
 *   63   CRC-8 (poly 0x07) over bytes 0-62
 *
 * Power-loss safety: a record is only ever programmed into erased space, and
 * the only sector ever erased is the one NOT holding the newest record.  A
 * torn write fails its CRC and the previous record stands; a torn erase
 * leaves the newest record untouched in the other sector. */
#ifndef OB_SETTINGS_STORE_H
#define OB_SETTINGS_STORE_H

#include "ob_types.h"
#include "settings.h"

#define STORE_OK 0
#define STORE_NONE 0x10         /* load: no valid record anywhere */
/* otherwise a FLASH_E_* code from flash_rw.h */

/* Scan both sectors; newest valid record into *out.  STORE_OK or STORE_NONE.
 * Also sets up where the next save goes. */
u8 store_load(__xdata struct ob_settings *out);

/* Append *s unless it equals what was last saved/loaded.  Requires a
 * store_load() first (it establishes the write position). */
u8 store_save(const __xdata struct ob_settings *s);

/* For the debug console / DDC/CI: active sector (0/1, 0xFF none), next free
 * slot, sequence of the newest record. */
extern u8 store_sector, store_slot;
extern u16 store_seq;
extern u8 store_last;           /* result of the last store_save(), 0xFF = none yet */

#endif
