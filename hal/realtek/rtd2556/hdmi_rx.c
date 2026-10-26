/* RTD2556 HDMI RX (port D3). */
#include "hdmi_rx.h"
#include "pipeline.h"
#include "delay.h"
#include "edp_tx.h"
#include "display.h"
#include "rtd2556_regs.h"

/* The capture path's input port: 0 = HDMI (D3), 1 = DisplayPort over USB-C
 * (D1, dp_rx.c).  Everything else in the input path is shared. */
u8 rx_port_dp;

struct hdmi_status hdmi_stat;

#define REF_KHZ 27000UL                    /* detector reference: M2PLL / 13 (GDI_CLK_DIVIDER = 1D) */

/* The GDI PHY's per-lane DFE registers: one block per TMDS data lane, L0_* (7CA0),
 * L1_* (7CB0), L2_* (7CC0) - the same layout, 0x10 apart. */
#define LANE_STEP (L1_DFE_EN_1 - L0_DFE_EN_1)
#define LANE_2 (L2_DFE_EN_1 - L0_DFE_EN_1)

/* indirect register spaces on page 72: reg = (reg & keep) | orv.  The *_wrf
 * forms take the field mask instead, as SR_WRF does. */

static void hidx_rmw(u8 idx, u8 keep, u8 orv)            /* HDMI_AP / HDMI_DP */
{
    SR(HDMI_AP) = idx;
    SR(HDMI_DP) = (SR(HDMI_DP) & keep) | orv;
}
static void hidx_w(u8 idx, u8 v) { SR(HDMI_AP) = idx; SR(HDMI_DP) = v; }
static u8 hidx_r(u8 idx) { SR(HDMI_AP) = idx; return SR(HDMI_DP); }
static void hdcpidx_rmw(u8 idx, u8 keep, u8 orv)          /* HDCP_AP / HDCP_DP */
{
    SR(HDCP_AP) = idx;
    SR(HDCP_DP) = (SR(HDCP_DP) & keep) | orv;
}
static void scdc_rmw(u8 idx, u8 keep, u8 orv)             /* SCDC: HDMI_CTRL_39 / HDMI_CTRL_3A */
{
    SR(HDMI_CTRL_39) = idx;
    SR(HDMI_CTRL_3A) = (SR(HDMI_CTRL_3A) & keep) | orv;
}
#define hidx_wrf(idx, mask, v) hidx_rmw(idx, (u8)~(mask), v)
#define hdcpidx_wrf(idx, mask, v) hdcpidx_rmw(idx, (u8)~(mask), v)
#define scdc_wrf(idx, mask, v) scdc_rmw(idx, (u8)~(mask), v)

/* Field groups this file writes together */
#define ABC0_W1C (TMDS_ABC0_WR_AKSV_FLAG | TMDS_ABC0_RD_RI_FLAG | TMDS_ABC0_RD_BKSV_FLAG | TMDS_ABC0_HIT_I2C_SLAVE_FLAG)
#define PWDCTL_PORTS (TMDS_PWDCTL_ECC | TMDS_PWDCTL_ERIP | TMDS_PWDCTL_EGIP | TMDS_PWDCTL_EBIP)   /* clock + R/G/B inputs */
#define OUTCTL_VIDEO (TMDS_OUTCTL_TRCOE | TMDS_OUTCTL_TGCOE | TMDS_OUTCTL_TBCOE | TMDS_OUTCTL_OCKE)
#define TMDS_CTRL_W1C (TMDS_CTRL_BCD | TMDS_CTRL_GCD | TMDS_CTRL_RCD | TMDS_CTRL_HO | TMDS_CTRL_YO)
#define TMDS_LOCKED (TMDS_CTRL_BCD | TMDS_CTRL_GCD | TMDS_CTRL_RCD | TMDS_CTRL_YO)
#define TMDS_RESET_LANES (TMDS_RESET_REG_RST_N_L0 | TMDS_RESET_REG_RST_N_L1 | TMDS_RESET_REG_RST_N_L2 \
                          | TMDS_RESET_REG_CDR_RST_N_L0 | TMDS_RESET_REG_CDR_RST_N_L1 | TMDS_RESET_REG_CDR_RST_N_L2)
#define ERR_DET_ALL (HDMI_CTRL_0B_ERR_DET_EN_CH0 | HDMI_CTRL_0B_ERR_DET_EN_CH1 | HDMI_CTRL_0B_ERR_DET_EN_CH2 \
                     | HDMI_CTRL_0B_VALID_CH0 | HDMI_CTRL_0B_VALID_CH1 | HDMI_CTRL_0B_VALID_CH2)
#define LOCK_TRA_ALL (HDMI_CTRL_17_LOCK_TRA_CH0 | HDMI_CTRL_17_LOCK_TRA_CH1 | HDMI_CTRL_17_LOCK_TRA_CH2)
#define LOCK_IE_ALL (HDMI_CTRL_18_LOCK_IE_CH0 | HDMI_CTRL_18_LOCK_IE_CH1 | HDMI_CTRL_18_LOCK_IE_CH2)
#define SCR_PIS (HDMI_SCR_PIS2 | HDMI_SCR_PIS3)

/* The two programmable packet slots: HDMI_PTRSV0/1 hold the packet types, and
 * HDMI_GPVS's PIS field flags each one's arrival (W1C). */
#define AVI_TYPE 0x82                       /* AVI InfoFrame */
#define GCP_TYPE 0x03                       /* General Control Packet */
#define GPVS_RSV0 FV(HDMI_GPVS_PIS, 1)      /* PTRSV0's type (AVI) received */
#define GPVS_RSV1 FV(HDMI_GPVS_PIS, 2)      /* PTRSV1's type (GCP) received */

/* GDI PHY power-on ------------------------------------------------------------ */
void hdmi_rx_phy_on(void) __banked
{
    /* shared GDI PHY setup (page-7C part) */
    SR_CLR(CMU_02, CMU_02_REG_EN_VTEST_HIZ);
    SR_WRF(KOFFSET_02, KOFFSET_02_REG_OFFCAL_RANGE, FV(KOFFSET_02_REG_OFFCAL_RANGE, 3));
    SR_CLR(WD, WD_REG_PLL_WD_EN);
    SR_WRF(ANA_CDR_02, ANA_CDR_02_REG_ADP_TIME, FV(ANA_CDR_02_REG_ADP_TIME, 2));
    SR_CLR(KOFFSET_04, KOFFSET_04_REG_HYBRID);
    SR_SET(BANDGAP_03, BANDGAP_03_REG_EN_ENHANCE_BIAS | BANDGAP_03_REG_PSM);
    SR_SET(AUX_PS_REPLY, AUX_PS_REPLY_FREQDET_CLK_GATING);     /* frequency detector clock ungate (required) */
    delay_ms(8);
    /* power on, HDMI mode */
    SR_WRF(BANDGAP_03, BANDGAP_03_REG_RXVCOM_SEL, FV(BANDGAP_03_REG_RXVCOM_SEL, 1));
    SR_WRF(KOFFSET_00, KOFFSET_00_REG_EN_BACK_OFFSET | KOFFSET_00_REG_EN_FORE_OFFSET | KOFFSET_00_REG_FORE_OFF_AUTOK,
           KOFFSET_00_REG_EN_BACK_OFFSET);
    SR_SET(BANDGAP_03, BANDGAP_03_REG_Z0_MANUAL);
    SR_WRF(IMPEDANCE_00, IMPEDANCE_00_REG_Z0_ADJR, 9);
    SR_WRF(IMPEDANCE_01, IMPEDANCE_01_REG_Z0_CM_ADJR, 6);
    SR_SET(BANDGAP_03, BANDGAP_03_REG_P_CHANNEL_EN | BANDGAP_03_REG_N_CHANNEL_EN);
    SR_WRF(DIG_00, DIG_00_REG_PHY_SI_RESET_N, 7);            /* digital reset released, 3 lanes */
    SR_SET(SDM_03, SDM_03_REG_BYPASS_CLK_RDY);
    SR_SET(EQEN_TIMER, EQEN_TIMER_REG_BYPASS_EQEN_RDY);
    SR_SET(DATA_TIMER, DATA_TIMER_REG_BYPASS_DATA_RDY);
    SR(DIG_01) = 0x24;                                          /* lane mux: MAC lane n from physical lane n */
    SR_CLR(DIG_03, DIG_03_REG_PHY_PN_SWAP);
    SR_SET(DIG_05, DIG_05_REG_PHY_SI_HD_DP_SEL);                /* HDMI (not DP) mode */
    /* terminations on */
    SR_CLR(BANDGAP_03, BANDGAP_03_REG_MHL_MODE);
    SR_CLR(CMU_01, CMU_01_REG_EN_MHL_OP);
    SR_WRF(TMDS_ABC0, ABC0_W1C, TMDS_ABC0_RD_RI_FLAG | TMDS_ABC0_RD_BKSV_FLAG);   /* clear these two flags only */
    SR_WRF(IMPEDANCE_00, IMPEDANCE_00_REG_Z0_EN, 0xF0);
    /* detector / decoder reset */
    SR_CLR(HDMI_FREQDET_CTRL, HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE);
    SR_CLR(HDMI_ERROR_TH, HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN);
    SR_CLR(TMDS_PWDCTL, TMDS_PWDCTL_ICCAF | PWDCTL_PORTS);
    SR_CLR(HDMI_CTRL_0B, ERR_DET_ALL);                          /* character error detection off */
    SR_SET(HDMI_CTRL_0C, HDMI_CTRL_0C_ERR_POPUP);
    SR_SET(HDMI_CTRL_17, LOCK_TRA_ALL);                         /* W1C */
    SR_CLR(HDMI_CTRL_18, LOCK_IE_ALL);
    SR_SET(HDMI_CTRL_1A, HDMI_CTRL_1A_XTAL_SEL);
    SR_WRF(HDMI_CTRL_1B, HDMI_CTRL_1B_RD_REQ_SET, 0x11);
}

/* HDMI system block init, then arm the frequency detector */
void hdmi_rx_sys_init(void) __banked
{
    u16 v;

    SR_CLR(TMDS_OUTCTL, TMDS_OUTCTL_AOE | OUTCTL_VIDEO);        /* video output off */
    SR_CLR(TMDS_ACC2, TMDS_ACC2_REG_HS_WIDTH_SEL | TMDS_ACC2_HDCP_VS_SEL);
    SR_CLR(TMDS_WDC, TMDS_WDC_VS_OUT_INV | TMDS_WDC_HS_OUT_INV);
    SR_CLR(HDMI_APC, HDMI_APC_AAIF);
    /* packet / deep-color HW off */
    hidx_wrf(HDMI_VWDSR, FV(HDMI_VWDSR_VWDPVSB, 1), 0); hidx_w(HDMI_PVGCR0, 0x00);
    hidx_wrf(HDMI_PVGCR1, HDMI_PVGCR1_PVSEF8, 0); hidx_wrf(HDMI_WDCR0, HDMI_WDCR0_ASMFE, 0);
    SR_WRF(TMDS_DPC1, TMDS_DPC1_DPC_CLK_SOURCE | TMDS_DPC1_DPC_AUTO, TMDS_DPC1_DPC_AUTO);
    hidx_wrf(HDMI_BCHCR, HDMI_BCHCR_BCH_FLAG_CLR_DVI | HDMI_BCHCR_BCHE | HDMI_BCHCR_BCHES | HDMI_BCHCR_BCHES2,
             HDMI_BCHCR_BCH_FLAG_CLR_DVI | HDMI_BCHCR_BCHE | HDMI_BCHCR_BCHES | HDMI_BCHCR_BCHES2);
    hidx_wrf(HDMI_SCR, SCR_PIS | HDMI_SCR_DVI_ACLK_MODE, HDMI_SCR_DVI_ACLK_MODE);
    hidx_wrf(HDMI_WDCR1, HDMI_WDCR1_VWDMOD, HDMI_WDCR1_VWDMOD);
    hidx_w(HDMI_PTRSV0, AVI_TYPE); hidx_w(HDMI_PTRSV1, GCP_TYPE); hidx_w(HDMI_PVGCR0, 0x00); hidx_w(HDMI_PVGCR1, 0x00);
    hidx_wrf(HDMI_PVGCR2, HDMI_PVGCR2_PVSEF22 | HDMI_PVGCR2_PVSEF21 | HDMI_PVGCR2_PVSEF20 | HDMI_PVGCR2_PVSEF19, 0);
    hidx_wrf(HDMI_PVSR_4, HDMI_PVSR_4_CHG_GMT_CGSN | HDMI_PVSR_4_CHG_GMT_PSC | HDMI_PVSR_4_CHG_GMT_NOCMT, 0);
    hidx_w(HDMI_PVGCR3, 0x00);
    hidx_wrf(HDMI_PVGCR4, HDMI_PVGCR4_CHG_GMT_CGSN_EN | HDMI_PVGCR4_CHG_GMT_PSC_EN | HDMI_PVGCR4_CHG_GMT_NOCMT_EN, 0);
    if (!(SR(HDMI_FREQDET_CTRL) & HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE))
        SR_CLR(HDMI_CTRL_19, HDMI_CTRL_19_SCDC_I2CS_EN);
    scdc_wrf(SCDC_21, SCDC_21_SCRAM_STATUS, 0);
    SR_WRF(HDMI_CTRL_01, HDMI_CTRL_01_SCRAM_AUTODET_EN | HDMI_CTRL_01_SCRAM_DET | HDMI_CTRL_01_SSCP_NUM,
           HDMI_CTRL_01_SCRAM_AUTODET_EN | FV(HDMI_CTRL_01_SSCP_NUM, 7));
    SR_WRF(HDMI_CTRL_38, HDMI_CTRL_38_AUTO_SET_SCRAM_ST | HDMI_CTRL_38_SCDC_AUTO_INC, HDMI_CTRL_38_AUTO_SET_SCRAM_ST);
    SR_CLR(HDMI_CTRL_1C, HDMI_CTRL_1C_RR_IGNORE_EN); SR_CLR(HDMI_CTRL_1A, HDMI_CTRL_1A_XTAL_SEL);
    SR_WRF(HDMI_CTRL_1B, HDMI_CTRL_1B_RD_REQ_SET, 0x09);
    SR_SET(HDMI_CTRL_00, HDMI_CTRL_00_HDMI_20_EN); SR_SET(HDMI_CTRL_19, HDMI_CTRL_19_SCDC_I2CS_EN);
    SR_CLR(HDCP_PCR, HDCP_PCR_DDCSEL);
    SR_SET(TMDS_Z0CC2, TMDS_Z0CC2_DDCDBNC | TMDS_Z0CC2_HDE | TMDS_Z0CC2_DBNC_LEVEL);
    hdcpidx_wrf(HDCP_SLAVE_ADD, HDCP_SLAVE_ADD_SLAVE_ADDR, 0x74 >> 1);    /* HDCP's DDC address */
    SR(HDCP_AP) = HDCP_BCAPS; SR(HDCP_DP) = HDCP_BCAPS_HDCP_HDMI_RESERVED | HDCP_BCAPS_HDCP_FAST_REAUTHEN;
    SR_SET(HDCP_CR, HDCP_CR_AC_MOD);
    SR_CLR(TMDS_CPS, TMDS_CPS_GLB_POWDN);
    SR_WRF(TMDS_PWDCTL, TMDS_PWDCTL_ICCAF | PWDCTL_PORTS, PWDCTL_PORTS);
    SR_SET(TMDS_PWDCTL, TMDS_PWDCTL_BRCW | TMDS_PWDCTL_PNSW);
    SR_SET(TMDS_CPS, TMDS_CPS_AUTO_DVI2HDMI);
    hidx_wrf(HDMI_SCR, SCR_PIS | HDMI_SCR_REG_PACKET_IGNORE | HDMI_SCR_CABS, HDMI_SCR_CABS);
    SR_WRF(TMDS_UDC2, TMDS_UDC2_NL, FV(TMDS_UDC2_NL, 3));
    SR_CLR(MHL_CTRL_00, MHL_CTRL_00_PN_SWAP | MHL_CTRL_00_MHL_EN); SR_CLR(MHL_CTRL_12, MHL_CTRL_12_GB_REF_EN);   /* MHL MAC off */
    /* arm the detector (interrupt path not used: the main loop polls freq_stable) */
    SR_SET(CMU_00, FV(CMU_00_REG_RX_EN, 8));                     /* lane 3: the clock lane receiver */
    SR_CLR(HDMI_FREQDET_CTRL, HDMI_FREQDET_CTRL_FREQDET_START | HDMI_FREQDET_CTRL_FREQ_SOURCE);
    SR_WRF(HDMI_FREQDET_BOUND_H, HDMI_FREQDET_BOUND_H_FREQDET_COUNT, 1);
    SR_SET(HDMI_FREQDET_OFFSET, HDMI_FREQDET_OFFSET_MCLK_DIV);
    SR_WRF(HDMI_FREQDET_OFFSET, HDMI_FREQDET_OFFSET_FREQDET_OFFSET, 2);
    v = 624000UL / 27;  SR(HDMI_FREQDET_UPPER_M) = HI(v); SR(HDMI_FREQDET_UPPER_L) = LO(v);   /* 624 MHz */
    v = 24000UL / 27;   SR(HDMI_FREQDET_LOWER_M) = HI(v); SR(HDMI_FREQDET_LOWER_L) = LO(v);   /* 24 MHz */
    SR(HDMI_FREQDET_STABLE) = FV(HDMI_FREQDET_STABLE_STABLE_TIME, 0x3F);
    SR_CLR(HDMI_FREQDET_CTRL, HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE);
    SR_SET(HDMI_ERROR_TH, HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN | HDMI_ERROR_TH_FREQ_STABLE);
    hidx_wrf(HDMI_INTCR, HDMI_INTCR_PENDING, HDMI_INTCR_PENDING);
    SR_SET(IRQ_CTRL0, IRQ_CTRL0_IRQ_EN);
    SR_SET(HDMI_FREQDET_CTRL, HDMI_FREQDET_CTRL_FREQDET_START);
}

/* Frequency detection (polled, not interrupt-driven) */
u8 hdmi_rx_detect(void) __banked
{
    u8 i;
    u16 count;

    for (i = 0; i < 150; i++) {
        if (SR(HDMI_ERROR_TH) & HDMI_ERROR_TH_FREQ_STABLE)
            break;
        delay_ms(10);
    }
    hdmi_stat.detect_polls = i;
    if (i == 150)
        return 0;
    SR_WRF(HDMI_ERROR_TH, HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN | HDMI_ERROR_TH_FREQ_STABLE, HDMI_ERROR_TH_FREQ_STABLE);  /* W1C, IRQ off */
    scdc_wrf(SCDC_40, SCDC_40_CLK_DET, SCDC_40_CLK_DET);
    SR_CLR(HDMI_CTRL_01, HDMI_CTRL_01_HDMI_FMT_RST | HDMI_CTRL_01_SCRAM_DET);
    SR(HDMI_CTRL_39) = SCDC_10;
    if (!(SR(HDMI_CTRL_3A) & SCDC_10_ST_UPDATE))
        SR_SET(HDMI_CTRL_3A, SCDC_10_ST_UPDATE);
    SR_SET(HDMI_FREQDET_CTRL, HDMI_FREQDET_CTRL_FREQ_UNSTABLE | HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE);
    SR_SET(HDMI_FREQDET_STABLE, HDMI_FREQDET_STABLE_RESULT_POPUP);
    count = ((u16)(SR(HDMI_FREQDET_RESULT_M) & 0x7F) << 8) | SR(HDMI_FREQDET_RESULT_L);
    hdmi_stat.tmds_count = count;
    hdmi_stat.f_tmds = (u16)(((u32)count * REF_KHZ + 50000UL) / 100000UL);
    hidx_wrf(HDMI_NTX1024TR0, HDMI_NTX1024TR0_RMCLK_DIV, hdmi_stat.f_tmds >= 3001 ? HDMI_NTX1024TR0_RMCLK_DIV : 0);
    /* +-1/64 tracking window */
    SR(HDMI_FREQDET_UPPER_M) = HI(count + (count >> 6)) & 0x7F; SR(HDMI_FREQDET_UPPER_L) = LO(count + (count >> 6));
    SR(HDMI_FREQDET_LOWER_M) = HI(count - (count >> 6));        SR(HDMI_FREQDET_LOWER_L) = LO(count - (count >> 6));
    SR_SET(HDMI_CTRL_17, LOCK_TRA_ALL);                         /* W1C */
    /* character error count lock / unlock threshold: the TMDS clock in MHz */
    SR(HDMI_CTRL_14) = (SR(HDMI_CTRL_14) & (u8)~HDMI_CTRL_14_ERR_CNT_BOUND) | HI(hdmi_stat.f_tmds / 10);
    SR(HDMI_CTRL_15) = LO(hdmi_stat.f_tmds / 10);
    SR_SET(HDMI_CTRL_0B, ERR_DET_ALL);
    SR_SET(HDMI_CTRL_18, LOCK_IE_ALL);
    hidx_wrf(HDMI_INTCR, HDMI_INTCR_PENDING, HDMI_INTCR_PENDING);
    return 1;
}

/* DFE */
/* L0_LIMIT_INIT: LE minimum low bits 8 (bit 4 is L0_INIT_1's), LE initial value v */
#define LE_LIMIT_INIT(v) (FV(L0_LIMIT_INIT_LE_MIN_LANE0, 8) | FV(L0_LIMIT_INIT_LE_INIT_LANE0, v))
#define DFE_ADAPT_LE 0x80                   /* L0_DFE_EN_2 bits: [7] LE, [6] Vth, [5] servo, [4:0] tap4-0 */
#define DFE_ADAPT_VTH 0x40
#define DFE_ADAPT_TAP1 0x02
#define DFE_ADAPT_TAP0 0x01

static void dfe_config(u16 f)
{
    u8 l, le, tap1;

    for (l = 0; l <= LANE_2; l += LANE_STEP)
        SR_WRF(L0_DFE_EN_1 + l,
               L0_DFE_EN_1_TIMER_CTRL_EN_LANE0 | L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0
               | L0_DFE_EN_1_RL_DET_EN_LANE0 | L0_DFE_EN_1_RL_DET_MODE_LANE0,
               L0_DFE_EN_1_TIMER_CTRL_EN_LANE0 | FV(L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0, 2));
    SR(MODE_TIMER) = (SR(MODE_TIMER) & (u8)~MODE_TIMER_ADAPT_MODE)
                     | (f < 1001 ? FV(MODE_TIMER_ADAPT_MODE, 2) : FV(MODE_TIMER_ADAPT_MODE, 3));
    SR_WRF(MODE_TIMER, MODE_TIMER_EDGE_DET_MODE | MODE_TIMER_TRANSITION_ONLY | MODE_TIMER_LE_AUTO_RELOAD | MODE_TIMER_TAP1_DELAY,
           MODE_TIMER_EDGE_DET_MODE | MODE_TIMER_TRANSITION_ONLY | FV(MODE_TIMER_TAP1_DELAY, 2));
    SR(TIMER) = FV(TIMER_SERVO_DELAY, 1);
    SR_WRF(GAIN_1, GAIN_1_TAP0_GAIN | GAIN_1_TAP1_GAIN, FV(GAIN_1_TAP0_GAIN, 2) | FV(GAIN_1_TAP1_GAIN, 1));
    SR_WRF(GAIN_2, GAIN_2_TAP2_GAIN | GAIN_2_TAP3_GAIN, FV(GAIN_2_TAP2_GAIN, 1) | FV(GAIN_2_TAP3_GAIN, 1));
    SR_WRF(GAIN_3, GAIN_3_TAP4_GAIN | GAIN_3_SERVO_GAIN, FV(GAIN_3_TAP4_GAIN, 1) | FV(GAIN_3_SERVO_GAIN, 2));
    SR_WRF(GAIN_4, GAIN_4_LE_GAIN1 | GAIN_4_LE_GAIN2, FV(GAIN_4_LE_GAIN1, 2) | FV(GAIN_4_LE_GAIN2, 6));
    SR_WRF(GAIN_PREC, GAIN_PREC_LE_PREC_SIGN | GAIN_PREC_LE_GAIN_PREC, FV(GAIN_PREC_LE_GAIN_PREC, 6));
    SR(LIMIT_1) = FV(LIMIT_1_VTH_THRESHOLD, 1) | FV(LIMIT_1_TAP0_MAX, 21);
    SR(LIMIT_2) = (SR(LIMIT_2) & (u8)~LIMIT_2_TAP1_MIN) | (f < 1001 ? FV(LIMIT_2_TAP1_MIN, 31) : FV(LIMIT_2_TAP1_MIN, 55));
    SR_WRF(LIMIT_3, LIMIT_3_TAP2_MAX, FV(LIMIT_3_TAP2_MAX, 10)); SR_WRF(LIMIT_3, LIMIT_3_TAP2_MIN, FV(LIMIT_3_TAP2_MIN, 1));
    SR_WRF(LIMIT_4, LIMIT_4_TAP2_MIN, FV(LIMIT_4_TAP2_MIN, 2)); SR_CLR(LIMIT_4, LIMIT_4_TAP3_MAX);
    SR_WRF(LIMIT_5, LIMIT_5_TAP3_MIN, FV(LIMIT_5_TAP3_MIN, 3)); SR_CLR(LIMIT_5, LIMIT_5_TAP4_MAX);
    SR_CLR(LIMIT_6, LIMIT_6_TAP4_MAX); SR_WRF(LIMIT_6, LIMIT_6_TAP4_MIN, FV(LIMIT_6_TAP4_MIN, 3));
    SR_WRF(RESET_1, RESET_1_TAP0_THRESHOLD, FV(RESET_1_TAP0_THRESHOLD, 10));
    SR(LIMIT_10) = FV(LIMIT_10_VTH_MAX, 6);
    SR(LOOP_DIV_1) = FV(LOOP_DIV_1_SERVO_DIVISOR, 40) | FV(LOOP_DIV_1_VTH_DIVISOR, 3);
    SR(LOOP_DIV_2) = FV(LOOP_DIV_2_VTH_DIVISOR, 3) | FV(LOOP_DIV_2_TAP_DIVISOR, 10);
    SR_SET(GRAY_DEC_2, GRAY_DEC_2_SERVO_NOTRANS | GRAY_DEC_2_TAP0_NOTRANS);
    SR_CLR(GAIN_2, GAIN_2_TAP1_TRANS | GAIN_2_TAP0_TRANS); SR_SET(GAIN_2, GAIN_2_TAP1_TRANS);
    le = f < 376 ? LE_LIMIT_INIT(4) : f < 701 ? LE_LIMIT_INIT(6) : f < 1001 ? LE_LIMIT_INIT(10) : LE_LIMIT_INIT(12);
    tap1 = f < 1001 ? 0 : 12;
    for (l = 0; l <= LANE_2; l += LANE_STEP) {
        SR_CLR(L0_GDI_DE + l, L0_GDI_DE_DE_INVERSE_LANE0 | L0_GDI_DE_DE_BRANK_EN_LANE0);   /* off until lock */
        SR(L0_LIMIT_INIT + l) = le; SR(L0_INIT_3 + l) = tap1;
        SR(L0_INIT_1 + l) = L0_INIT_1_LE_MIN_LANE0 | FV(L0_INIT_1_SERVO_INIT_LANE0, 16); SR(L0_INIT_2 + l) = 15;
        SR(L0_INIT_4 + l) = 0; SR(L0_INIT_5 + l) = 0; SR(L0_INIT_6 + l) = 0; SR(L0_INIT_7 + l) = 4;
        SR(L0_INIT_8 + l) = 0xFF; SR(L0_INIT_8 + l) = 0x00;    /* load every initial value */
    }
    SR_PULSE_LO(RESET_1, RESET_1_DFE_ADAPT_RSTB_LANE0 | RESET_1_DFE_ADAPT_RSTB_LANE1
                         | RESET_1_DFE_ADAPT_RSTB_LANE2 | RESET_1_DFE_ADAPT_RSTB_LANE3);
    for (l = 0; l <= LANE_2; l += LANE_STEP)
        SR(L0_DFE_EN_2 + l) = 0;                                 /* adaptation off */
}

/* PHY configuration for the measured band, APLL/CMU path (f < 200.1 MHz) */
void hdmi_rx_band_config(void) __banked
{
    u16 f = hdmi_stat.f_tmds;
    u8 l, a;

    SR_CLR(ANA_CDR_00, ANA_CDR_00_REG_EN_ANA_CDR);
    SR_SET(CMU_01, CMU_01_REG_EN_APLL_LANE2);
    SR_CLR(CMU_01, CMU_01_REG_CMU_REFCK_SEL);
    SR_CLR(CMU_01, CMU_01_REG_CMU_EN);
    SR_CLR(CMU_00, CMU_00_REG_CDR_EN);
    SR_SET(CMU_00, CMU_00_REG_RX_EN);                            /* all lanes + clock */
    SR_WRF(CMU_00, CMU_00_REG_CDR_EN, FV(CMU_00_REG_CDR_EN, 1));  /* CDR on: lane 0, 0-1, 0-2 */
    SR_WRF(CMU_00, CMU_00_REG_CDR_EN, FV(CMU_00_REG_CDR_EN, 3));
    SR_WRF(CMU_00, CMU_00_REG_CDR_EN, FV(CMU_00_REG_CDR_EN, 7));
    SR_WRF(ANA_CDR_02, ANA_CDR_02_REG_CDR_KVCO, FV(ANA_CDR_02_REG_CDR_KVCO, 1));
    SR_SET(ANA_CDR_03, ANA_CDR_03_REG_SEL_CONT3);
    SR_CLR(KOFFSET_04, KOFFSET_04_REG_EN_FLD_REF_ECBUSS);
    SR_WRF(CMU_04, CMU_04_REG_CMU_SEL_CP | CMU_04_REG_EN_LARGERS_FORCMU | CMU_04_REG_CMU_SEL_RS,
           CMU_04_REG_CMU_SEL_CP_2P | CMU_04_REG_EN_LARGERS_FORCMU | CMU_04_REG_CMU_SEL_RS_16K);
    SR_WRF(ANA_CDR_07, ANA_CDR_07_REG_CDR_CP_IN, FV(ANA_CDR_07_REG_CDR_CP_IN, 12));
    SR_CLR(KD, KD_REG_KD_L0 | KD_REG_KD_L1 | KD_REG_KD_L2);
    SR_CLR(KI_L0, KI_L0_REG_KI_L0); SR_CLR(KI_L1, KI_L1_REG_KI_L1); SR_CLR(KI_L2, KI_L2_REG_KI_L2);
    a = f < 1500 ? 0x20 : 0x0F;
    SR(KP_L0) = a; SR(KP_L1) = a; SR(KP_L2) = a;
    /* CDR/CMU divider band table.  ANA_CDR_03 holds bit 8 of the two FLD counts
     * (ANA_CDR_04 / 05): REF 0x0xx, CDR 0x1xx in every band. */
    SR_WRF(ANA_CDR_03, ANA_CDR_03_REG_REF_DIV_CONT | ANA_CDR_03_REG_CDR_DIV_CONT, ANA_CDR_03_REG_CDR_DIV_CONT);
    if (f < 376) {
        SR(ANA_CDR_04) = 0x1D; SR(ANA_CDR_05) = 0xDF; SR_CLR(CMU_05, CMU_05_REG_CMU_SELPREDIV); SR(CMU_06) = 0x4C;
        SR_WRF(CMU_07, CMU_07_REG_SEL_DIV, CMU_07_REG_SEL_DIV_187_5M_375M);
        SR_WRF(EN_01, EN_01_REG_RATE_SEL, EN_01_REG_RATE_SEL_37_5M_18_75M);
    } else if (f < 751) {
        SR(ANA_CDR_04) = 0x31; SR(ANA_CDR_05) = 0x8F; SR_CLR(CMU_05, CMU_05_REG_CMU_SELPREDIV); SR(CMU_06) = 0x24;
        SR_WRF(CMU_07, CMU_07_REG_SEL_DIV, CMU_07_REG_SEL_DIV_375_750M);
        SR_WRF(EN_01, EN_01_REG_RATE_SEL, EN_01_REG_RATE_SEL_75M_37_5M);
    } else if (f < 1501) {
        SR(ANA_CDR_04) = 0x7C; SR(ANA_CDR_05) = 0xF3; SR_CLR(CMU_05, CMU_05_REG_CMU_SELPREDIV); SR(CMU_06) = 0x10;
        SR_WRF(CMU_07, CMU_07_REG_SEL_DIV, CMU_07_REG_SEL_DIV_750M_1_5G);
        SR_WRF(EN_01, EN_01_REG_RATE_SEL, EN_01_REG_RATE_SEL_150M_75M);
    } else {
        SR(ANA_CDR_04) = 0xF9; SR(ANA_CDR_05) = 0xF3; SR_WRF(CMU_05, CMU_05_REG_CMU_SELPREDIV, CMU_05_REG_CMU_SELPREDIV_2); SR(CMU_06) = 0x10;
        SR_WRF(CMU_07, CMU_07_REG_SEL_DIV, CMU_07_REG_SEL_DIV_1_5G_3G);
        SR_WRF(EN_01, EN_01_REG_RATE_SEL, EN_01_REG_RATE_SEL_300M_150M);
    }
    SR_CLR(SDM_03, SDM_03_REG_EN_CLKOUT_MANUAL);
    SR_CLR(EQEN_TIMER, EQEN_TIMER_REG_EN_EQEN_MANUAL);
    SR_CLR(DATA_TIMER, DATA_TIMER_REG_EN_DATA_MANUAL);
    SR_WRF(ANA_CDR, ANA_CDR_REG_ACDR_L0_EN | ANA_CDR_REG_EN_DATA_MANUAL_L0 | ANA_CDR_REG_BYPASS_DATA_RDY_L0
                    | ANA_CDR_REG_EN_EQEN_MANUAL_L0 | ANA_CDR_REG_BYPASS_EQEN_RDY_L0,
           ANA_CDR_REG_BYPASS_DATA_RDY_L0 | ANA_CDR_REG_BYPASS_EQEN_RDY_L0);
    SR_CLR(TMDS_RESET, TMDS_RESET_LANES);
    SR_WRF(TMDS_RESET, TMDS_RESET_LANES, TMDS_RESET_REG_RST_N_L0 | TMDS_RESET_REG_RST_N_L1 | TMDS_RESET_REG_RST_N_L2);
    dfe_config(f);
    SR_SET(CMU_07, CMU_07_REG_TAP_EN);                           /* all taps */
    SR_WRF(CMU_08, CMU_08_REG_LE_LOWGAIN_HIGHBOOST, FV(CMU_08_REG_LE_LOWGAIN_HIGHBOOST, 7));
    SR_SET(CMU_08, CMU_08_REG_SEL_DFE_DELAY);
    SR_WRF(CMU_09, CMU_09_REG_LE_RLSEL | CMU_09_REG_LE_ISEL, FV(CMU_09_REG_LE_RLSEL, 3));
    SR_CLR(CMU_08, CMU_08_REG_RESERVED);
    SR_CLR(CMU_03, CMU_03_REG_EN_SUMAMP_HIGHGAIN);
    /* foreground offset calibration, auto mode: enable, then restart it */
    SR_SET(KOFFSET_00, KOFFSET_00_REG_FORE_OFF_AUTOK); SR_SET(KOFFSET_00, KOFFSET_00_REG_EN_FORE_OFFSET);
    SR_PULSE_LO(KOFFSET_00, KOFFSET_00_REG_EN_FORE_OFFSET);
    delay_ms(8);
    SR_WRF(ANA_CDR_01, ANA_CDR_01_REG_AUTO_MODE | ANA_CDR_01_REG_ADP_EN_MANUAL | ANA_CDR_01_REG_CP_EN_MANUAL
                       | ANA_CDR_01_REG_ADP_EQ_OFF | ANA_CDR_01_REG_XTAL_EN | ANA_CDR_01_REG_CP_ADJ_EN,
           ANA_CDR_01_REG_AUTO_MODE | ANA_CDR_01_REG_CP_ADJ_EN);
    SR_SET(ANA_CDR_03, ANA_CDR_03_REG_EN_WIDE_TEMP);
    SR_WRF(ANA_CDR_02, ANA_CDR_02_REG_VCM_TUNE, FV(ANA_CDR_02_REG_VCM_TUNE, 3));
    /* APLL/CMU path */
    SR_PULSE_LO(ANA_CDR_00, ANA_CDR_00_REG_RSTB_FSM);
    delay_ms(30);
    SR_SET(CMU_01, CMU_01_REG_CMU_EN);
    SR(TMDS_RESET) = 0xFF;                                       /* every lane and CDR out of reset */
    delay_ms(10);
    SR_SET(SDM_03, SDM_03_REG_EN_CLKOUT_MANUAL);
    SR_SET(EQEN_TIMER, EQEN_TIMER_REG_EN_EQEN_MANUAL);
    a = f < 376 ? 0 : f < 1001 ? DFE_ADAPT_VTH | DFE_ADAPT_TAP1 | DFE_ADAPT_TAP0
                               : DFE_ADAPT_LE | DFE_ADAPT_VTH | DFE_ADAPT_TAP1 | DFE_ADAPT_TAP0;
    for (l = 0; l <= LANE_2; l += LANE_STEP)
        SR(L0_DFE_EN_2 + l) = a;
    delay_ms(23);
    for (l = 0; l <= LANE_2; l += LANE_STEP)
        SR_CLR(L0_DFE_EN_2 + l, DFE_ADAPT_VTH);
    /* common tail */
    SR_SET(ANA_CDR, ANA_CDR_REG_EN_DATA_MANUAL_L0); SR_SET(DATA_TIMER, DATA_TIMER_REG_EN_DATA_MANUAL);
    SR_CLR(TMDS_PWDCTL, TMDS_PWDCTL_ICCAF | PWDCTL_PORTS);
    SR_SET(TMDS_PWDCTL, PWDCTL_PORTS);
    SR_SET(TMDS_CHANNEL_FIFO, TMDS_CHANNEL_FIFO_CHANNEL_FIFO_RST);
    SR_WRF(TMDS_CHANNEL_FIFO, TMDS_CHANNEL_FIFO_CHANNEL_FIFO_EN | TMDS_CHANNEL_FIFO_CHANNEL_FIFO_RST,
           TMDS_CHANNEL_FIFO_CHANNEL_FIFO_EN);
    SR_WRF(TMDS_CTRL, TMDS_CTRL_W1C, TMDS_CTRL_BCD | TMDS_CTRL_GCD | TMDS_CTRL_RCD);   /* W1C the lock flags */
}

/* Lock check and video output enable */
u8 hdmi_rx_lock(void) __banked
{
    u8 i, l;

    for (i = 0; i < 90; i++) {
        if ((SR(TMDS_CTRL) & TMDS_LOCKED) == TMDS_LOCKED)
            break;
        delay_ms(10);
    }
    hdmi_stat.lock = SR(TMDS_CTRL);
    if (i == 90)
        return 0;
    SR_SET(TMDS_CTRL, TMDS_CTRL_W1C);
    SR_PULSE_HI(TMDS_CPS, TMDS_CPS_CLR_INFOFRAME_DVI);
    SR_SET(HDMI_GPVS, GPVS_RSV1); SR_SET(HDMI_GPVS, GPVS_RSV0);   /* W1C */
    SR_CLR(HDMI_CTRL_01, HDMI_CTRL_01_HDMI_FMT_RST | HDMI_CTRL_01_SCRAM_DET);
    for (i = 0; i < 90; i++) {
        if (SR(HDMI_SR) & HDMI_SR_MODE)
            break;
        delay_ms(10);
    }
    hdmi_stat.mode_hdmi = SR(HDMI_SR) & HDMI_SR_MODE;
    if (hdmi_stat.mode_hdmi) {
        SR_CLR(TMDS_WDC, TMDS_WDC_DVI_ENC_MODE);
        if (!(SR(HDMI_CTRL_00) & HDMI_CTRL_00_HDMI_20_EN)) {
            hidx_wrf(HDMI_SCR, SCR_PIS | HDMI_SCR_MODE | HDMI_SCR_MSMODE, HDMI_SCR_MODE | HDMI_SCR_MSMODE);   /* manual: HDMI */
            SR_WRF(HDMI_CTRL_01, HDMI_CTRL_01_HDMI_FMT_RST | HDMI_CTRL_01_SCRAM_DET, HDMI_CTRL_01_HDMI_FMT_RST);
            SR_CLR(HDMI_CTRL_01, HDMI_CTRL_01_HDMI_FMT_RST | HDMI_CTRL_01_SCRAM_DET);
        }
        if (!(SR(HDMI_CTRL_01) & HDMI_CTRL_01_SCRAM_DET))
            scdc_wrf(SCDC_21, SCDC_21_SCRAM_STATUS, SCDC_21_SCRAM_STATUS);
    } else {
        SR_SET(TMDS_WDC, TMDS_WDC_DVI_ENC_MODE);
        if (!(SR(HDMI_CTRL_00) & HDMI_CTRL_00_HDMI_20_EN))
            hidx_wrf(HDMI_SCR, SCR_PIS | HDMI_SCR_MODE | HDMI_SCR_MSMODE, 0);   /* automatic */
    }
    SR_SET(TMDS_PWDCTL, TMDS_PWDCTL_DEO);
    for (l = 0; l <= LANE_2; l += LANE_STEP)
        SR_SET(L0_GDI_DE + l, L0_GDI_DE_DE_INVERSE_LANE0 | L0_GDI_DE_DE_BRANK_EN_LANE0);
    hidx_wrf(HDMI_VWDSR, FV(HDMI_VWDSR_VWDPVSB, 1), 0); hidx_w(HDMI_PVGCR0, 0);
    hidx_wrf(HDMI_PVGCR1, HDMI_PVGCR1_PVSEF8, 0); hidx_wrf(HDMI_WDCR0, HDMI_WDCR0_ASMFE, 0);
    hidx_wrf(HDMI_ACRCR, HDMI_ACRCR_CSAM | HDMI_ACRCR_CSC | HDMI_ACRCR_PRDSAM, 0); hidx_wrf(HDMI_VCR, HDMI_VCR_DSC, 0);
    delay_ms(30);
    SR_CLR(TMDS_UDC0, TMDS_UDC0_DPC_BYPASS_DIS);
    hidx_wrf(HDMI_AVMCR, HDMI_AVMCR_VE, HDMI_AVMCR_VE); hidx_wrf(HDMI_DPC_SET0, HDMI_DPC_SET0_DPC_EN, HDMI_DPC_SET0_DPC_EN);
    SR_SET(TMDS_OUTCTL, OUTCTL_VIDEO);
    return 1;
}

/* The TMDS clock has left the +-1/64 window hdmi_rx_detect() set around the
 * locked count (or stopped): the source is changing mode or going away.  The
 * flag is W1C and hdmi_rx_detect() clears it on every lock. */
u8 hdmi_clock_moved(void) __banked
{
    if (SR(TMDS_UDC1) & TMDS_UDC1_NO_CLK_IN)
        return 1;
    return (SR(HDMI_FREQDET_CTRL) & HDMI_FREQDET_CTRL_FREQ_UNSTABLE) ? 1 : 0;
}

/* Start watching from now: clear the sticky "unstable" flag (W1C) - a source
 * still wobbling while the mode set ran must not tear down the picture it
 * has just been given. */
void hdmi_clock_watch(void) __banked
{
    SR(HDMI_FREQDET_CTRL) |= HDMI_FREQDET_CTRL_FREQ_UNSTABLE;
}

/* Cheap poll for "a TMDS clock is present" (no reconfigure): the RX must have been
 * armed once by hdmi_rx_phy_on + hdmi_rx_sys_init.  TMDS_UDC1 no_clk_in = 0 means a
 * clock is arriving.  Used by the main loop to decide when to bring the input up. */
u8 hdmi_clock_present(void) __banked
{
    return (SR(TMDS_UDC1) & TMDS_UDC1_NO_CLK_IN) ? 0 : 1;
}

/* Refresh comes from the off-line measure block, which counts the real input
 * periods: HS_PERIOD_D is the line length in pixel clocks and VS_PERIOD_A the
 * frame length in lines, both stored as (value - 1).  Then
 *     Vrefresh [0.1 Hz] = pixel_clock [0.1 MHz] * 1e6 / (Htotal * Vtotal)
 * Example: 1024x768@60 is 1344 x 806 x 60 = 65.0 MHz.
 *
 * The high bytes are narrow fields (HS_PERIOD_D [4:0], VS_PERIOD_A [3:0]) and
 * must be masked - the neighboring bits are not ours.
 *
 * Range check: a refresh above ~90 Hz is rejected, and so is one below
 * 20 Hz, because a half-measured frame reads as an implausibly slow one
 * rather than as an error flag. */
#define HDMI_REFRESH_MIN 200    /* 20.0 Hz */
#define HDMI_REFRESH_MAX 907    /* 90.7 Hz */

/* Which mode the input is, from its total timing.
 *
 * Page 30 counts periods and pulse widths only; the active region comes from
 * the boundary search (hdmi_boundary_search), and this table is its fallback -
 * its hstart / vsta columns are exactly what the search measures.
 *
 * htotal and vtotal are measured reliably, and together they identify a video
 * mode uniquely.  Every mode a PC or console will send is a CEA-861 or DMT
 * timing with fixed totals, so a small table keyed on (htotal, vtotal) gives
 * the active size exactly.
 *
 * Totals, not refresh: 1080p60 and 1080p30 share 2200x1125 and the same active
 * area, so keying on the totals alone is both sufficient and stable. */
/* hstart and vsta are where the input's active video begins for this mode:
 * hstart is the capture window's H start (source pixels), vsta the number of
 * lines from input VSYNC to the first active line.
 *
 * Neither is written straight into a register.  vsta feeds the frame-sync
 * calculation (frame_sync_set, below), which splits one IVS->DVS delay between
 * the capture window and the display's read position - that is where the IVS
 * delay (001C) actually comes from.  1080p is hstart 8, vsta 43, measured by
 * the boundary search.
 *
 * The 720p rows are tuned by eye against the panel edges (hstart 97, vsta 23;
 * the frame-sync calculation gives IVS delay 19 from vsta 23, matching).
 * Theory (sync + back porch + 2) is off by ~21 and ~4 there, so every row
 * other than 720p and 1080p is theory only. */
#define TABLE_IHS_DELAY 262     /* 001D = 0x06 with 001E bit 0 = IHS delay bit 8 */

struct hdmi_mode {
    u16 htotal, vtotal, hactive, vactive;
    u16 hstart;
    u8 vsta;
};

/*  htot  vtot  hact  vact  hsta vsta                            sync+bp (H, V) */
static const __code struct hdmi_mode hdmi_modes[] = {
    {  800,  525,  640,  480,    0,  37 },  /* 640x480p60   DMT      144,  35 */
    {  858,  525,  720,  480,    0,  38 },  /* 720x480p60   CEA 2/3  122,  36 */
    {  864,  625,  720,  576,    0,  46 },  /* 720x576p50   CEA17/18 132,  44 */
    { 1056,  628,  800,  600,   32,  29 },  /* 800x600p60   DMT      216,  27 */
    { 1344,  806, 1024,  768,  112,  37 },  /* 1024x768p60  DMT      296,  35 */
    { 1650,  750, 1280,  720,   97,  23 },  /* 1280x720p60  CEA 4    260,  25  MEASURED */
    { 1980,  750, 1280,  720,   97,  23 },  /* 1280x720p50  CEA 19   260,  25  (as p60) */
    { 1680,  831, 1280,  800,  144,  30 },  /* 1280x800p60  CVT      328,  28 */
    { 1688, 1066, 1280, 1024,  176,  43 },  /* 1280x1024p60 DMT      360,  41 */
    { 1904,  934, 1440,  900,  200,  33 },  /* 1440x900p60  CVT      384,  31 */
    { 1760,  926, 1600,  900,    0,  25 },  /* 1600x900p60  CVT-RB   112,  23  BROKEN: blocky + jittery; not in our EDID */
    { 1840, 1080, 1680, 1050,    0,  29 },  /* 1680x1050p60 CVT-RB   112,  27 */
    { 2200, 1125, 1920, 1080,    8,  43 },  /* 1920x1080p60 CEA 16   192,  41  MEASURED */
    { 2640, 1125, 1920, 1080,    8,  43 },  /* 1920x1080p50 CEA 31   192,  41 */
    { 2750, 1125, 1920, 1080,    8,  43 },  /* 1920x1080p24 CEA 32   192,  41 */
};
#define HDMI_MODE_COUNT (sizeof(hdmi_modes) / sizeof(hdmi_modes[0]))

/* An unrecognized timing is set up as 1080p60 (hdmi_input_mode). */
static const __code struct hdmi_timing timing_1080p = { 2200, 1125, 600, 1, 1920, 1080, 8, TABLE_IHS_DELAY, 43 };

static void measure_active(struct hdmi_timing *t, u16 htot, u16 vtot)
{
    u8 i;

    t->active_ok = 0;
    t->hactive = 0;
    t->vactive = 0;
    for (i = 0; i < HDMI_MODE_COUNT; i++) {
        if (hdmi_modes[i].htotal != htot || hdmi_modes[i].vtotal != vtot)
            continue;
        t->hactive = hdmi_modes[i].hactive;
        t->vactive = hdmi_modes[i].vactive;
        t->hstart = hdmi_modes[i].hstart;
        t->ihs = TABLE_IHS_DELAY;       /* the table rows were tuned against a fixed 262 */
        t->vsta = hdmi_modes[i].vsta;
        t->active_ok = 1;
        return;
    }
}

u8 hdmi_measure(struct hdmi_timing *t) __banked
{
    u32 num, den;
    u16 htot, vtot, hz;

    if (SR(P30_MEAS_ERROR_FLG0) & (P30_MEAS_ERROR_FLG0_VS_TIME_OUT
                                 | P30_MEAS_ERROR_FLG0_MS_TIMEOUT_HALF
                                 | P30_MEAS_ERROR_FLG0_V_OVER_FLOW_A
                                 | P30_MEAS_ERROR_FLG0_H_OVER_FLOW_A))
        return 0;
    SR(P30_MEAS_CTL1) |= P30_MEAS_CTL1_MEAS_POP_UP;     /* latch the counters */
    htot = (u16)(((u16)(SR(P30_MEAS_HS_PERIOD_D_M) & P30_MEAS_HS_PERIOD_D_M_HS_PERIOD_D) << 8)
                 | SR(P30_MEAS_HS_PERIOD_D_L));
    vtot = (u16)(((u16)(SR(P30_MEAS_VS_PERIOD_A_M) & P30_MEAS_VS_PERIOD_A_M_VS_PERIOD_A) << 8)
                 | SR(P30_MEAS_VS_PERIOD_A_L));
    if (htot == 0 || vtot == 0 || hdmi_stat.f_tmds == 0)
        return 0;
    htot++;
    vtot++;
    den = (u32)htot * vtot;
    num = (u32)hdmi_stat.f_tmds * 1000000UL;            /* f_tmds <= 2000, so no overflow */
    hz = (u16)(num / den);
    if (hz < HDMI_REFRESH_MIN || hz > HDMI_REFRESH_MAX)
        return 0;
    t->htotal = htot;
    t->vtotal = vtot;
    t->refresh_x10 = hz;
    measure_active(t, htot, vtot);
    return 1;
}

/* Digital boundary search: the first/last active pixel and line, off DE.
 *
 *  - 001E is NOT touched: its bit 0 can read back clear after a cold boot, so
 *    it must never be read-modify-written; input_geometry_set() writes it
 *    outright, auto-function delay bits 5:4 = 0.
 *  - 0077 is written outright with the window bits, for the same reason,
 *    rather than read-modify-written.
 *  - Input sampling (M1_VGIP_CTRL IVRUN_EN) is switched ON first.  The search
 *    measures DE on the sampled input, and hdmi_measure_start() clears
 *    IVRUN_EN, so in SETTLING - before the mode set turns it back on - the
 *    search would read all zeros.  Harmless: the display is still showing the
 *    background color, so nothing sampled is visible.
 * NOW_AF is NOT polled: with AUTOPHASE_EN set it does not self-clear.  The
 * search waits a fixed time and reads; validity is judged from the values.
 * 007B / 007D end up F8 / C0. */
#define BOUND_SETTLE_MS 90      /* ~5 frames at 60 Hz */

u8 hdmi_boundary_search(struct hdmi_bound *b) __banked
{
    u8 hx;

    SR_SET(M1_VGIP_CTRL, M1_VGIP_CTRL_IVRUN_EN);          /* the search needs the input sampled */
    SR_CLR(AUTO_ADJ_CTRL1, AUTO_ADJ_CTRL1_NOW_AF);        /* stop anything in progress */
    SR(AUTO_ADJ_CTRL0) = 0x00;                            /* digital: no field select, color 0 */
    SR(HW_AUTO_PHASE_CTRL0) = FV(HW_AUTO_PHASE_CTRL0_AUTOSOD_STEP_NUM, 31);
    /* H window 0x0002 .. 0x1FFF, V window 0x000 .. 0xFFF: the whole frame */
    SR(H_BOUNDARY_H) = FV(H_BOUNDARY_H_HB_END, 0xF);
    SR(H_BOUNDARY_STA_L) = 0x02;
    SR(H_BOUNDARY_END_L) = 0xFF;
    SR(GRN_NOISE_MARGIN) = GRN_NOISE_MARGIN_HB_END;       /* noise margin 0 */
    SR(V_BOUNDARY_H) = FV(V_BOUNDARY_H_VB_END, 0xF);
    SR(V_BOUNDARY_STA_L) = 0x00;
    SR(V_BOUNDARY_END_L) = 0xFF;
    /* go: measure off DE (M_VGEN_EN) */
    SR_WRF(AUTO_ADJ_CTRL1, AUTO_ADJ_CTRL1_M_VGEN_EN | AUTO_ADJ_CTRL1_AUTOPHASE_EN | AUTO_ADJ_CTRL1_DIFF_EN | AUTO_ADJ_CTRL1_NOW_AF,
           AUTO_ADJ_CTRL1_M_VGEN_EN | AUTO_ADJ_CTRL1_AUTOPHASE_EN | AUTO_ADJ_CTRL1_NOW_AF);
    delay_ms(BOUND_SETTLE_MS);

    hx = SR(BLU_NOISE_MARGIN);
    b->hs = (u16)(((u16)(SR(H_START_END_H) >> H_START_END_H_HX_STA_SHIFT) << 8) | SR(H_START_L));
    b->he = (u16)(((u16)(SR(H_START_END_H) & H_START_END_H_HX_END) << 8) | SR(H_END_L));
    if (hx & BLU_NOISE_MARGIN_HX_STA) b->hs |= 0x1000;
    if (hx & BLU_NOISE_MARGIN_HX_END) b->he |= 0x1000;
    b->vs = (u16)(((u16)(SR(VER_START_END_H) >> VER_START_END_H_VX_STA_SHIFT) << 8) | SR(VER_START_L));
    b->ve = (u16)(((u16)(SR(VER_START_END_H) & VER_START_END_H_VX_END) << 8) | SR(VER_END_L));

    return (b->he > b->hs && b->ve > b->vs) ? 1 : 0;
}

/* Measured boundary -> capture geometry.
 *
 * H: the capture register counts from the HSYNC after the IHS delay, which is
 * 001D:001E bit 0 = 0x106 = 262 pixels (both as the table sets them), so
 * capture hstart = HX_STA - 262.  1080p: 270 - 262 = 8, the table's value.
 * 720p: 360 - 262 = 98, against 97 tuned by eye - within the eye's +-1.
 *
 * V: vsta is VX_STA itself.  1080p: 43, the table's value.  720p measures 28
 * against the 23 that looked right by eye; the measurement comes straight off
 * DE, so it is trusted over the eye.
 *
 * At 1080p this reproduces the table exactly. */
/* The split: the capture START never moves; the offset goes in the IHS/IVS
 * delays.  The split matters, not just the total: part of the I-domain pipeline counts
 * from the line start the IHS delay creates, not from the capture start, so
 * putting the offset in the capture start leaves the picture late with its
 * left edge cut off.
 *
 * IHS = HX_STA - 8 gives 1080p exactly 262 (measured 270). */
#define CAPTURE_HSTART 8

void hdmi_timing_from_bound(struct hdmi_timing *t, const struct hdmi_bound *b) __banked
{
    t->hactive = (u16)(b->he - b->hs + 1);
    t->vactive = (u16)(b->ve - b->vs + 1);
    t->hstart = CAPTURE_HSTART;
    t->ihs = (b->hs > CAPTURE_HSTART) ? (u16)(b->hs - CAPTURE_HSTART) : 0;
    t->vsta = (u8)b->vs;
    t->active_ok = 1;
}

/* Live TMDS frequency-detector count (as hdmi_rx_detect reads it), without
 * disturbing hdmi_stat. */
u16 hdmi_freq_count(void) __banked
{
    SR_SET(HDMI_FREQDET_STABLE, HDMI_FREQDET_STABLE_RESULT_POPUP);
    return (u16)(((u16)(SR(HDMI_FREQDET_RESULT_M) & 0x7F) << 8) | SR(HDMI_FREQDET_RESULT_L));
}

u8 hdmi_rx_start(void) __banked
{
    hdmi_rx_phy_on();
    hdmi_rx_sys_init();
    if (!hdmi_rx_detect())
        return hdmi_stat.result = HDMI_ERR_NO_CLOCK;
    if (hdmi_stat.f_tmds >= 2001)
        return hdmi_stat.result = HDMI_ERR_BAND;
    hdmi_rx_band_config();
    if (!hdmi_rx_lock())
        return hdmi_stat.result = HDMI_ERR_NO_LOCK;
    return hdmi_stat.result = HDMI_OK;
}

/* Input-path registers an HDMI/DP input needs that no per-mode code writes: the
 * ones whose cold-boot value gives a different end state after the mode set. */
static const __code struct { u16 reg; u8 val; } input_path_init[] = {
    { IRQ_CTRL0, IRQ_CTRL0_IRQ_EN },
    { POWER_CTRL, 0x00 },                   /* cold 0x20 */
    { M1_IPV_ACT_STA_L, 0x04 },             /* capture V start; the V position is in the IVS delay */
    /* disp_sram_busy is sticky: writing 1 clears it;
     * input_geometry_set() sets H/V per mode */
    { SCALE_CONTROL_REG, SCALE_CONTROL_REG_DISP_SRAM_BUSY | SCALE_CONTROL_REG_MAIN_ENABLE_H },
    { FS_DELAY_FINE_TUNE, FS_DELAY_FINE_TUNE_IV_DV_LINE_SKIP },
    { LAST_LINE_H, LAST_LINE_H_TOTAL_LINE_SEL | FV(LAST_LINE_H_SEL_MEAS_VALUE, 4) },   /* frame_sync_set() */
    { LAST_LINE_L, 0x64 },                                                            /* RMWs these */
    { GAMMA_BIST_DITHER_SETA, FV(GAMMA_BIST_DITHER_SETA_SRGB_PRECI, 2) },             /* cold 0x60 */
    { D_DITHER_REGISTER_ADDR, 0x05 },       /* cold 0x01 */
    { I_DITHER_COMMON_CTRL1_SETA, I_DITHER_COMMON_CTRL1_SETA_DITHER_ENABLE | I_DITHER_COMMON_CTRL1_SETA_DITHER_TEMPORAL_EN },
    { RGB2YCC_CTRL, RGB2YCC_CTRL_Y_OUT_SHIFT | FV(RGB2YCC_CTRL_CB_CR_OUT_SHIFT, 2) },
    { ADC_POWER, ADC_POWER_REG_ADC_MBIAS_POW },
    { ADC_SOG_CTRL, ADC_SOG_CTRL_REG_ADC_SOG_RES_500K | ADC_SOG_CTRL_REG_ADC_SOG_IB_SOURCE_SEL },   /* cold 0x40 */
    /* the line counts' upper bits: per mode */
    { IN1_LINE_NUM_H, IN1_LINE_NUM_H_IN1_BK_STA_SEL | FV(IN1_LINE_NUM_H_IN1_LNUM_H, 4) },
    { MN_PRRD_VST_L, 0x00 },                /* cold 0x02 */
    { MN_LINE_NUM_H, FV(MN_LINE_NUM_H_MN_LNUM_H, 4) },
    { MN_SDR_STATUS, MN_SDR_STATUS_MN_IN_FMT_30 },
    /* D-domain YCbCr -> RGB (page 14): coefficients and offsets */
    { SR_YCC2RGB_CTRL, SR_YCC2RGB_CTRL_CB_CR_CLAMP | SR_YCC2RGB_CTRL_Y_CLAMP },
    { SR_YCC2RGB_COEF_K11_MSB, 0xE4 }, { SR_YCC2RGB_COEF_K11_LSB, 0xAB },
    { SR_YCC2RGB_COEF_K13_MSB, 0x27 }, { SR_YCC2RGB_COEF_K13_LSB, 0x31 },
    { SR_YCC2RGB_COEF_K22_LSB, 0xDB },
    { SR_YCC2RGB_COEF_K23_MSB, 0x42 }, { SR_YCC2RGB_COEF_K23_LSB, 0x23 },
    { SR_YCC2RGB_COEF_K32_MSB, 0x88 }, { SR_YCC2RGB_COEF_K32_LSB, 0x79 },
    { SR_YCC2RGB_R_OFFSET_LSB, 0x10 }, { SR_YCC2RGB_G_OFFSET_LSB, 0x10 }, { SR_YCC2RGB_B_OFFSET_LSB, 0x10 },
    /* page-16 measure block (the mode set itself measures with page 30's) */
    { P16_MEAS_CTL0, FV(P16_MEAS_CTL0_TIME_CLK_SEL, 1) },
    { P16_MEAS_CTL1, P16_MEAS_CTL1_MEAS_CONTI_START | P16_MEAS_CTL1_MEAS_START },
    { P16_MEAS_HS_VS_DELTA_CTL, P16_MEAS_HS_VS_DELTA_CTL_HS_DELTA_SEL_16_CLOCK | P16_MEAS_HS_VS_DELTA_CTL_VS_DELTA_SEL_4_HSYNC
                                | P16_MEAS_HS_VS_DELTA_CTL_HS_PULSE_DELTA_SEL_16_CLOCK },
    { MEAS_HS_VS_DELTA_WD, MEAS_HS_VS_DELTA_WD_HS_OVER_RANGE_WD_EN | MEAS_HS_VS_DELTA_WD_VS_OVER_RANGE_WD_EN
                           | MEAS_HS_VS_DELTA_WD_HS_PULSE_OVER_RANGE_WD_EN },
    { MEAS_HS_VS_ERROR_WD, MEAS_HS_VS_ERROR_WD_VS_OVERFLOW_A_WD_EN | MEAS_HS_VS_ERROR_WD_HS_OVERFLOW_A_WD_EN },
    { M1_MEASURE_DUMMY0, M1_MEASURE_DUMMY0_VS_TIME_OUT_WD_EN },
};

/* Indirect banks behind address/data port pairs on page 0 (data port = address + 1):
 * Zero-filled first (their cold contents are not known to be zero),
 * then these, each bank's entry 1 last - writing entry 1 is what loads a bank. */
#define SD (u8)SD_ADDRESS_PORT_M1
#define FW (u8)FIFO_WINDOW_ADDR_PORT
#define SF (u8)SF_ADDR_PORT
#define SP (u8)SYNC_PROC_PORT_ADDR
static const __code struct { u8 ap, idx, val; } input_bank_init[] = {
    { SD, SD_H_SD_FACTOR_H_M1, 0x10 },
    { SD, SD_INITIAL_R_L_M1, 0x01 }, { SD, SD_INITIAL_G_L_M1, 0x01 }, { SD, SD_INITIAL_B_L_M1, 0x01 },
    { SD, SD_OUT_WID_M_M1, 0x07 }, { SD, SD_OUT_WID_L_M1, 0x80 },             /* 1920 */
    { SD, SD_OUT_LEN_M_M1, 0x04 }, { SD, SD_OUT_LEN_L_M1, 0x38 },             /* 1080 */
    { SD, SD_V_SD_FACTOR_H_M1, 0x02 },
    { FW, FW_DISPLAY_READ_WID_BSU_L, 0x80 },                                    /* the rest: per mode */
    { SF, SF_HOR_FILTER_COEF_INI, 0xFF }, { SF, SF_VER_FILTER_COEF_INI, 0xFF },
    { SF, SF_HOR_FILTER_COEF_INI_2, 0xFF }, { SF, SF_VER_FILTER_COEF_INI_2, 0xFF },
    { SF, SF_HOR_SCALE_FACTOR_M, 0xFF },
    { SP, SP_G_CLAMP_START, 0x04 }, { SP, SP_BR_CLAMP_START, 0x04 }, { SP, SP_BR_CLAMP_END, 0x10 },
    { SP, SP_COAST_CTRL, FV(SP_COAST_CTRL_COAST_FRONT, 2) | FV(SP_COAST_CTRL_COAST_BACK, 1) }, { SP, SP_CAPTURE_WINDOW_SETTING, 0x04 },
    { SP, SP_DETECTION_TOLERANCE_SETTING, 0x04 },
    { SP, SP_G_CLAMP_END, 0x10 },
};
#undef SD
#undef FW
#undef SF
#undef SP

static void input_path_load(void)
{
    static const __code u8 bank_ap[4] = {
        (u8)SD_ADDRESS_PORT_M1, (u8)FIFO_WINDOW_ADDR_PORT, (u8)SF_ADDR_PORT, (u8)SYNC_PROC_PORT_ADDR
    };
    static const __code u8 bank_n[4] = { 64, 64, 64, 32 };
    u8 i, j;

    for (i = 0; i < sizeof(input_path_init) / sizeof(input_path_init[0]); i++)
        SR(input_path_init[i].reg) = input_path_init[i].val;
    for (i = 0; i < 4; i++)
        for (j = 0; j < bank_n[i]; j++)
            if (j != 1) {
                SR(bank_ap[i]) = j;
                SR(bank_ap[i] + 1) = 0;
            }
    for (i = 0; i < sizeof(input_bank_init) / sizeof(input_bank_init[0]); i++) {
        SR(input_bank_init[i].ap) = input_bank_init[i].idx;
        SR(input_bank_init[i].ap + 1) = input_bank_init[i].val;
    }
}

/* M1 input = digital (HDMI) and the off-line measure block running on the input
 * syncs.  Separate from hdmi_input_mode() because the signal check that GATES
 * the mode set needs the measurement running FIRST.
 *
 * Clears imd_db_en itself: these are I-domain registers, and with the double
 * buffer set the writes wait for a trigger that a dead input never sends. */
#define MEAS_SOURCE_D3 FV(P30_SYNC_SELECT_MEAS_SOURCE, 3)
#define PATH0_FROM_D1 FV(FC_INPUT_SEL_P01_PATH0_INPUT, 4)     /* path 0 source index */
#define PATH0_FROM_D3 FV(FC_INPUT_SEL_P01_PATH0_INPUT, 6)

void hdmi_measure_start(void) __banked
{
    SR_CLR(I_DB_CTRL, I_DB_CTRL_IMD_DB_EN | I_DB_CTRL_IMD_DB_READY);
    SR_CLR(TMDS_WDC, TMDS_WDC_VS_OUT_INV | TMDS_WDC_HS_OUT_INV);
    /* in_sel 3 for a digital input (the DB lists 3 as reserved, 1 as TMDS) */
    SR_WRF(M1_VGIP_CTRL, M1_VGIP_CTRL_DOUBLE_ENABLE | M1_VGIP_CTRL_IN_SEL | M1_VGIP_CTRL_DIGITAL_MODE | M1_VGIP_CTRL_IVRUN_EN,
           FV(M1_VGIP_CTRL_IN_SEL, 3));
    SR_CLR(M1_VGIP_DELAY_CTRL, M1_VGIP_DELAY_CTRL_HDLY_ONE | M1_VGIP_DELAY_CTRL_HS_SYNCEDGE | M1_VGIP_DELAY_CTRL_VS_SYNCEDGE);
    /* HS/VS inversion stays OFF for every mode, -sync modes included: the
     * position offset lives in the IHS delay instead (hdmi_timing_from_bound). */
    SR_CLR(M1_VGIP_SIGINV, M1_VGIP_SIGINV_FIELD_HS_INV | M1_VGIP_SIGINV_VS_INV);
    SR_CLR(P30_SYNC_SELECT, P30_SYNC_SELECT_CLK_PATH_DIV2);
    SR_WRF(P30_MEAS_CTL0, P30_MEAS_CTL0_TIME_CLK_SEL | P30_MEAS_CTL0_MEAS_HS_DEN_SWAP | P30_MEAS_CTL0_VSMS_INV | P30_MEAS_CTL0_OFF_MEAS_PWR,
           FV(P30_MEAS_CTL0_TIME_CLK_SEL, 3));
    SR_CLR(P30_MEAS_CTL1, P30_MEAS_CTL1_MEAS_CONTI_START | P30_MEAS_CTL1_MEAS_START);
    SR(P30_SYNC_SELECT) = (SR(P30_SYNC_SELECT) & (u8)~P30_SYNC_SELECT_MEAS_SOURCE)
                          | (rx_port_dp ? P30_SYNC_SELECT_MEAS_SOURCE_D1 : MEAS_SOURCE_D3);
    delay_ms(8);
    SR_SET(P30_MEAS_CTL1, P30_MEAS_CTL1_MEAS_CONTI_START | P30_MEAS_CTL1_MEAS_START);
    /* Route path 0 to the input port.  hdmi_input_mode() does this too, but
     * the boundary search runs BEFORE the first mode set after boot, and
     * without the routing it sees no DE and reads 0 0. */
    SR(FC_INPUT_SEL_P01) = rx_port_dp ? PATH0_FROM_D1 : PATH0_FROM_D3;
}

/* Input geometry, from the looked-up active size.
 *
 * Two separate things have to agree with the input:
 *
 *  1. the M1 CAPTURE WINDOW (0014-001B) - how much of the incoming frame to
 *     take;
 *  2. the SDR FRAME GEOMETRY on page 05 - MN_PXL_NUM (pixels per line) and
 *     MN_LINE_NUM / IN1_LINE_NUM (lines per frame), i.e. how the captured
 *     frame is laid out in memory and read back.  A mismatch starts every
 *     line at the wrong offset, which reads as smearing and color noise
 *     rather than as a wrong picture size.
 *
 * ONLY sizes are touched.  The capture window START is fixed (H 8 per mode
 * row, V 4 from input_path_init) - tiny values, a fixed pipeline offset
 * rather than an absolute back-porch position.  The high registers
 * are read-modify-written because they share space with unrelated fields
 * (MADR_TOG_MUL, FRAME_MD, BK_STA_SEL).
 *
 * At 1080p this writes 1920x1080 throughout. */
/* The panel, i.e. what the scaler must produce.  The display timing generator
 * is fixed 1080p (display.c), so this is the scaler's output size. */
#define PANEL_H PANEL_WIDTH
#define PANEL_V PANEL_HEIGHT

/* Scaling factor: how far to step through the SOURCE per OUTPUT pixel, as a
 * 20-bit fraction with 2^20 = 1.0, written high-nibble first through the
 * scaling-factor port (0033 address / 0034 data).
 *
 * The exact form is pinned by the 1:1 value: 1:1 is
 * 0F FF FF = 0xFFFFF, and ((1920 << 20) - 1) / 1920 is exactly 0xFFFFF, so the
 * "- 1" is real and not a rounding guess.  1280 -> 1920 gives 0xAAAAA = 2/3,
 * which is upscaling: two thirds of a source pixel per output pixel. */
static void sf_put20(u8 base, u32 v)
{
    SR(SF_ADDR_PORT) = base;            SR(SF_DATA_PORT) = BYTE2(v) & 0x0F;
    SR(SF_ADDR_PORT) = (u8)(base + 1);  SR(SF_DATA_PORT) = HI(v);
    SR(SF_ADDR_PORT) = (u8)(base + 2);  SR(SF_DATA_PORT) = LO(v);
}

/* FIFO window: the input active size, packed exactly like the capture window -
 * one byte of high nibbles (H then V), then the two low bytes. */
static void fifo_window_put(u8 base, u16 h, u16 v)
{
    SR(FIFO_WINDOW_ADDR_PORT) = base;
    SR(FIFO_WINDOW_DATA_PORT) = (u8)(((HI(h) & 0x0F) << 4) | (HI(v) & 0x0F));
    SR(FIFO_WINDOW_ADDR_PORT) = (u8)(base + 1);  SR(FIFO_WINDOW_DATA_PORT) = (u8)h;
    SR(FIFO_WINDOW_ADDR_PORT) = (u8)(base + 2);  SR(FIFO_WINDOW_DATA_PORT) = (u8)v;
}

/* Frame-sync delay: the IVS->DVS delay, the frame-length correction, and its
 * split between the capture IVS delay and the display read delay.  All in
 * INPUT clocks.
 *
 * The display is frame-synced: each input VSYNC restarts the display timing
 * generator after a delay X.  X is chosen so the display reads the frame about
 * 2.5 input lines behind where the capture is writing it (`lead`) - close
 * enough to keep the FIFO small, far enough that the reader never overtakes the
 * writer.  Whole lines of X are then moved into the capture's IVS delay
 * (001C), leaving the capture V start at 4, and the remainder is the display's
 * read delay (003E:0040 lines, 0041 16-clock steps, 003D single clocks).
 *
 * A read delay that does not match the mode makes the display read into the
 * FIFO: STATUS0 shows overflow and underflow every frame.
 *
 * From 1080p's (2200, 1080, vsta 43, hstart 8) this gives IVS 39, 0 lines,
 * 0x3D, 0, and from 720p's vsta 23 it gives IVS 19 - the value tuned by eye.
 *
 * Not implemented: a guard that raises the display V start when
 * (X + 2 lines)/Htot + 1 >= Vstart, which would fire for 720p.  Also: dX
 * assumes an exact 60.000 Hz source against our fixed DCLK; a 50 Hz or
 * 59.94 Hz source needs the DCLK retrimmed, which is not implemented, so those
 * modes will slip frames regardless of this. */
#define DISP_HTOT PANEL_HTOTAL  /* display line length, DTG[0x00:01] + 4 */
#define DISP_VSTART PANEL_VSTART    /* display active start after DVS: lines ... */
#define DISP_HSTART PANEL_HSTART    /* ... and clocks */
#define DISP_VTOT PANEL_VTOTAL
#define DISP_LAST_LINE 2080     /* partial last line, 0x820 dclk */
/* M1_VGIP_HV_DELAY's IHS-delay bits (bit 0 = IHS bit 8, bit 2 = IHS bit 9) for
 * the current mode, set by input_geometry_set().  frame_sync_set() adds the IVS
 * bit and writes the register outright - see the note there. */
static u8 hv_ihs_bits = M1_VGIP_HV_DELAY_IHS_DLY_;
s8 hdmi_fs_extra_lines;


/* The display frame, in display clocks, for this input, solved analytically
 * instead of measured.
 *
 * Frame sync makes a display frame last exactly one input frame.  For the
 * display to stay a fixed distance behind the capture, the display must ALSO
 * spend exactly as long on its 1080 active lines as the input spends on its
 * Vact_i - otherwise reader and writer drift apart through the frame and the
 * FIFO over/underflows.  That makes the frame T' = Htot_d * Vtot_i * Vact_d /
 * Vact_i display clocks.  1080p60 and 720p60 both have the panel's own 0.96
 * active fraction, so T' is 2230 x 1125 for both; every other mode differs,
 * by up to 23 input lines of drift per frame at 640x480.
 *
 * target rounds T' to the nearest frame whose last, partial line is exactly
 * 2080 clocks. */
static void frame_target(const struct hdmi_timing *t, u32 *tp, u32 *target) __reentrant
{
    u32 v, rem, d;

    v = (u32)DISP_HTOT * t->vtotal;
    v *= PANEL_V;
    v /= t->vactive;
    *tp = v;
    rem = v % DISP_HTOT;
    v = v - rem + DISP_LAST_LINE;
    d = (rem > DISP_LAST_LINE) ? rem - DISP_LAST_LINE : DISP_LAST_LINE - rem;
    if (d > DISP_HTOT / 2) {
        if (rem > DISP_LAST_LINE)
            v += DISP_HTOT;
        else
            v -= DISP_HTOT;
    }
    *target = v;
}

/* The display clock that makes one display frame of `target` clocks last one
 * input frame: f = target * refresh, refresh = f_tmds / (Htot_i * Vtot_i).
 * Expressed as the DDS offset relative to 0x843 = 150.516 MHz, which
 * is 2508600 clocks at 60.000 Hz.
 *
 * f_tmds comes from the frequency detector (count x 27 kHz), which reads about
 * +0.036% high at 1080p (5502 vs 5500).  Hence the snap: within 0.1% of the
 * default, the default is used, so 1080p60 and 720p60 keep the default clock
 * exactly and the error only ever applies to modes that need a real change.
 *
 * Returns 0 when the needed clock is outside the DDS range (~141-159 MHz):
 * 1080p50 needs 125.4 MHz, which takes a DPLL divider change, not a DDS trim.
 *
 * Integer only - float would not link (its helpers need internal RAM there is
 * none of).  word = 34883 * target * count * 27000 / (hv * 150516000), and
 * 27000 / 150516000 = 3 / 16724, so word = K * target / hv with
 * K = 104649 * count / 16724.  The product is ~39 bits, so target / hv is built
 * as q1 + f1/2^10 + f2/2^20 by two 10-bit long-division steps (r < hv < 2^22,
 * so r << 10 stays inside 32 bits) and K is kept in q2.  Example:
 * 1080p (count 5502) gives 0x84F, which the snap turns back into 0x843. */
#define DCLK_SNAP 35                    /* LSBs, ~0.1% */

/* dclk_for()'s inputs and working values as of the last mode set, for the
 * debug console. */
struct dclk_dbg hdmi_dclk_dbg;

/* Base clock of the display PLL at multiplier m, in kHz: 141390 at the boot
 * M of 77, ~1.79 MHz per step (display.c). */
#define DPLL_BASE_KHZ(m) ((141390UL * ((u32)(m) + 2)) / 79)
#define DPLL_M_MIN 55
#define DDS_PREFER_MIN 2000     /* when M must move, land the DDS near 1080p60's 0x843 */

static u8 dclk_for(const struct hdmi_timing *t, u32 target, u8 *m, u16 *off) __reentrant
{
    u32 hv = (u32)t->htotal * t->vtotal;
    u32 c27 = (u32)hdmi_stat.tmds_count * 27UL;          /* TMDS clock, kHz */
    u32 q1, r, f1, f2, fk, fb, o;
    u8 mm;

    /* required display clock fk = target * c27 / hv  (kHz), in pieces: the
     * product is ~39 bits, so target / hv is long-divided in 10-bit steps. */
    q1 = target / hv;
    r = target % hv;
    f1 = (r << 10) / hv;
    r = (r << 10) % hv;
    f2 = (r << 10) / hv;
    fk = c27 * q1;
    f1 *= c27;
    fk += f1 >> 10;
    f2 *= c27;
    fk += f2 >> 20;
    hdmi_dclk_dbg.count = hdmi_stat.tmds_count;
    hdmi_dclk_dbg.target = target;
    hdmi_dclk_dbg.hv = hv;
    hdmi_dclk_dbg.k2 = c27;
    hdmi_dclk_dbg.w2 = fk;

    *m = DISPLAY_DPLL_M_DEFAULT;
    *off = DISPLAY_DCLK_DEFAULT;
    /* Boot M first: every 60 Hz mode fits here, and this reproduces the
     * offsets they already run on.  Within ~0.1% of 150.516 MHz use 0x843
     * exactly (the detector reads +0.036% at 1080p). */
    fb = DPLL_BASE_KHZ(DISPLAY_DPLL_M_DEFAULT);
    if (fk >= fb) {
        o = ((fk - fb) << 15) / fb;
        if (o <= 0x0FFF) {
            if (o >= DISPLAY_DCLK_DEFAULT - DCLK_SNAP && o <= DISPLAY_DCLK_DEFAULT + DCLK_SNAP)
                o = DISPLAY_DCLK_DEFAULT;
            *off = (u16)o;
            return 1;
        }
    }
    /* Below the boot M's range (50 Hz): step M down until the DDS lands in
     * the upper half of its range, near where 1080p60 is proven to run. */
    for (mm = DISPLAY_DPLL_M_DEFAULT - 1; mm >= DPLL_M_MIN; mm--) {
        fb = DPLL_BASE_KHZ(mm);
        if (fk < fb)
            continue;
        o = ((fk - fb) << 15) / fb;
        if (o >= DDS_PREFER_MIN && o <= 0x0FFF) {
            *m = mm;
            *off = (u16)o;
            return 1;
        }
    }
    return 0;                   /* out of reach: leave the default 60 Hz clock */
}

static void frame_sync_set(const struct hdmi_timing *t) __reentrant
{
    u32 htot = t->htotal;
    u32 v, a_i, x, dxm, tp, target, lines, rem;
    s16 q, e05a, ivs;
    u8 neg;

    /* The only signed quantity (dX) is handled as a magnitude and a sign,
     * which is also how C's truncating division behaves. */

    /* A_i = A_d * Vact_i / Vact_d * Htot_i / Htot_d.  A_d is where the
     * display's ACTIVE window starts, so a pillarboxed window starts later. */
    v = (u32)(DISP_HTOT * DISP_VSTART + DISP_HSTART);
    v += hdmi_out_x;
    v *= t->vactive;
    v /= PANEL_V;
    v *= htot;
    a_i = v / DISP_HTOT;
    /* B_i + lead, lead = 2 * Htot_i + Htot_i * Vact_i / (2 * Vact_d) */
    x = (u32)t->vsta * htot;
    x += t->hstart + 2;                                 /* E050 = capture hstart + 2 */
    x += 2UL * htot;
    v = htot * t->vactive;
    v /= 2UL * PANEL_V;
    x += v;
    if (hdmi_fs_extra_lines > 0)
        x += (u32)hdmi_fs_extra_lines * htot;
    else if (hdmi_fs_extra_lines < 0)
        x -= (u32)(-hdmi_fs_extra_lines) * htot;
    x = (x > a_i) ? x - a_i : 0;

    /* move the DVS by half the frame-length change
     * (1080p60: 2508600 - 2508750 = -150). */
    frame_target(t, &tp, &target);
    neg = (target < tp) ? 1 : 0;
    dxm = neg ? tp - target : target - tp;
    dxm *= t->vactive;
    dxm /= DISP_VTOT;
    dxm *= htot;
    dxm /= DISP_HTOT;
    dxm /= 2;
    if (neg)
        x = (x > dxm) ? x - dxm : 0;
    else
        x += dxm;

    /* whole lines into the IVS delay, V start kept >= 4 */
    q = (s16)(x / htot);
    e05a = (s16)t->vsta - q;
    ivs = (e05a < 4) ? q - (4 - e05a) : q;
    if (ivs < 0)
        ivs = 0;
    v = (u32)ivs * htot;
    x = (x > v) ? x - v : 0;
    lines = x / htot;
    rem = x % htot;

    SR(M1_IVS_DELAY) = LO(ivs);
    /* WRITTEN OUTRIGHT, never read-modify-written.  001E also carries bit 8 of
     * the HORIZONTAL capture delay (bit 0; with 001D = 0x06 the delay is
     * 0x106 = 262 px), and on the first mode set after a cold boot a read of
     * 001E returns bit 0 clear.  An RMW would write it back as 0 and start the
     * capture 256 source pixels early (a black band on the left). */
    SR(M1_VGIP_HV_DELAY) = (u8)(hv_ihs_bits | ((ivs & 0x100) ? M1_VGIP_HV_DELAY_IVS_DLY : 0));
    SR(IVS2DVS_DLY_LINE_H) = (u8)((SR(IVS2DVS_DLY_LINE_H) & (u8)~IVS2DVS_DLY_LINE_H_IV_DV_LINE_INI)
                                  | (HI(lines) & IVS2DVS_DLY_LINE_H_IV_DV_LINE_INI));
    SR(IVS2DVS_DLY_LINE_L) = LO(lines);
    v = (rem >= 16) ? (rem >> 4) - 1 : 0;               /* 16-clock steps, HW adds one */
    SR(DISPLAY_ALIGN_CONTROL) = (u8)((SR(DISPLAY_ALIGN_CONTROL) & (u8)~DISPLAY_ALIGN_CONTROL_IV_DV_16ICLK1_INI)
                                     | ((v & 0x100) ? DISPLAY_ALIGN_CONTROL_IV_DV_16ICLK1_INI : 0));
    SR(IVS2DVS_DLY_TUNE_ODD) = LO(v);
    SR(IV_DV_DELAY_CLK_FINE) = (u8)((SR(IV_DV_DELAY_CLK_FINE) & (u8)~IV_DV_DELAY_CLK_FINE_IV_DV_ICLK1_INI)
                                    | ((u8)(rem & 0x0F) << IV_DV_DELAY_CLK_FINE_IV_DV_ICLK1_INI_SHIFT));
}

/* Does the measured eDP MVID agree with a requested display clock (kHz) to
 * within 2%?  MVID = f x 32768 / 270000 = f x 2048 / 16875 (fits 32 bits for
 * any clock this chip runs). */
static u8 dclk_matches(u32 fk)
{
    u32 want = (fk * 2048UL) / 16875UL;
    u32 got = ((u32)edp_stat.mvid[0] << 16) | ((u16)edp_stat.mvid[1] << 8) | edp_stat.mvid[2];
    u32 d = (got > want) ? got - want : want - got;

    return (d * 50UL <= want) ? 1 : 0;
}

/* ---- scaling mode ------------------------------------------------------------
 *
 * The output window is the DTG's ACTIVE window inside its BACKGROUND window.
 * The background stays the whole panel (48..1968 x 6..1086, display.c) and
 * the scaler writes only the active window, so the border is the background
 * color (display_bg_rgb, black).  Only the width ever changes - see the
 * constraint in hdmi_rx.h.
 *
 * Aspect is the picture's, not the pixel grid's.  The AVI InfoFrame's picture
 * aspect (M) decides when the source sends one - that is the only thing that
 * tells 720x480p 4:3 (VIC 2) from 16:9 (VIC 3), which share the timing.
 * Without it, 720x480 / 720x576 are taken as 4:3 SD and everything else as
 * square pixels (PC modes typically send "no data"). */
u16 hdmi_out_w = PANEL_H, hdmi_out_x;
u8 hdmi_ar_used;

#define AVI_LEN 13

u8 hdmi_avi_aspect(void) __banked
{
    u8 i, b, sum, pb2 = 0;

    if (!(SR(HDMI_GPVS) & GPVS_RSV0))           /* AVI received */
        return HDMI_AR_NONE;
    SR(HDMI_PSAP) = 2;
    if (SR(HDMI_PSDP) != AVI_LEN)
        return HDMI_AR_NONE;
    sum = (u8)(AVI_TYPE + AVI_LEN);
    for (i = 1; i <= 3 + AVI_LEN; i++) {        /* version, length, checksum, PB1..PB13 */
        if (i == 2)
            continue;                           /* length: already in the sum */
        SR(HDMI_PSAP) = i;
        b = SR(HDMI_PSDP);
        sum += b;
        if (i == 5)
            pb2 = b;
    }
    if (sum != 0)
        return HDMI_AR_NONE;                    /* torn or stale */
    b = (pb2 >> 4) & 0x03;                      /* M1:M0 */
    return (b == 1) ? HDMI_AR_4_3 : ((b == 2) ? HDMI_AR_16_9 : HDMI_AR_NONE);
}
static u8 scale_mode = HDMI_SCALE_FIT;

void hdmi_scaling_set(u8 mode) __banked
{
    scale_mode = (mode < HDMI_SCALE_COUNT) ? mode : HDMI_SCALE_STRETCH;
}

static void output_window(const struct hdmi_timing *t) __reentrant
{
    u32 w;
    u16 ar_w = t->hactive, ar_h = t->vactive;

    hdmi_ar_used = hdmi_avi_aspect();
    if (hdmi_ar_used == HDMI_AR_4_3) {
        ar_w = 4;
        ar_h = 3;
    } else if (hdmi_ar_used == HDMI_AR_16_9) {
        ar_w = 16;
        ar_h = 9;
    } else if (ar_w == 720 && (ar_h == 480 || ar_h == 576)) {
        ar_w = 4;
        ar_h = 3;
    }
    w = PANEL_H;
    /* Fit, and the picture is narrower than the panel: width = 1080 * aspect.
     * A picture WIDER than 16:9 would need a letterbox, which the line buffer
     * cannot do; it is stretched vertically instead (no such mode is in the
     * EDID today). */
    if (scale_mode == HDMI_SCALE_FIT && (u32)ar_w * PANEL_V < (u32)ar_h * PANEL_H) {
        w = ((u32)PANEL_V * ar_w + ar_h / 2) / ar_h;
        w &= ~1UL;                                      /* even: keeps the centering exact */
    }
    hdmi_out_w = (u16)w;
    hdmi_out_x = (u16)((PANEL_H - w) / 2);
}

/* The AVI aspect has changed since the mode set: would it move the output
 * window?  At boot the first InfoFrame often lands just after the first mode
 * set ("none" -> 16:9), which for a 16:9 timing is the same window, and a
 * needless mode set would blank a working picture.  When the window would not
 * move, the new aspect is simply adopted. */
u8 hdmi_window_stale(const struct hdmi_timing *t) __banked
{
    u16 w = hdmi_out_w, x = hdmi_out_x;

    output_window(t);
    if (hdmi_out_w == w)
        return 0;                   /* hdmi_ar_used keeps the new aspect */
    hdmi_out_w = w;                 /* leave the rest to the mode set */
    hdmi_out_x = x;
    return 1;
}

/* Write the DTG active window for hdmi_out_w/x.  The bank reloads on the
 * DH_TOTAL low byte, as display_freerun_init() loads it, so that goes last
 * with display.c's own value. */
static void dtg_active_window_set(void)
{
    u16 sta = DISP_HSTART + hdmi_out_x;
    u16 end = sta + hdmi_out_w;

    SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_ACT_STA_H; SR(DISPLAY_FORMAT_DATA_PORT) = HI(sta);
    SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_ACT_STA_L; SR(DISPLAY_FORMAT_DATA_PORT) = LO(sta);
    SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_ACT_END_H; SR(DISPLAY_FORMAT_DATA_PORT) = HI(end);
    SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_ACT_END_L; SR(DISPLAY_FORMAT_DATA_PORT) = LO(end);
    SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_TOTAL_L;   SR(DISPLAY_FORMAT_DATA_PORT) = DISPLAY_DTG_HTOTAL_L;
}

static void input_geometry_set(const struct hdmi_timing *t)
{
    output_window(t);
    dtg_active_window_set();

    /* IHS delay: where the I-domain line begins.  Written outright (001E is
     * never read back - see frame_sync_set); 1080p gets 0x06 / 0x01, the
     * table's own values. */
    hv_ihs_bits = (u8)(((t->ihs & 0x100) ? M1_VGIP_HV_DELAY_IHS_DLY_ : 0)
                       | ((t->ihs & 0x200) ? M1_VGIP_HV_DELAY_IHS_DLY : 0));
    SR(M1_IHS_DELAY) = LO(t->ihs);
    SR(M1_VGIP_HV_DELAY) = hv_ihs_bits;

    /* M1 capture window: size and start, both per mode */
    SR(M1_IPH_ACT_STA_H) = (u8)(((HI(t->hactive) & 0x0F) << M1_IPH_ACT_STA_H_IH_ACT_WID_SHIFT)
                                | (HI(t->hstart) & 0x0F));
    SR(M1_IPH_ACT_STA_L) = LO(t->hstart);
    SR(M1_IPH_ACT_WID_L) = LO(t->hactive);
    /* V start stays at 4 (input_path_init) - frame_sync_set() keeps it there
     * and puts the vertical position in the IVS delay. */
    SR(M1_IPV_ACT_STA_H) = (u8)((SR(M1_IPV_ACT_STA_H) & (u8)~M1_IPV_ACT_STA_H_IV_ACT_LEN)
                                | ((HI(t->vactive) & 0x0F) << M1_IPV_ACT_STA_H_IV_ACT_LEN_SHIFT));
    SR(M1_IPV_ACT_LEN_L) = LO(t->vactive);

    /* SDR frame geometry: pixels per line and lines per frame */
    SR(MN_PXL_NUM_H) = (u8)((SR(MN_PXL_NUM_H) & (u8)~MN_PXL_NUM_H_MN_PXL_NUM_H)
                            | (HI(t->hactive) & MN_PXL_NUM_H_MN_PXL_NUM_H));
    SR(MN_PXL_NUM_L) = LO(t->hactive);
    SR(MN_LINE_NUM_H) = (u8)((SR(MN_LINE_NUM_H) & (u8)~MN_LINE_NUM_H_MN_LNUM_H)
                             | (HI(t->vactive) & MN_LINE_NUM_H_MN_LNUM_H));
    SR(MN_LINE_NUM_L) = LO(t->vactive);
    SR(IN1_LINE_NUM_H) = (u8)((SR(IN1_LINE_NUM_H) & (u8)~IN1_LINE_NUM_H_IN1_LNUM_H)
                              | (HI(t->vactive) & IN1_LINE_NUM_H_IN1_LNUM_H));
    SR(IN1_LINE_NUM_L) = LO(t->vactive);

    /* FIFO window and scaling factors.  Both banks are written (0x00 and 0x20
     * blocks); they are evidently a pair.
     *
     * Wrong values give a picture of the right SIZE resampled at the wrong
     * rate, which looks like color noise rather than a geometry fault. */
    fifo_window_put(FW_DISPLAY_READ_WID_BSU_H, t->hactive, t->vactive);
    fifo_window_put(FW_DISPLAY_READ_WID_BSU_H_2, t->hactive, t->vactive);
    sf_put20(SF_HOR_SCALE_FACTOR_H, (((u32)t->hactive << 20) - 1UL) / hdmi_out_w);
    sf_put20(SF_VER_SCALE_FACTOR_H, (((u32)t->vactive << 20) - 1UL) / PANEL_V);
    sf_put20(SF_HOR_SCALE_FACTOR_H_2, (((u32)t->hactive << 20) - 1UL) / hdmi_out_w);
    sf_put20(SF_VER_SCALE_FACTOR_H_2, (((u32)t->vactive << 20) - 1UL) / PANEL_V);

    /* Filter enables: H always, V only when the height scales (1080p: 0x01;
     * bit 5 is cleared in input_path_init).  Written
     * outright so V is cleared again after a scaled mode: left on, 1080p comes
     * up rotated by a third of the screen. */
    SR(SCALE_CONTROL_REG) = SCALE_CONTROL_REG_MAIN_ENABLE_H
                          | (t->vactive != PANEL_V ? SCALE_CONTROL_REG_MAIN_ENABLE_V : 0);

    /* Display clock for this input's active fraction and refresh (see
     * frame_target / dclk_for).  Unchanged for 1080p60 and 720p60.  If it does
     * change, the eDP MVID has to be re-measured or the panel regenerates the
     * old pixel clock. */
    {
        u32 tp, target;
        u16 off;
        u8 m;

        u8 m_changed;

        frame_target(t, &tp, &target);
        hdmi_stat.dclk_ok = dclk_for(t, target, &m, &off);
        m_changed = (m != display_dpll_m_get());
        if (display_dclk_program(m, off))
            edp_mvid_update();
        hdmi_stat.dclk_path = m_changed ? DCLK_PATH_SOFT : DCLK_PATH_NONE;
        /* A PLL multiplier change is verified against the eDP MVID, which is
         * the real pixel clock (pixel / 270 MHz link x 32768): a live M write
         * can silently fail to take.  If the soft change missed, fall back to
         * the full boot power-up sequence. */
        if (m_changed && !dclk_matches(hdmi_dclk_dbg.w2)) {
            display_dpll_restart(m, off);
            edp_mvid_update();
            hdmi_stat.dclk_path = dclk_matches(hdmi_dclk_dbg.w2) ? DCLK_PATH_HARD : DCLK_PATH_FAILED;
        }
    }

    /* Every mode, 1080p60 included: the whole read delay (including the line
     * count 003E:0040 and 003F bit 4) must be rewritten, or it is inherited
     * from the PREVIOUS mode.  For 1080p60 003D's single-clock fine delay
     * comes out 0; a few clocks either way is far inside the ~2.5-line lead. */
    frame_sync_set(t);
}

/* M1 input capture + scaler + measure, display frame-synced */
void hdmi_input_mode(const struct hdmi_timing *t) __banked
{
    /* I-domain double buffer OFF while programming: with imd_db_en set the writes wait
     * for a trigger and never land */
    SR(I_DB_CTRL) = 0; SR(D_DB_CTRL) = 0;
    display_watchdogs_disarm();     /* never reprogram with a watchdog action latched */
    hdmi_measure_start();           /* M1 input = digital (HDMI), measure on the input syncs */
    input_path_load();              /* the input path's fixed state (input_path_init) */
    /* burst-loaded SRAM tables: scale-up/scale-down filter coefficients and input dither.
     * Must happen while imd_db_en is clear, like the rest of the input-path programming. */
    pipeline_tables_load(PIPELINE_SHARPNESS_DEFAULT, 8, 1);
    /* MN_SDR_STATUS mn_dbwr_en: self-clearing commit of the main-side (MN_*) geometry into its
     * working copy, pulsed; without it the display reads with stale geometry
     * (FIFO overflow every frame, wrap line at ~45 %). */
    /* Before the dbwr commit below, which is what latches the geometry into the
     * working copy.  An unrecognized timing gets 1080p's geometry and frame sync,
     * never the previous mode's. */
    input_geometry_set(t && t->active_ok ? t : &timing_1080p);
    if (rx_port_dp)
        SR(MN_SDR_STATUS) = 0x00;   /* 24-bit capture on DP */
    SR_SET(MN_SDR_STATUS, MN_SDR_STATUS_MN_DBWR_EN);
    SR(M1_VGIP_CTRL) |= M1_VGIP_CTRL_IVRUN_EN;
    SR(FC_INPUT_SEL_P01) = rx_port_dp ? PATH0_FROM_D1 : PATH0_FROM_D3;
    SR_CLR(SRGB_CTRL, SRGB_CTRL_SRGB_ENABLE);                    /* sRGB matrix: coefficient SRAM not loaded */
    SR(STATUS0) = 0xFF;                                      /* W1C every flag */
    /* before the double buffer goes back on, and with the background forced
     * (same write): the first frames after a lock are noise on every mode
     * change, HDMI and DP alike.  Released below, a few frames after everything
     * settled. */
    display_input_lock();
    delay_ms(60);                                            /* ~3.5 frames */
    SR(STATUS0) = 0xFF;
    /* Deliberately does NOT arm the watchdogs.  Arming belongs to the source
     * state machine (core/source.c), which is the only thing that can get back
     * out when one trips - see display_watchdogs_arm().  Arming here races the
     * frame sync; when it loses, the display wedges in a state only a power
     * cycle clears. */
    SR(I_DB_CTRL) = I_DB_CTRL_IMD_DB_EN; SR(D_DB_CTRL) = 0;
    delay_ms(75);                                            /* ~4.5 more frames */
    display_force_bg(0);
}

/* ---- HDMI audio (receiver side) ---------------------------------------------
 *
 * HDMI audio start-up, in order: the audio DPLL power-up, then the audio
 * init.  Every write goes through the HDMI
 * indirect port (HDMI_AP/HDMI_DP).
 *
 * ORDER MATTERS.  Writing the end-state values alone leaves the audio PLL
 * unlocked, and the hardware keeps clearing AVMCR's unmute bit ("if audio PLL
 * non-lock").  The PLL has to be powered down, given its M code and output
 * divider, loaded through the CMCR double buffer, VCO-reset released, powered
 * up and then calibrated in three timed steps (CMP enable, latch, validate) -
 * only then do the FIFO/tracking/preset-code writes and the automatic audio
 * output flow (AFCR bit 6, left as reset) turn the output on by themselves.
 *
 * HOP(index, field mask, value): reg = (reg & ~mask) | value; a mask of 0xFF is a
 * plain write. */
struct hidx_op { u8 idx, keep, val; };
#define HOP(idx, mask, val) { idx, (u8)~(mask), val }

#define AUD_PLL_STEP_MS 3
#define AFSM_MOD_ALL (HDMI_PRESET_AFSM_MOD_DP_ABUF_WR_MOD_EN | HDMI_PRESET_AFSM_MOD_AUTO_STOP_TRK_EN \
                      | HDMI_PRESET_AFSM_MOD_TRK_MOD_EN | HDMI_PRESET_AFSM_MOD_AFSM_MOD_EN \
                      | HDMI_PRESET_AFSM_MOD_DBUF_FREQ_CHANGE_SEL)
#define AUD_SAME_EN_ALL (HDMI_AUDIO_FREQ_RESULT_AUD_441_48_SAME_EN | HDMI_AUDIO_FREQ_RESULT_AUD_882_96_SAME_EN \
                         | HDMI_AUDIO_FREQ_RESULT_AUD_176_192_SAME_EN)

static const __code struct hidx_op aud_pll_pre[] = {
    HOP(HDMI_DPCR0, HDMI_DPCR0_REG_DPLL_PWDN, HDMI_DPCR0_REG_DPLL_PWDN),        /* DPLL power down */
    HOP(HDMI_MCAPR, 0xFF, 0x1A),                        /* M code for this crystal */
    HOP(HDMI_DPCR0, HDMI_DPCR0_REG_DPLL_O, 0),          /* output divider /1 */
    HOP(HDMI_CMCR, HDMI_CMCR_DBDCB | HDMI_CMCR_REG_DPLL_N, HDMI_CMCR_DBDCB),   /* K = 0, double-buffer download */
    HOP(HDMI_DPCR2, HDMI_DPCR2_REG_DPLL_VCOMD, FV(HDMI_DPCR2_REG_DPLL_VCOMD, 2)),
    HOP(HDMI_DPCR2, HDMI_DPCR2_REG_DPLL_WDO | HDMI_DPCR2_REG_DPLL_RESERVE, HDMI_DPCR2_REG_DPLL_RESERVE),
    HOP(HDMI_DPCR3, HDMI_DPCR3_REG_DPLL_VCORSTB, 0),    /* release the VCO reset */
    HOP(HDMI_DPCR0, HDMI_DPCR0_REG_DPLL_PWDN, 0),       /* power up */
};
static const __code u8 aud_pll_cal[] = {               /* HDMI_DPCR0, one step at a time */
    HDMI_DPCR0_REG_DPLL_CMPEN, HDMI_DPCR0_REG_DPLL_CALLCH, HDMI_DPCR0_REG_DPLL_CALSW
};

static const __code struct hidx_op aud_init[] = {
    HOP(HDMI_AFCR, HDMI_AFCR_AUDIO_TEST_ENABLE, 0),     /* internal test tone off */
    HOP(HDMI_WDCR0, HDMI_WDCR0_ASMFE | HDMI_WDCR0_AUDIO_WD_BY_VIC, HDMI_WDCR0_AUDIO_WD_BY_VIC),
    HOP(HDMI_PSCR, HDMI_PSCR_FDINT | HDMI_PSCR_ETFD | HDMI_PSCR_ETFBC,          /* FIFO depth tracking: */
        HDMI_PSCR_FDINT_N_8 | HDMI_PSCR_ETFD | HDMI_PSCR_ETFBC),               /* trend and boundary */
    HOP(HDMI_AAPNR, HDMI_AAPNR_ESDM, HDMI_AAPNR_ESDM),  /* SDM (phase swallow) on */
    HOP(HDMI_MAG_M_FINAL, HDMI_MAG_M_FINAL_AUTO_DLY_MOD, 0),
    HOP(HDMI_AUDIO_LD_P_TIME_M, HDMI_AUDIO_LD_P_TIME_M_LDP_TIME, 0),          /* LDP time 0x010 */
    HOP(HDMI_AUDIO_LD_P_TIME_N, 0xFF, 0x10),
    HOP(HDMI_ZCD_TIMEOUT, HDMI_ZCD_TIMEOUT_ZCD_TIMEOUT, 0x1F),
    HOP(HDMI_FBR, HDMI_FBR_BAD, 5),                     /* boundary distance */
    HOP(HDMI_ICTPSR0, 0xFF, 0x00), HOP(HDMI_ICTPSR1, 0xFF, 0x02), HOP(HDMI_PCTPSR0, 0xFF, 0x7F), HOP(HDMI_PCTPSR1, 0xFF, 0xFF),
    HOP(HDMI_ICBPSR0, 0xFF, 0x00), HOP(HDMI_ICBPSR1, 0xFF, 0x40), HOP(HDMI_PCBPSR0, 0xFF, 0x7F), HOP(HDMI_PCBPSR1, 0xFF, 0xFF),
    /* audio output enable + unmute (HW clears both while unlocked) */
    HOP(HDMI_AVMCR, HDMI_AVMCR_AUDIO_OUTPUT_EN | HDMI_AVMCR_AOMC, HDMI_AVMCR_AUDIO_OUTPUT_EN | HDMI_AVMCR_AOMC),
    HOP(HDMI_WDCR0, HDMI_WDCR0_AWDFO | HDMI_WDCR0_AWDFU, 0),
    HOP(HDMI_WDCR1, HDMI_WDCR1_XV, FV(HDMI_WDCR1_XV, 3)),
    HOP(HDMI_WDCR2, HDMI_WDCR2_YV, FV(HDMI_WDCR2_YV, 3)),
    HOP(HDMI_PRESET_AFSM_MOD, AFSM_MOD_ALL, AFSM_MOD_ALL),
    HOP(HDMI_AUDIO_FREQ_RESULT, AUD_SAME_EN_ALL, 0),    /* 44.1 / 48 kHz etc. told apart */
    HOP(HDMI_AUDIO_FREQ_RESULT_M, HDMI_AUDIO_FREQ_RESULT_M_AUD_FREQ_TIME, FV(HDMI_AUDIO_FREQ_RESULT_M_AUD_FREQ_TIME, 1)),
    /* frequency-detect thresholds */
    HOP(HDMI_TH1, 0xFF, 0x00), HOP(HDMI_TH2, 0xFF, 0x28), HOP(HDMI_TH3, 0xFF, 0x4C), HOP(HDMI_TH4, 0xFF, 0x5C),
    HOP(HDMI_TH5, 0xFF, 0x70),
    /* preset S codes (S1 | S/2), then D codes */
    HOP(HDMI_PRESET_S_CODE0, 0xFF, 0xB0), HOP(HDMI_PRESET_S_CODE1, 0xFF, 0xA0), HOP(HDMI_PRESET_S_CODE2, 0xFF, 0xA0),
    HOP(HDMI_PRESET_D_CODE_0, 0xFF, 0xD8), HOP(HDMI_PRESET_D_CODE_1, 0xFF, 0x47), HOP(HDMI_PRESET_D_CODE_2, 0xFF, 0x2C),
    HOP(HDMI_PRESET_D_CODE_3, 0xFF, 0xC8), HOP(HDMI_PRESET_D_CODE_4, 0xFF, 0xD8), HOP(HDMI_PRESET_D_CODE_5, 0xFF, 0x47),
    HOP(HDMI_AUDIO_FREQDET, HDMI_AUDIO_FREQDET_AUTO_LOAD_SCODE, HDMI_AUDIO_FREQDET_AUTO_LOAD_SCODE),   /* auto-load S/D codes */
    HOP(HDMI_SCAPR, 0xFF, HDMI_SCAPR_SLC | FV(HDMI_SCAPR_SC, 1)),
    HOP(HDMI_CMCR, HDMI_CMCR_DBDCB, HDMI_CMCR_DBDCB),   /* double-buffer download */
    HOP(HDMI_AUDIO_FREQDET, HDMI_AUDIO_FREQDET_AUDIO_FREQ_DETECT, HDMI_AUDIO_FREQDET_AUDIO_FREQ_DETECT),   /* sampling-rate detection on */
    HOP(HDMI_AOCR, 0xFF, 0xFF),                         /* all I2S / SPDIF outputs */
};

static void hidx_ops(const __code struct hidx_op *op, u8 n)
{
    for (; n; n--, op++) {
        if (op->keep)
            hidx_rmw(op->idx, op->keep, op->val);
        else
            hidx_w(op->idx, op->val);
    }
}

void hdmi_audio_start(void) __banked
{
    u8 i;

    hidx_ops(aud_pll_pre, sizeof(aud_pll_pre) / sizeof(aud_pll_pre[0]));
    for (i = 0; i < sizeof(aud_pll_cal); i++) {
        delay_ms(AUD_PLL_STEP_MS);
        hidx_wrf(HDMI_DPCR0, aud_pll_cal[i], aud_pll_cal[i]);
    }
    hidx_ops(aud_init, sizeof(aud_init) / sizeof(aud_init[0]));
}

u8 hdmi_audio_rate(void) __banked
{
    return hidx_r(HDMI_AUDIO_FREQ_RESULT) & HDMI_AUDIO_FREQ_RESULT_AFREQ_MEAS_RANGE;   /* 0 none, 3 = 48 kHz, ... */
}
