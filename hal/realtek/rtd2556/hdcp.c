/* HDCP 1.4 key download - see hdcp.h. */
#include "hdcp.h"
#include "flash_assets.h"
#include "rtd2556_regs.h"

#define HDCP_KSV_FLASH  0x01306FUL      /* 5 bytes (public: the receiver's ID) */
#define HDCP_KEYS_FLASH 0x013074UL      /* 320 bytes: 40 x 56-bit device keys, as stored */
#define HDCP_KEYS_LEN   320
/* flash_burst_to_port's (page, port) arguments for a data-port register */
#define PAGE_PORT(reg) HI(reg), LO(reg)

u8 hdcp_ready;

/* HDCP index port: reg = (reg & keep) | orv; hdcp_idx_wrf takes the field mask instead, as SR_WRF does */
static void hdcp_idx_rmw(u8 idx, u8 keep, u8 orv)
{
    SR(HDCP_AP) = idx;
    SR(HDCP_DP) = (SR(HDCP_DP) & keep) | orv;
}
#define hdcp_idx_wrf(idx, mask, v) hdcp_idx_rmw(idx, (u8)~(mask), v)

/* TMDS_ABC0's write-1-to-clear flags */
#define ABC0_W1C (TMDS_ABC0_WR_AKSV_FLAG | TMDS_ABC0_RD_RI_FLAG | TMDS_ABC0_RD_BKSV_FLAG | TMDS_ABC0_HIT_I2C_SLAVE_FLAG)

void hdcp_init(void) __banked
{
    u8 i, v;

    /* open both key ports */
    SR_SET(D1_PB0_DP_HDCP_CONTROL, D1_PB0_DP_HDCP_CONTROL_HDCP_EN | D1_PB0_DP_HDCP_CONTROL_KEY_DL_EN);
    SR_CLR(D1_PB0_HDCP_IRQ, D1_PB0_HDCP_IRQ_KM_CLK_SEL);
    hdcp_idx_wrf(HDCP_BCAPS, HDCP_BCAPS_HDCP_REPEATER, 0);
    SR_WRF(TMDS_RPS, TMDS_RPS_HDCP_KEY_RDY | TMDS_RPS_AUTHST_MODE_SEL, TMDS_RPS_AUTHST_MODE_SEL);
    SR_CLR(TMDS_ABC0, TMDS_ABC0_HDCP_KEY_ACCESS_MODE);
    SR_SET(HDCP_PCR, HDCP_PCR_DKAP_CLK_GATING | HDCP_PCR_HDCP_CLK_GATING);
    SR_SET(HDCP_CR, HDCP_CR_AC_MOD | HDCP_CR_DKAPDE | HDCP_CR_ENABLE);
    SR_CLR(TMDS_Z0CC2, TMDS_Z0CC2_KM_CLK_SEL);

    /* the key block, straight from flash into both key ports */
    flash_burst_to_port(HDCP_KEYS_FLASH, HDCP_KEYS_LEN, PAGE_PORT(D1_PB0_DP_HDCP_DOWNLOAD_PORT));   /* DP */
    flash_burst_to_port(HDCP_KEYS_FLASH, HDCP_KEYS_LEN, PAGE_PORT(HDCP_DKAP));                       /* HDMI */

    /* the KSV: HDMI Bksv at HDCP index 00-04, DP Bksv at DPCD 0x68000-4 */
    SR(HDCP_AP) = HDCP_BKSV0;
    flash_burst_to_port(HDCP_KSV_FLASH, 5, PAGE_PORT(HDCP_DP));        /* index auto-increments */
    /* DP: the DPCD access port does not take a burst cleanly (an
     * auto-incremented burst loses bytes), so copy the public KSV
     * from HDMI's freshly loaded index registers.  Only the KSV - the private
     * key block never touches the CPU. */
    SR(D1_DPCD_CTRL) &= (u8)~D1_DPCD_CTRL_DPCD_ACCESS_PORT_AUTO_INC;
    for (i = 0; i < 5; i++) {
        SR(HDCP_AP) = HDCP_BKSV0 + i;
        v = SR(HDCP_DP);
        SR(D1_DPCD_ADDR_PORT_H) = BYTE2(DPCD_HDCP_BKSV_BYTE_0);
        SR(D1_DPCD_ADDR_PORT_M) = HI(DPCD_HDCP_BKSV_BYTE_0);
        SR(D1_DPCD_ADDR_PORT_L) = LO(DPCD_HDCP_BKSV_BYTE_0 + i);
        SR(D1_DPCD_DATA_PORT) = v;
        SR_SET(D1_HDCP_INTGT_VRF, D1_HDCP_INTGT_VRF_INTGT_VRF_EN);
    }

    /* close the ports */
    SR_CLR(D1_PB0_DP_HDCP_CONTROL, D1_PB0_DP_HDCP_CONTROL_KEY_DL_EN);
    SR_CLR(HDCP_CR, HDCP_CR_DKAPDE);
    SR_SET(HDCP_CR, HDCP_CR_AC_MOD);
    SR_WRF(TMDS_ABC0, ABC0_W1C, TMDS_ABC0_WR_AKSV_FLAG);   /* clear the Aksv flag only */
    SR_SET(TMDS_ABC1, TMDS_ABC1_IRQ_AKSV_EN);              /* HDMI Aksv irq: the INT0 handler sets KEY_RDY */

    /* DP side: CP_IRQ by hardware when R0' is ready, link-integrity check on, HDCP 1.4 (not 2.2) */
    SR_WRF(D1_PB0_HDCP_IRQ, D1_PB0_HDCP_IRQ_CP_IRQ_AUTO_SET | D1_PB0_HDCP_IRQ_R0_MANUAL_MODE, D1_PB0_HDCP_IRQ_CP_IRQ_AUTO_SET);
    SR_SET(D1_HDCP_INTGT_VRF, D1_HDCP_INTGT_VRF_INTGT_VRF_EN);
    SR_CLR(D1_PB0_HDCP_IRQ, D1_PB0_HDCP_IRQ_HDCP22_EN);
    hdcp_ready = 1;
}
