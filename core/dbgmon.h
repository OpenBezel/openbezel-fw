/* Debug monitor - firmware side.
 *
 * Transport (RTD2556): OSD FONT SRAM words MB_WORD_BASE..+0x0F, written via
 * the page-0 OSD port (XDATA 0x0090-0x0094).  This is the only RAM both the
 * 8051 and the DDC host can reach AND read back.
 *
 * Word layout (3 bytes per OSD word) - MUST match tools/rtdlib/monitor.py:
 *   word +0x000: MAGIC(0x5A) | HEARTBEAT | CHECKPOINT
 *   word +0x001: key polls | lowest A0 | UI mode (main.c; cleared at init)
 *   words +0x002..+0x00F: scratch diagnostics (dbgmon_diag)
 *
 * Halt-and-inspect only: the host reads it in ISP mode, with osd_en off.
 * Live reads go through the DDC/CI debug peek instead.
 */
#ifndef DBGMON_H
#define DBGMON_H

#include "ob_types.h"

#define MB_WORD_BASE 0x1F00   /* in the font SRAM (0x2800 words), clear of what the OSD uses */
#define MB_MAGIC_ALIVE 0x5A

/* Checkpoint ids: written BEFORE each stage runs, so a hang is
 * attributable to the stage. */
#define CP_MAIN_LOOP 0x10

void dbgmon_init(void);
void dbgmon_checkpoint(u8 id);
u8 dbgmon_last_checkpoint(void);
void dbgmon_heartbeat(void);
/* scratch diagnostics: write any 3 bytes into mailbox word 2..15 */
void dbgmon_diag(u8 word, u8 b0, u8 b1, u8 b2);

#endif
