#include "dbgmon.h"
#include "rtd2556_regs.h"

/* Page-0 OSD port (OSD_ADR_PORT_MSB/LSB 0x0090/91, OSD_DATA_PORT 0x0092,
 * OSD_WRITE_OPTION 0x0094).  Scaler pages 0x00-0x2F are flat-mapped into
 * XDATA 0x0000-0x2FFF, so the firmware needs no page select to reach it. */
#define OSD_ADDR_H SR(OSD_ADR_PORT_MSB)
#define OSD_ADDR_L SR(OSD_ADR_PORT_LSB)
#define OSD_DATA SR(OSD_DATA_PORT)
#define OSD_WRITE_OPT SR(OSD_WRITE_OPTION)

/* bytesel=0b111 [7:5], sram_sel=font SRAM (0b10 in bits [3:2]) = 0xE8 */
#define WRITE_ALL_BYTES_FONT (OSD_WRITE_OPTION_BYTESEL | OSD_SRAM_FONT)

static u8 heartbeat_count;
static u8 checkpoint_id;

/* Debug state goes to the font SRAM below (read over DDC-ISP). */

static void mb_write(u16 word, u8 b0, u8 b1, u8 b2)
{
    OSD_WRITE_OPT = WRITE_ALL_BYTES_FONT;
    OSD_ADDR_H = HI(word) & OSD_ADR_PORT_MSB_OSD_ADDR_MSB;
    OSD_ADDR_L = LO(word);
    OSD_DATA = b0;
    OSD_DATA = b1;
    OSD_DATA = b2;
    /* Deselect the SRAM when idle (RE-documented glitch avoidance, and it
     * leaves the port closer to how the host tools expect to find it). */
    OSD_WRITE_OPT = 0x00;
}

static void mb_write_state(void)
{
    mb_write(MB_WORD_BASE, MB_MAGIC_ALIVE, heartbeat_count, checkpoint_id);
}

void dbgmon_init(void)
{
    heartbeat_count = 0;
    checkpoint_id = 0;
    mb_write(MB_WORD_BASE + 1, 0, 0, 0); /* clear the key-poll word */
    mb_write_state();
}

void dbgmon_checkpoint(u8 id)
{
    checkpoint_id = id;
    mb_write_state();
}

#ifdef CONFIG_DEBUG_CONSOLE
u8 dbgmon_last_checkpoint(void)
{
    return checkpoint_id;
}
#endif

void dbgmon_heartbeat(void)
{
    heartbeat_count++;
    mb_write_state();
}

void dbgmon_diag(u8 word, u8 b0, u8 b1, u8 b2)
{
    mb_write(MB_WORD_BASE + word, b0, b1, b2);
}

