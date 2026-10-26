/* RTD2556 eDP transmitter - TX1 (scaler pages 0x9C/0x9D + PHY0 0x38/0x39/0x55).
 *
 * The panel is wired to the SECOND DisplayPort transmitter: page 0x9C (MAC,
 * HPD, SFIFO, digital PHY control), page 0x9D (AUX, same layout as page BD)
 * and the "PHY0" serializer/analog PHY + CMU in pages 0x38/0x39/0x55.  Pages
 * BB/BC/BD are a separate, unconnected TX.
 *
 * Link parameters are compile-time: HBR x 2 lanes,
 * enhanced framing, downspread, eDP ASSR - what the intouch156b panel
 * supports (DPCD caps 11 0A 82 41 ...).
 */
#ifndef RTD2556_EDP_TX_H
#define RTD2556_EDP_TX_H

#include "ob_types.h"

/* AUX transaction results */
#define AUX_OK 0
#define AUX_ERR_TIMEOUT 1   /* engine never raised auxdone */
#define AUX_ERR_NACK 2      /* sink NACK/DEFER (reply cmd != ACK) */
#define AUX_ERR_SHORT 3     /* read returned fewer bytes than requested */

/* Link-training results (edp_link_train) */
#define EDP_LT_OK 0
#define EDP_LT_ERR_PLL 0x10        /* PHY0 CMU never reported clock ready */
#define EDP_LT_ERR_AUX_CAPS 0x20   /* DPCD receiver caps unreadable */
#define EDP_LT_ERR_AUX_CFG 0x21    /* link config writes failed */
#define EDP_LT_ERR_CR 0x30         /* clock recovery never completed */
#define EDP_LT_ERR_EQ 0x40         /* channel equalization failed */
#define EDP_LT_ERR_SILENT 0x50     /* panel answers but DPCD reads all zero (clock/power gap) */

/* Snapshot of the last training attempt, for the debug monitor. */
struct edp_status {
    u8 caps[16];     /* DPCD 0x000-0x00F */
    u8 lane[6];      /* DPCD 0x202-0x207 (status + adjust request) */
    u8 cr_tries;
    u8 eq_tries;
    u8 aux_err;      /* last AUX error code */
    u8 pll_stat;     /* PHY0 CMU status 38D4 after bring-up (bit7 = locked) */
    u8 aux_dbg[4];   /* at last AUX failure: 9DB1 irq, 9DA2 fsm, 9DAC fifo, 9DA3 ctrl */
    u8 mac_fsm_max;  /* highest 9DA2[3:0] MAC state seen in the last transaction */
    u8 phy_fsm_max;  /* highest 9DF8[2:0] TX PHY state seen (0 = never left idle) */
    u8 phy_flags;    /* 9DF5[6] toggle-seen | 9DF7[3:0] PHY errors */
    u8 groupa_polls; /* 3900 handshake polls (0xFF = never acknowledged) */
    u8 mvid[3];      /* MVID auto measurement used as M (9CAF-9CB1) */
};

extern struct edp_status edp_stat;

void edp_tx_init(void);          /* clock select, power domain, TX1 AUX PHY + HPD */
void edp_panel_power_cycle(void); /* park PHY0, drop panel VCC, delay, restore (real edge) */
void edp_phy0_on(void);          /* PHY0 pages 38/39/55 + PHY_RST release */
u8 edp_read_caps(void);          /* DPCD 0x000-0x00F into edp_stat.caps */
u8 edp_aux_read(u8 addr_hi, u16 addr, u8 len, u8 *buf);
u8 edp_aux_write(u8 addr_hi, u16 addr, u8 len, const u8 *buf);
u8 edp_link_train(void);         /* config + CR + EQ + finish; EDP_LT_* */
u8 edp_read_link_status(void);   /* refresh edp_stat.lane[] */
u8 edp_hpd(void);                /* debounced panel HPD (9C70 bit2) */
void edp_video_on(void);         /* MSA + M/N + TU, main-link pattern = video (after display_freerun_init) */
void edp_mvid_update(void);   /* re-measure MVID + resend MSA: after ANY DCLK change */

#endif
