/* DP receiver internals shared by dp_rx.c (unbanked: the INT0 link-training handler)
 * and dp_rx_link.c (banked: port bring-up, the post-training retune, the stream
 * and audio).  Not for other modules. */
#ifndef OB_DP_RX_INT_H
#define OB_DP_RX_INT_H

#include "dp_rx.h"

/* { addr, mask, val }: mask 0 = write val; else reg = (reg & mask) | val */
struct reg_op {
    u16 addr;
    u8 mask;
    u8 val;
};

#define LT_IDLE       0x00
#define LT_CR_OK      0x01
#define LT_CR_RETRY   0x02
#define LT_EQ_OK      0x05
#define LT_EQ_RETRY   0x06
#define LT_LINK_UP    0x09
#define LT_FAILED     0x0C
#define LT_TRAINED    0x0E      /* training ended; the post-LT retune is due */

#define RATE_RBR 0x06
/* The D1 PHY's per-lane DFE blocks: D1_L0_* (7AA0), D1_L1_* (7AB0) ... D1_L3_*,
 * the same layout, LANE_STEP apart.  LN(D1_L0_reg) is a register's offset in
 * its block; PHY_LANE(l, offset) the register for lane l. */
#define LANE_STEP (D1_L1_DFE_EN_1 - D1_L0_DFE_EN_1)
#define LN(reg) ((u8)((reg) - D1_L0_DFE_EN_1))
#define PHY_LANE(l, off) (D1_L0_DFE_EN_1 + (u16)(l) * LANE_STEP + (off))

/* link-training state: written by the ISR, read and reset by the main loop */
extern volatile u8 lt_state;
extern volatile u8 lt_lanes;    /* 1, 2 or 4 */
extern volatile u8 lt_rate;     /* DPCD 100 */
extern u8 lt_enh;               /* DPCD 101 bit 7 -> B001 enhanced framing */
extern u16 lt_trained_t0;       /* main loop: when LT_TRAINED was seen */
extern u8 pw_save[3];           /* DPCD 202-204 across SET_POWER D3 */
extern volatile u16 irq_burst;   /* INT0 entries since the last main-loop pass */

/* busy-wait (dp_rx.c): the ISR's, and post_lt's with INT0 held off - its parameter
 * lives in fixed XDATA, so the two must never overlap */
void i_wait(u16 n);
#define i_units(n) i_wait((u16)(n) * 6)
#define i_short(n) i_wait((u16)(n) * 7)

#endif
